#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
// 每次读一个字符，每行生成一个执行command
// 执行命令
int main(int argc,char* argv[]){
    char buf[512],*p;
    char* newargv[argc];
    int i;
    if(argc < 1){
        fprintf(2,"usage: xargs \n");
        exit(1);
    }
    for(i = 0;i < argc-1;i++){
        newargv[i] = argv[i+1];
    }
    newargv[argc-1] = buf;
    p = buf;
    while(read(0,p,1) == 1){
        if(*p == '\n'){
            *p = 0;
            if(fork()==0){
                exec(newargv[0],newargv);
                exit(0);
            }else{
                wait(0);
            }
            p = buf;
        }else{
            p++;
        }
    }
    exit(0);
}