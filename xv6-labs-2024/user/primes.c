#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"

__attribute__((noreturn)) void primes(int fd);

void primes(int fd) {
    // 先准备管道
    int p[2];
    pipe(p);

    int prime;
    if (read(fd, &prime, sizeof(int)) != sizeof(int)) {
        // 说明没有数据
        close(fd);
        exit(0);
    }
    
    printf("prime %d\n", prime);

    // 子进程 -- 下一个筛选器
    if (fork() == 0) {
        // 为了节约资源我们先关闭写端
        close(p[1]);
        close(fd); // 关闭旧的读取端，这里不需要
        primes(p[0]); // 从读端获得数据进行下一层筛选
        close(p[0]);
        exit(0);
    } else {
        // 父进程 -- 当前筛选器
        close(p[0]); // 父进程只在这里写入
        int n;

        while (read(fd, &n, sizeof(int)) == sizeof(int)) {
            if (n % prime != 0) {
                write(p[1], &n, sizeof(int));
            }
        }
        close(fd);
        close(p[1]);
        wait(0);
        exit(0);
    }   
}

int 
main(int argc, char* argv[]) {
    // 先准备pipe
    int p[2];
    pipe(p);

    if (fork() == 0) {
        close(p[0]);
        for (int i = 2; i <= 280; i++) {
            write(p[1], &i, sizeof(int));
        }
        close(p[1]);
        exit(0);
    } else {
        close(p[1]);
        primes(p[0]);
        close(p[0]);
        wait(0);
    }
    return 0;
}