#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"

int 
main(int argc, char* argv[]) {
    // 创建管道
    int p[2];
    pipe(p);
    char buf[1];

    int fp = fork();
    if (fp == 0) {
        if (read(p[0], buf, 1) != 1) {
            fprintf(2, "read failed\n");
            exit(1);
        }
        close(p[0]);

        // 接收到信号后就写
        printf("%d: received ping\n", getpid());

        // 发信号
        if (write(p[1], "a", 1) != 1) {
            fprintf(2, "write failed\n");
            exit(1);
        } 
        close(p[1]);
        exit(0);
    } else {
        // 父进程

        // 发信号
        if (write(p[1], "a", 1) != 1) {
            fprintf(2, "write failed\n");
            exit(1);
        }
        close(p[1]);
        // 等待子进程结束
        wait(0);
        if (read(p[0], buf, 1) != 1) {
            fprintf(2, "read failed\n");
            exit(1);
        }
        close(p[0]);
        printf("%d: received pong\n", getpid());
        exit(0);
    }
}