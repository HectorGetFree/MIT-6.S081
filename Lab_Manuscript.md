# Lab Manuscript

### tracing

- 需要一个参数，还有一个整数mask——指定哪个系统调用被trace
- 例如：trace(1 << SYS_fork) 其中` SYS_fork`在`kernel/syscall.h`.中
- 需要修改内核从而在系统调用将要返回时打印一行信息：包括进程id，系统调用的名称，还有返回值
- 不需要打印系统调用的参数



- 通过proc结构记住参数
- 从用户空间检索系统调用参数的函数位于 kernel/syscall.c 中
- 可以在sysproc.c中看到使用例子
- 最后在syscall.c中添加sys_trace到数组中



### Attack xv6

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

### Getpid()

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

### Use superpages

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



### COW lab

cow fork()为子进程创建页表，包含从用户内存指向父进程物理页的PTE。它将所有的父子进程中所有的user PTE标记为只读

当任何一个进程尝试写入COW页时，CPU会强制发生页错误，handler监测情况，然后分配新的物理页，将原来的页复制到新页，然后修改相关的PTE来指向新的物理页，这时PTE标记为可写

cow fork()应该在此物理页的所有饮用页表消失时被释放



plan:

1. 修改`uvmcopy()`来将父进程的物理页映射到子进程，然后擦除父子进程中页表PTE的`PTE_W`
2. 修改`usertrap()`来识别页错误。当页错误发生在COW页时，使用`kalloc()`来分配一个新页，将旧的页复制到新的页去，然后添加PTE（设置PTE_W）。原来仅读的PTE应该保持仅读属性并且在父子进程之间共享，尝试写这个页的进程应该被杀死
3. 当最后一个PTE消失的时候确保每一个物理页都被free。好的做法是：*为每一个物理页都维护一个引用数目*。当`kalloc()`时设置引用数目为1。每次fork()都增加引用数目，每次drop时都减少引用数目。当引用数目为0时`kfree()`应该将这个页放回到freelist。将这些引用数目放进一个固定大小的数组，应该解决如何设置索引并确定数组大小
4. 修改copyout（），以便在遇到COW页面时使用与页面故障相同的方案。



- 用RSW位来记录是不是COW页
- 有用的宏和定义在`kernel/riscv.h`中
- 如果COW页错误发生并且没有空闲的内存，这个进程应该被杀死



我自己先进行了尝试，后来卡在了第三步维护计数数组上，之后参考了[ttzytt博客](https://ttzytt.com/2022/07/xv6_lab6_record/)，接着完成

然后ttzytt的思路就是在更新数组时单独封装，加锁处理

### Net driver

参考[KuangjuX](https://github.com/KuangjuX/xv6-riscv-solution/tree/main/net)

`kernel/net.c`和`kernel/net.h` -- 包含了网络协议栈的部分实现，包括用户进程发送UDP数据包的完整代码，但是缺少接收数据并将他们传送到用户空间的代码

NIC

- 任务是完成`e1000_transmit()`和`e1000_recv()` -- 驱动器可以传输和接收数据包



### lock lab

基本思路是为每个CPU维护一个空闲列表，每个列表都有自己的锁

挑战：

- 一个CPU的空闲链表为空，另一个有空闲内存的情况
- 需要 steal