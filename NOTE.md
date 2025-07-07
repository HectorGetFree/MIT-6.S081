# NOTE

## Lecture 1 -- overall intro

`int dup(int oldfd);`

- oldfd：要复制的原文件描述符
- 返回值：新的文件描述符（数值最小的未占用描述符），失败返回 -1，并设置 errno

## Lecture 2 -- OS organization and System calls

Topic:

- isolation 隔离

  内核模式 和 用户模式

- system call 系统调用

OS should be defensive  防御性

- 应用不能使OS崩溃
- 应用不能破坏隔离性

在RISC-V中，我们可以使用 `ecall <n>` 来进入内核，n是系统调用的序号索引

- 比如说我们调用fork()，实际上调用的是 `ecall sys_fork` 

- 然后进入内核调用接口

kernel 有时候又叫 TCB ***Trusted Computing Base***

- kernel必须没有bug
- kernel必须将进程视为 malicious 怀有恶意的

