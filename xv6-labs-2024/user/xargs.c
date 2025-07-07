#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "param.h"

int read_line(char* new_argv[], int current_arg) {
    char buf[1024];
    int n = 0;
    // 逐字节读取
    while (read(0, buf+n, 1) == 1) {
        if (buf[n] == '\n') {
            break;
        }
        n++;
    }

    if (n == 0) {
        // 遇到空行或者出错
        return 0;
    }

    int offset = 0;
    while (offset < n) {
        current_arg++;
        while (buf[offset] != ' ' && offset < n) {
            continue;
        } 
        while (buf[offset] == ' ' && offset < n) {
            buf[offset] = 0;
        }
        offset++;
    }

    return current_arg;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs command (arg ...)\n");
        exit(1);
    }

    char* cmd = argv[1];

    // 获取新的参数列表
    char* new_argv[MAXARG];
    for (int i = 2; i < argc; i++) {
        new_argv[i - 2] = argv[i];
    }

    // 处理参数列表并执行
    int current_arg = 0;
    while ((current_arg = read_line(new_argv, current_arg)) != 0) {
        new_argv[current_arg] = 0;
        if (fork() == 0) {
            exec(cmd, new_argv);
            exit(0);
        }
        wait(0);
    }
    exit(0);
}