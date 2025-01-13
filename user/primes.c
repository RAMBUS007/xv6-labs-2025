#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define READ  0
#define WRITE 1
#define stdin 0
#define stdout 1
#define stderr 2

void sieveProc(int left[2]) {
    close(left[WRITE]);

    int prime, temp, pid, right[2];

    // if doesn't read a single number from left, then exit
    if( read(left[READ], &prime, sizeof(int)) == 0 ) {
        close(left[READ]);
        exit();
    }

    printf("prime %d\n", prime);
    pipe(right);
    if( (pid = fork()) < 0 ) {
        fprintf(stderr, "fork error...\n");
        close(right[READ]);
        close(right[WRITE]);
        close(left[READ]);
        exit();
    } else if( pid > 0 ) {
        close(right[READ]);
        while(read(left[READ], &temp, sizeof(int))) {
            if( temp % prime == 0 ) continue;
            write(right[WRITE], &temp, sizeof(int));
        } 
        close(right[WRITE]);
        wait();
        exit();
    } else {
        sieveProc(right);
        exit();
    }
}

int main(int argc, char *argv[]) {
    int pid, p[2];

    pipe(p);
    if((pid = fork()) < 0) {
        fprintf(stderr, "fork error...\n");
        exit();
    } else if (pid > 0) {
        close(p[READ]);
        for(int i = 2; i <= 35; ++i) {
            write(p[WRITE], &i, sizeof(int));
        }
        close(p[WRITE]);
        wait();
        exit();
    } else {
        sieveProc(p);
        exit();
    }
}