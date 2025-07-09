# NOTE

## Lecture 1 -- overall intro

`int dup(int oldfd);`

- oldfd：要复制的原文件描述符
- 返回值：新的文件描述符（数值最小的未占用描述符），失败返回 -1，并设置 errno

> **一个 pipe 是单向的**！

所谓管道其实是在父子进程之间通信

- p[0]表示读端

- p[1]表示写端

  经验是：关闭用不到的端口--节省资源，同时预防一些潜在的问题



xargs

前半部分的指令的输出结果可以从从标准输入中得到

然后每部分参数由换行符分割

每一部分若有多一个参数则会用空格分割

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

#### starting xv6

- 运行一个boot loader（存在只读内存中），它将xv6的内核写入内存

- 在machine模式下，CPU从`_entry (kernel/entry.S:7)`开始执行xv6

  RISC-V启动时分页硬件处于禁用状态：虚拟地址直接映射到物理地址

- loader 将内核加载到物理地址0x80000000处

- `_entry`的代码创建一个可以运行C代码的栈

  xv6 在文件 `start.c  (kernel/start.c:11)`中声明了初始堆栈 stack0 的空间

  `_entry`用地址` stack0+4096`加载栈指针，然后调用在`start(kernel/start.c:21)`处的代码

- `start`在machine模式下执行配置代码，然后切换到supervisor模式

- 为了进入supervisor模式，RISC-V提供了指令`mret`

  此指令最常用于从上一个调用从管理模式返回到机器模式

- `start`并不会因为这条指令返回，他会将切换到supervisor之前的模式存到寄存器`mstatus`中，然后将返回地址设置到`main`的地址（通过将main的地址写入寄存器`mepc`）；通过将0写入页表寄存器`satp`来禁用在supervisor模式下的虚拟地址翻译；将所有中断和异常提升至supervisor模式

- 在进入supervisor之前，start还会使硬件产生时钟中断

  完成所有工作后start 通过调用 mret 返回到管理模式。这会导致程序计数器变为` main (kernel/main.c:11)`

- main初始化一些设备和子系统，创建第一个进程：`userinit (kernel/proc.c:226)`,它执行一个汇编程序，发出第一个系统调用。` initcode.S (user/initcode.S:3) `将调用exec所需的数字`SYS_EXEC(kernel/syscall.h:8)`加载到a7寄存器中，然后调用`ecall`重新进入内核
- 内核利用a7中的数字来调用想要的系统调用，即exec
- 然后执行exec，返回到/init进程，`Init(user/init.c:15)`  会根据需要创建一个新的控制台设备文件，然后将其作为文件描述符 0、1、2 打开。之后，它会在控制台上启动一个 shell。系统启动完毕。



#### tracing

- 需要一个参数，还有一个整数mask——指定哪个系统调用被trace
- 例如：trace(1 << SYS_fork) 其中` SYS_fork`在`kernel/syscall.h`.中
- 需要修改内核从而在系统调用将要返回时打印一行信息：包括进程id，系统调用的名称，还有返回值
- 不需要打印系统调用的参数



- 通过proc结构记住参数
- 从用户空间检索系统调用参数的函数位于 kernel/syscall.c 中
- 可以在sysproc.c中看到使用例子
- 最后在syscall.c中添加sys_trace到数组中

```
make GRADEFLAGS=sleep grade
```

#### Attack xv6

- `kernel/vm.c (line 272)`
- `kalloc.c (line 60 89)`
- 省略这三行使得上一次使用时得到的信息被泄漏给当前的进程



- `user/secret/c`在内存里写入了8字节大小的secret
- 目标是在`attack.c`中添加代码来找到之前执行 secret.c 时写入内存的秘密
- 然后将秘密写入到文件描述符2



