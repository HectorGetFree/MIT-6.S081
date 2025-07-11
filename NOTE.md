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







Getpid()

- 当每个进程被创建时，映射一个仅读的页在`USYSCALL`(一个被定义在`memlayout.h`的虚拟地址)
- 在这个页的开头，存储一个`struct usyscall`（也定义在`memlayout.h`）

- 然后初始化他来储存当前进程的PID
- 在本lab中，`ugetpid()`被提供给用户空间，然后回自动使用USYSCALL映射



- 选择permission bits来使得用户空间仅读取这一页
- 在一个新页的生命周期里有一些事情需要完成
  - 查看`proc.c`中trapframe的处理来得到灵感



`kpgtbl()`系统调用调用`vmprint()`

 需要一个`pagetable_t`的参数，任务是打印pgtbl

- 第一行展示的是`vmprint()`的参数
- 然后每一行是一个PTE
- 包括下一级别的PTE
- 每个PTE展示的是虚拟地址 pte位和物理地址
- 不要打印无效的PTE



- 使用在`riscv.h`最后定义的宏
- 留意`freewalk`
- 用%p打印地址



Use superpages

- RISC-V分页硬件支持2MB还有典型的4096-byte大小的分页，大页也就是superpages
- OS创建superpages：在level-1 PTE中设置PTE_V和PTE_R；设置物理页号指向2MB大小的物理内存



- 任务是修改xv6内核来支持superpages
- 具体表述：
  - 如果调用`sbrk(>=2MB)` 并且新创建的地址范围包括一个或者多个2MB对齐的且大小至少为2MB的区域
  - 内核应该使用superpage

- hints

  - 读`pgtbltest.c`中的`superpg_test`

  - `sys_sbrk`,按照代码路径找到为 sbrk 分配内存的函数 growproc(n) - > uvmalloc -> kalloc()\mappages()

  - 内核应该可以分配和释放2MB大小的内存区域

    修改`kalloc.c`来在物理内存中设置一些2MB大小的内存

    创建`superalloc()`和`superfree()`方法

  - 当具有超级页面的进程fork时必须分配超级页面，并在退出时释放超级页面；您需要修改 `uvmcopy()` 和 `uvmunmap()`



思路来自[博客](https://blog.csdn.net/qq_60409213/article/details/147146553?ops_request_misc=%257B%2522request%255Fid%2522%253A%25226dd9a9b840f35e33f8cca03825d5b5f6%2522%252C%2522scm%2522%253A%252220140713.130102334..%2522%257D&request_id=6dd9a9b840f35e33f8cca03825d5b5f6&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduend~default-2-147146553-null-null.142^v102^pc_search_result_base2&utm_term=xv6%20lab3&spm=1018.2226.3001.4187) 看完博客后自己尝试做了一遍

- 先去看`superpg_test`中的`superpg_test()，`其核心代码是  `supercheck(s)`
  - 然后在`supercheck()`中观察到超级页大小是 **512 * PGSIZE*** 正好是2MB



- 接着我们去看`sbrk`关键调用路径是：growproc(n) - > uvmalloc -> kalloc()\mappages()

  - 其中`kalloc()`的关键是对 空闲内存链表 `freelist`操作

    得到基本思路：**由于巨页的大小跟普通页不一样，所以我们需要额外维护一个巨页空闲链表**

  - 然后管理巨页空闲链表的方式应该与管理普通空闲链表的方式类似 -> **观察普通链表行为**



- 观察到`kinit()`行为：他进行第一次物理页的分配，其范围是：end（kernel结束地址）-  PHYSTOP最高地址
  - 类似，我们要对`kalloc.c`文件中跟空闲链表有关的函数都进行模仿**得到超级链表版本**



- 回到uvmalloc
  - 我们也要有相应的超级页版本
  - 可以看到既然我们改了uvmalloc，那么也要改释放uvmdealloc -- 需要 **提供超级页版本的walk**
  - 然后还有映射 `mappages() `和解映射 `uvmunmap()`
  - 接着就是`uvmcopy()`



## Chapter 4 Traps and System calls

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





`kernelvec` pushes all 32 registers onto the stack,



return address 在fp offset -8处

保存的fp在fp-16处



基本思路是：循环fp直到栈的最高位，打印相应的返回地址



添加 `sigalarm(interval, handler)`系统调用

每隔interval就调用一次handler



## Chapter 5 Interrupts and device drivers

驱动程序在两部分执行：上半部分在进程的内核线程中执行（syscall，比如I/O），下半部分在中断时执行



console driver 接收用户输入的字符，通过UART串行端口硬件连接到RISC-V



用户type一个字符，UART硬件要求RISC-V发出一个中断，这将激活trap handler，handler调用`devintr`，它会去查看`scause`来确定中断来自外部设备，然后它要求硬件单元PLIC告诉它哪个设备被中断了，如果是UART，`devintr`调用`uartintr`



`uartintr`从UART读取任何正在等待的输入字符，并将他们交给`consoleintr`；它并不会等待字符，因为未来的输入会引发中断；它的作用是在`cons.buf`中积累字符，直到一整行到达，他也会对空格和特殊字符进行特殊处理，当换行符被接受，`consoleintr`唤醒正在等待的`consoleread`，后者将字符串复制到user space
