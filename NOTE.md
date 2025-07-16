# NOTE

## Small Details

`int dup(int oldfd);`

- oldfd：要复制的原文件描述符
- 返回值：新的文件描述符（数值最小的未占用描述符），失败返回 -1，并设置 errno

> **一个 pipe 是单向的**！

所谓管道其实是在父子进程之间通信

- p[0]表示读端

- p[1]表示写端

  经验是：关闭用不到的端口--节省资源，同时预防一些潜在的问题



`xargs`

前半部分的指令的输出结果可以从从标准输入中得到

然后每部分参数由换行符分割

每一部分若有多一个参数则会用空格分割



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



## starting xv6

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



## Paging

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

`proc_mapstacks(proc.c:33)`为每个进程分配一个内核栈

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



## Traps and System calls

三种情况：都是trap

- 系统调用 `ecall`指令
- `exception` 异常
- `interrupt` 中断 ：设备需要注意



寄存器

- `stvec`:内核将handler的地址写入此寄存器
- `spec`:保存程序计数器pc的值（因为pc之后会被stvec中的值覆盖），指令`sret`将spec的值复制到pc中完成恢复
- `scause`:保存一个数字来指明trap的原因
- `sscratch`:内核在这里放置一个值，这个值在陷阱处理程序一开始就很有用。
- `sstatus`:sstatus中的SIE位控制设备中断是否启用，如果内核清理了SIE，RISC-V 将推迟设备中断，直到内核设置 SIE。SPP位表明trap来自user模式还是supervisor模式，并控制`sret`返回到哪个模式

整个流程是：

1. 如果trap是设备中断，SIE被清理，以下的步骤都不会执行
2. 通过清理SIE来禁用中断
3. 将pc复制到sepc
4. 在SPP中保存当前机器状态
5. 设置scause
6. 将模式设置为supervisor

7. 将stvec复制给pc
8. 开始在新pc处执行



User mode trap handling

path :

`uservec`->`usertrap`->`usertrapret`->`userret`

由于在trap时不进行pg的切换，且要求stvec在内核pg和用户pg中都有相应的映射来保证handler的正确运行 -- 使用 trampoline页 ，它包含了`uservec`也就是stvec所指向的handler代码，该页以PTE_U被映射到用户页表，可以在supervisor模式下运行。因为跳板页被映射到内核地址空间的相同地址，handler可以在切换到内核页表后继续运行

`uservec`:保存引发中断的用户代码的32个寄存器的值，放入内存以便恢复，但是存入内存需要一些寄存器来保存地址，但是眼下没有可用的通用寄存器，所以使用`sscratch`来搭把手

​	在`uservec`开始的`csrrw`指令交换a0和sscratch的值，现在a0的值被保存，a0可以被程序使用，交换后a0保存着内核先前放入sscratch的值

​	在交换后a0保存着当前进程的trapframe指针，这样就可以保存所有寄存器了，（a0原来的值则在sscratch中）

​	trapframe保存着很多有用的值，供uservec使用，然后更改`satp`到内核页表，然后调用`usertrap`



Usertrap:

​	任务是决定trap的类型，处理它，接着返回

​	它首先更改stvec的值，使得在内核中的trap能够被`kernelvec`处理而不是uservec，它保存sepc的值，因为usertrap可能会调用yield来切换到另一个进程的内核线程，这可能会更改sepc

​	如果trap是一个syscall，则`usertrap`调用syscall来处理；如果是一个设备中断，调用`devintr`;如果是异常，内核会杀死错误进程。

`usertrapret`

​	返回到user space的第一步就是调用usertrapret，它设置控制寄存器来准备向user space的trap，这包括将stvec指向uservec，准备trapframe并且设置sepc到之前保存的user pc，最后，userttrapret调用`userret`，这将切换页表

## Interrupts and device drivers

驱动程序在两部分执行：上半部分在进程的内核线程中执行（syscall，比如I/O），下半部分在中断时执行（handler）



console driver 接收用户输入的字符，通过UART串行端口硬件连接到RISC-V



用户type一个字符，UART硬件要求RISC-V发出一个中断，这将激活trap handler，handler调用`devintr`，它会去查看`scause`来确定中断来自外部设备，然后它要求硬件单元PLIC告诉它哪个设备被中断了，如果是UART，`devintr`调用`uartintr`



`uartintr`从UART读取任何正在等待的输入字符，并将他们交给`consoleintr`；它并不会等待字符，因为未来的输入会引发中断；它的作用是在`cons.buf`中积累字符，直到一整行到达，他也会对空格和特殊字符进行特殊处理，当换行符被接受，`consoleintr`唤醒正在等待的`consoleread`，后者将字符串复制到user space
