#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc,char* argv){
    if(argc != 1){
        fprintf(1,"usage: pingpong\n");
        exit(1);
    }

    int p[2];
    pipe(p);

    char ch;

    if(fork()==0){
        if(read(p[0],&ch,1)){
            fprintf(1,"%d: received ping\n",getpid());
        }
        write(p[1],&ch,1);
        exit(0);
    }else{
        // 发送byte
        write(p[1],&ch,1);
        if(read(p[0],&ch,1)){
            fprintf(1,"%d: received pong\n",getpid());
        }
        exit(0);
    }
}