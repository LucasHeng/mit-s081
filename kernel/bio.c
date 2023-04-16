// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  // struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf bufhash[NBUFHASH_BUCKET];
  struct spinlock bufhash_locks[NBUFHASH_BUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i=0;i<NBUFHASH_BUCKET;i++)
    initlock(&bcache.bufhash_locks[i], "bcache");

  // Create linked list of buffers
  for (int i=0;i<NBUFHASH_BUCKET;i++)
  {
    bcache.bufhash[i].prev = &bcache.bufhash[i];
    bcache.bufhash[i].next = &bcache.bufhash[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bufhash[0].next;
    b->prev = &bcache.bufhash[0];
    initsleeplock(&b->lock, "buffer");
    bcache.bufhash[0].next->prev = b;
    bcache.bufhash[0].next = b;
  }
}

int
hash(uint blockno)
{
  return blockno % NBUFHASH_BUCKET;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{

  struct buf *b;

  int idx = hash(blockno);

  acquire(&bcache.bufhash_locks[idx]);

  // Is the block already cached?
  for(b = bcache.bufhash[idx].next; b != &bcache.bufhash[idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufhash_locks[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.bufhash[idx].prev; b != &bcache.bufhash[idx]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.bufhash_locks[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for(int j=(idx+1)%NBUFHASH_BUCKET;j != idx; j = (j+1)%NBUFHASH_BUCKET)
  {
    acquire(&bcache.bufhash_locks[j]);
    for(b = bcache.bufhash[j].prev;b != &bcache.bufhash[j];b = b->prev){
      if(b->refcnt == 0)
      {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.bufhash_locks[j]);
        b->next = bcache.bufhash[idx].next;
        b->prev = &bcache.bufhash[idx];
        bcache.bufhash[idx].next->prev = b;
        bcache.bufhash[idx].next = b;
        release(&bcache.bufhash_locks[idx]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.bufhash_locks[j]);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int idx = hash(b->blockno);
  acquire(&bcache.bufhash_locks[idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bufhash[idx].next;
    b->prev = &bcache.bufhash[idx];
    bcache.bufhash[idx].next->prev = b;
    bcache.bufhash[idx].next = b;
  }
  
  release(&bcache.bufhash_locks[idx]);
}

void
bpin(struct buf *b) {
  int idx = hash(b->blockno);
  acquire(&bcache.bufhash_locks[idx]);
  b->refcnt++;
  release(&bcache.bufhash_locks[idx]);
}

void
bunpin(struct buf *b) {
  int idx = hash(b->blockno);
  acquire(&bcache.bufhash_locks[idx]);
  b->refcnt--;
  release(&bcache.bufhash_locks[idx]);
}


