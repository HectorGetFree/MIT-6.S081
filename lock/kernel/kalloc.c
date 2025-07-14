// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define STEAL_CNT 8 // 这里是我自己编的magic number

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock, stlk;
  struct run *freelist;
  uint64 st_ret[STEAL_CNT];
} kmems[NCPU];

static uint name_sz = sizeof("kmem cpu 0");
char kmem_lk_n[NCPU][sizeof("keme cpu 0")];

int 
steal(int cpu)
{
  uint st_left = STEAL_CNT;
  int idx = 0; // 记录偷到了几个

  memset(kmems[cpu].st_ret, 0, sizeof(kmems[cpu].st_ret)); // 填充
  for (int i = 0; i < NCPU; i++) {
    if (i == cpu) continue; // 跳过当前CPU

    acquire(&kmems[i].lock); // 要偷空间必须要得到另一个CPU的锁

    while (kmems[i].freelist && st_left) {// 如果当前的cpu i的空闲链表不为空并且还能接着偷
      kmems[cpu].st_ret[idx++] = (uint64)kmems[i].freelist;
      kmems[i].freelist = kmems[i].freelist->next; // 更新被偷的空闲链表
      st_left--;
    }

    release(&kmems[i].lock);
    if (st_left == 0) break; // 最多偷STEAL_CNT个
  }

  return idx;
}





void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    snprintf(kmem_lk_n[i], name_sz, "kmem cpu %d", i); // 格式化写入字符串命名数组
    initlock(&kmems[i].lock, kmem_lk_n[i]);
  }
  freerange(end, (void*)PHYSTOP);  
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 获取cpu编号
  push_off();
  int id = cpuid();
  pop_off();

  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r = 0;

  // 获取cpu编号
  push_off();
  int cpu = cpuid();

  acquire(&kmems[cpu].lock);
  r = kmems[cpu].freelist;
  if(r) { // 如果本cpu的空闲列表不是空
    kmems[cpu].freelist = r->next; // 更新空闲列表
    release(&kmems[cpu].lock); // 释放锁
  } else { // 否则就去偷
    release(&kmems[cpu].lock); 
    int ret = steal(cpu); // 我们在偷的时候让cpu不允许中断，这样可以偷页过程中处理别的进程造成重复偷页
    if (ret <= 0) { // 没偷成
      pop_off(); // 恢复中断
      return 0; // 返回
    }
    acquire(&kmems[cpu].lock); // 重新获取本cpu的锁
    for (int i = 0; i < ret; i++) { // 将偷来的页加到本CPU的空闲链表前面
      if (!kmems[cpu].st_ret[i]) break;
      ((struct run*)kmems[cpu].st_ret[i])->next = kmems[cpu].freelist; 
      kmems[cpu].freelist = (struct run*)kmems[cpu].st_ret[i];
    }

    r = kmems[cpu].freelist;
    kmems[cpu].freelist = r->next;
    release(&kmems[cpu].lock);
  }
  
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}
