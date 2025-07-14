// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCK_SIZ 13 // 散列表桶的数目 -- 素数更不易冲突
#define BCACHE_HASH(dev, blk) (((dev << 27) | blk) % BUCK_SIZ) // 散列表映射

struct {
  struct spinlock bhash_lk[BUCK_SIZ]; // 散列表的锁 -- 一个桶一个锁
  struct buf bhash_head[BUCK_SIZ]; // 每个桶的开头，不用buf*是因为我们需要得到某个buf前面的buf
                                   // 用指针不方便 

  struct buf buf[NBUF]; // 最终的缓存
} bcache;

void
binit(void)
{

  for (int i = 0; i < BUCK_SIZ; i++) { // 初始化所有的锁
    initlock(&bcache.bhash_lk[i], "bcache buf hash lock");
    bcache.bhash_head[i].next = 0; // 初始化所有的桶的开始
  }

  // 构建散列表来维护缓存
  for(int i = 0; i < NBUF; i++){
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buf sleep lock");
    b->lst_use = 0;
    b->refcnt = 0; // 引用次数
    b->next = bcache.bhash_head[0].next; // 最开始吧所有的缓存分配都分配到桶0上
    bcache.bhash_head[0].next = b;
  }
}

// bget()的更改逻辑是：我们在获取缓存的时候放弃锁，但是这可能导致缓存重复添加
// 所以要做的就是在添加之前再检查一次有没有被添加过
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int key = BCACHE_HASH(dev, blockno);

  acquire(&bcache.bhash_lk[key]); // 任何时候分配内存都需要竞争这个锁 
                                 // 所以我们在这里优化 

  // 查看是否被对应key的桶缓存，有则返回
  for(b = bcache.bhash_head[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bhash_lk[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.bhash_lk[key]); // 这里释放锁，是为了防止多个进程同时操作一个缓存块
                                  // 带来的死锁问题
  int lru_bkt;
  struct buf* pre_lru = bfind_prelru(&lru_bkt); // 偷空闲的缓存
  // 返回空闲缓存中的前一个（链表的前一个）的地址
  // 并确保拿到了缓存对应的桶锁 -- 为了保证更新缓存的正确性
  // 传入 lru_bkt 来储存对应的桶 
  if (lru_bkt == 0) panic("bget: no buffers");

  struct buf* lru = pre_lru->next; // lru 是最久没有被使用的缓存，位于 pre_lru 的下一个
  pre_lru = lru->next; // 更新 pre_lru, 因为lru将要被使用
  release(&bcache.bhash_lk[lru_bkt]); // 注意 bfind_prelru() 已经拿到了锁,这里要释放
                                  // 因为我们已经更新好了另一个桶的缓存，释放锁来增加并发性能
  
  acquire(&bcache.bhash_lk[key]); 

  for(b = bcache.bhash_head[key].next; b; b = b->next){ // 防止重复添加
      if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bhash_lk[key]);
      acquiresleep(&b->lock);
      return b;
    }
    }
  
  lru->next = bcache.bhash_head[key].next; // 把找到的缓存添加到链表头部
  bcache.bhash_head[key].next = lru;

  lru->dev = dev, lru->blockno = blockno;
  lru->valid = 0, lru->refcnt = 1; 

  release(&bcache.bhash_lk[key]);

  acquiresleep(&lru->lock);
  return lru;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int key = BCACHE_HASH(b->dev, b->blockno);
  acquire(&bcache.bhash_lk[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lst_use = ticks; // 利用时钟来设置时间戳
  }
  
  release(&bcache.bhash_lk[key]);
}

void
bpin(struct buf *b) {
  int key = BCACHE_HASH(b->dev, b->blockno);
  acquire(&bcache.bhash_lk[key]);
  b->refcnt++;
  release(&bcache.bhash_lk[key]);
}

void
bunpin(struct buf *b) {
  int key = BCACHE_HASH(b->dev, b->blockno);
  acquire(&bcache.bhash_lk[key]);
  b->refcnt--;
  release(&bcache.bhash_lk[key]);
}


struct buf*
bfind_prelru(int* lru_bkt) // 返回 lru 前面的一个，并且加锁
{
  *lru_bkt = -1;
  struct buf* lru_res = 0;
  
  for (int i = 0; i < BUCK_SIZ; i++) { // 遍历每一个桶
    acquire(&bcache.bhash_lk[i]); // 获取锁
    int found_new = 0;
    for (struct buf* b = &bcache.bhash_head[i]; b->next; b = b->next) {
      if(b->next->refcnt == 0 && (!lru_res || b->next->lst_use < lru_res->next->lst_use)){
        lru_res = b;
        found_new = 1;
      }
    }
    if (found_new) {
      // 如果有更好的选择
      if(*lru_bkt != -1) release(&bcache.bhash_lk[*lru_bkt]); // 直接释放以前选择的锁
      *lru_bkt = i; // 更新最佳选择
    } else {
      // 没有就一直持有这个锁
      // release(&bcache.bhash_lk[i]); // ？为什么这里还要release
    }
  }
  return lru_res;
}