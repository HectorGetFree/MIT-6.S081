#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

int read_line(char* buf, int buf_size, char* new_argv[MAXARG], int current_arg) {
    int n = 0;
    // 逐字节读取
    while (read(0, buf+n, 1) == 1) {
        if (n == buf_size - 1) {
            fprintf(2, "argument is too long\n");
            exit(1);
        }
        if (buf[n] == '\n') {
            break;
        }
        n++;
    }

    // 哪怕buf是在main函数中定义的而且从来没有清空过
    // 但是我们每次读取都会覆盖原来的内容
    // 而且会将新读取内容后加上终止符
    // 也就等效于清空再写入了，不会对结果造成影响
    buf[n] = 0;
    if (n == 0) {
        // 遇到空行或者出错
        return 0;
    }

    int offset = 0;
    while (offset < n) {
        new_argv[current_arg++] = buf + offset; // 记住每个参数在buf的起始位置，回到main中直接用\0分割（不会接着往后读）
        while (buf[offset] != ' ' && offset < n) {
            offset++;
        } 
        while (buf[offset] == ' ' && offset < n) {
            buf[offset++] = 0;
        }
    }
    return current_arg;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs command (arg ...)\n");
        exit(1);
    }

    // 深拷贝 不要 浅拷贝
    char* cmd = malloc(strlen(argv[1]) + 1);
    strcpy(cmd, argv[1]);

    // 获取新的参数列表 -- 先复制本来就带着的参数
    char* new_argv[MAXARG];
    for (int i = 1; i < argc; i++) {
        new_argv[i - 1] = malloc(strlen(argv[i]) + 1);
        strcpy(new_argv[i - 1], argv[i]);
    }

    // 再得到从标准输入得到的参数
    int current_arg = 0;
    char buf[1024];
    while ((current_arg = read_line(buf, 1024, new_argv, argc - 1)) != 0) {
        new_argv[current_arg] = 0;
        if (fork() == 0) {
            exec(cmd, new_argv); // 这里的第二个参数要求格式为“命令 + 参数”
            fprintf(2, "exec failed\n");
			exit(1);
        }
        wait(0);
    }
    exit(0);
}