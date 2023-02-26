#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


void prime(int p_left[]){
    int mod,n;
    if(read(p_left[0],&mod,4)){
        fprintf(1,"prime %d\n",mod);
        int p_right[2];
        pipe(p_right);

        if(fork()==0){
            close(p_left[0]);
            close(p_right[1]);
            prime(p_right);
        }else{
            close(p_right[0]);
            while(read(p_left[0],&n,4)){
                if(n%mod!=0){
                    write(p_right[1],&n,4);
                }
            }
            close(p_right[1]);
            close(p_left[0]);
            wait(0);
        }
    }
}

int main(){
    int p[2],i;
    pipe(p);

    if(fork()==0){
        close(p[1]);
        prime(p);
    }else{
        close(p[0]);
        for(i = 2;i < 36;i++){
            write(p[1],&i,4);
        }
        close(p[1]);
        wait(0);
    }
    exit(0);
}