以下解析来自：[博客](https://blog.csdn.net/weixin_42543071/article/details/143351746)

查找起来并不简单，因为fork和exec会对内存进行分配和释放

**fork**

- 首先调用`allocproc`为创建子进程做准备 

  - 在其中调用`kalloc()`时会为trapframe **分配一页空间**

  - 然后`proc_pagetable()`分配一个空页表

    由于xv6采用三级页表 -- 会**分配三页**

- 回到fork

  - 将父进程--即attacktest的页表拷贝到secret中，这一步创建了**2个页表页和4个用户内存页**
  - 然后fork结束

- 接下来是`exec`

  - 首先`proc_pagetable()`分配了一个新页表  -- **也就是3页（2 + 1）**
  - 随后将elf中需要load的段装入内存，核心就是这个uvmalloc函数，secret的elf有两个需要装入的段，各占一页
  - 也就是说这一段新分配了**两个页表页+两页用户内存**
  - 随后**分配了两页**，一页作为用户栈，一页作为guard
  - 最后释放旧页表 -- **attacktest中5页页表页 + 4页内存页**

- 然后运行secret
  - 分配了**32页** -- 由于一个页表页可以管512页，因此这时不用分配新的页表页

- secret退出

  - 在attacktest的wait中调用了freeproc释放secret的内存

  - 先释放了trapframe随后进入proc_freepagetable

  - proc_freepagetable调用uvmfree

  - 其先释放用户内存36页，之后释放页表5页

    - ###### 36 = 32 + 4（secret一开始的内存占用4页）

    - ###### 5 = 2 + 3

- 然后是fork+exec调用attack
  - attack和secret一样都只有4页用户内存
  - 因此在执行attack的时候系统分配的页面数应该与secret时相同也就是1(trapframe)+3(proc_pagetable)+6(uvmcopy)+3(proc_pagetable)+4(load)+2(stack & guard) - 9(oldpagetable) = 10页
- 链式栈管理空闲内存 
  - secret写入的是分配的32页中的第十页
  - 前面有22（32 - 10） + 5（pagetable）
  - 然后已经被占用10页了
  - 所以attack只需要分配17页即可
  - 系统把空闲页前4个字节作为链式栈的指针了，所以覆盖掉了秘密的值 -- 页内offset要>=32

## Lecture 4 Paging

xv6运行的是Sv39 RISC-V：也就是64位地址仅低位39位会被用到

- 一张page table 包含 2^27^个条目

- 每个PTE包含一个44位的PPN和一些标识位

- 翻译规则：

  虚拟地址的高27位（在采用的39中）最为索引得到PPN

  然后得到56位的物理地址

- 页表使操作系统能够以 4096 (212) 字节为单位，以对齐的块为粒度控制虚拟地址到物理地址的转换。这样的块称为页。
- Sv39 RISC-V使用三级页表
  - 第一级页表大小是4096-byte，包含512个PTE
  - 第二级页表也包含512个PTE
  - 虚拟地址的高27位中的高9位用作第一级别索引
  - 中间9位用于第二级索引，最后9位用于最后一级的索引
  - flags&page hardware-related structure 定义在 riscv.h中



- 内核在 `stap`寄存器中记录下第一级页表的物理地址
  - 每个CPU都有一个stap



Xv6为每个进程维护一个页表，描述了每个进程的用户地址空间还有一个页表用来描述内核地址空间

- `memlayout.h`描述了xv6的内核地址布局



qemu模拟的计算机，其物理地址开始在 `0x80000000`到 `0x86400000`结束 -- 内核地址直接映射到物理地址

- 但是有些捏合虚拟地址不是直接映射的
  - trampoline
  - 内核栈



xv6维护地址空间和页表的代码基本都在 `vm.c`中

- 关键的结构是 `pagetable_t`

  它是一个指向架构第一级页表的指针

  它要么是内核页表，要么是进程也表

- 关键的方法是 `walk`

  - 它为一个虚拟地址和 `mappages` 找到PTE 以及 

  - mappages 将PTE安装到新的映射

- 以`kvm`开头的方法维护的内核页表

- 以`uvm`开头的方法维护的是用户页表

- 其他方法二者都有维护

- `copyout`和`copyin`



`main`调用了`kvminit(vm.c:54)`来用`kvmmake(vm.c:20)`创建内核页表 -- 直接映射到物理地址

`proc_maostacks(proc.c:33)`为每个进程分配一个内核栈

它调用`kvmmap(vm.c:127)` 来对每个栈上的虚拟地址进行映射，同时为guard留下空间

`kvmmap`调用`mappages(vm.c:138)`

`main`调用`kvminithart(vm.c:62)`来将内核第一级页表地址写入寄存器`satp`



物理地址分配

- xv6每次分配或者释放整个的4096-byte的页表
- 它利用链表跟踪空闲页
- 分配者在`kalloc(kalloc.c:1)`中
- 每个空闲链表的元素都是一个`struct run(kalloc.c:17)`
- 分配器将每个空闲链表的run结构储存在空闲页本身（反正空闲页本身也没有被使用）
- `main`调用`kinit`来初始化分配器`kalloc.c:27`
- `kinit`调用`freerange`，通过为每个页表调用`kfree`来添加内存到空闲链表，`freerange`利用`PGROUNDUP`来确保4096数据对齐



进程地址空间：

- 每个进程都一个页表，可用于不同进程之间的切换
- 一个进程的`user memeory`从虚拟地址0开始可以增长到`MAXVA(riscv.h:360)` -- 256 GB内存
- 当进程要求更多用户内存时，调用`kalloc`来分配物理内存 -- 然后添加PTE到进程页表
- 

## Lecture 5 RISC-V calling convertion

