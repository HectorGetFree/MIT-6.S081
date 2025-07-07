#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"

int 
main(int argc, char* argv[]) {
    // 创建管道
    int p1[2]; // 父 -> 子
    int p2[2]; // 子 -> 父
    char buf[1];

    pipe(p1);
    pipe(p2);

    int fp = fork();
    if (fp == 0) {
        // 子进程
        close(p1[1]);
        close(p2[0]);
        if (read(p1[0], buf, 1) == 1) {
            printf("%d, received ping\n", getpid());
            write(p2[1], "a", 1);
        }
    } else {
        // 父进程
        close(p1[0]);
        close(p2[1]);
        write(p1[1], "a", 1);
        if (read(p2[0], buf, 1) == 1) {
            printf("%d, received pong\n", getpid());
            wait(0);
            exit(0);
        }
    }
}