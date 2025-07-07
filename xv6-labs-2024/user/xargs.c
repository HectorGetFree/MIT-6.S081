#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "param.h"

/*
    要编写 xargs
    主要就是进行指令执行，和字符串的的重新拼接得到新指令
*/

char* xargs(char* cmd1, char* cmd1_args[], char* cmd2) {
    char* buf[1];
    int p[2];
    pipe(p);

    // 先运行cmd1并储存其运行结果
    if (fork() == 0) {
        // 子进程执行cmd1
        close(1); // 关闭标准输出
        dup(p[1]); // 将输出重定向到写端
        exec(cmd1, cmd1_args);
        close(p[0]);
        exit(0);
    } else {
        wait(0);
        // 将结果读取到buf
        read(p[0], buf, 1);
        // 处理buf

        // 拼接字符串

        close(p[0]);
        close(p[1]);
        return cmd2;
    }
}

int main() {
    // 读取指令
    // 分割
    // 传参
    // 运行
    
}