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

// add lab8-2
#define NBUC 13
#define NBUCKET 13

// delete lab8-2
/*
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;

} bcache;
*/

// add lab8-2
struct bbucket {
    struct spinlock lock;      // 保护本 hash bucket 的小锁
    struct buf *bentry[NBUF];  // 用数组存储指向 bcache.buf 数组中相应对象的指针
    uint entry_num;            // 桶中的缓存块数量
};
struct {
    struct spinlock lock;              // 保护 bcache 的大锁
    struct buf buf[NBUF];              // 用数组存储所有缓存块
    struct bbucket hashbkt[NBUCKET];   // 哈希桶
    uint glob_tstamp;                  // 时间戳，LRU 算法选择 eviction block 时使用
} bcache;
// 哈希函数，传入 blockno，返回对应的哈希桶 bbucket 指针
struct bbucket*
bhash(uint blockno) {
    return &bcache.hashbkt[blockno % NBUCKET];
}


// delete lab8-2
/*
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}
*/
// add lab8-2
void
binit(void) {
    struct buf* b;

    // 初始化 bcache
    initlock(&bcache.lock, "bcache");
    bcache.glob_tstamp = 0;
    
    // 初始化所有 bbucket
    struct bbucket* bkt;
    for (bkt = bcache.hashbkt; bkt < bcache.hashbkt + NBUCKET; bkt++) {
        initlock(&bkt->lock, "bbucket");
        bkt->entry_num = 0;
    }

    // 初始化 bcache.buf 数组（即缓存池）中的所有缓存块
    for (int i = 0; i < NBUF; i++) {
        b = &bcache.buf[i];
        initsleeplock(&b->lock, "buffer");
        // bcache.buf[i] 设为 blockno 为 i 的块。
        b->blockno = i;
        b->refcnt = 0;
        b->tstamp = 0;
        // 注意 valid 一定要设为 0
        //表示缓存块中的数据是无效的，bread 到这个块时需要从磁盘重新读取数据
        b->valid = 0;
        // 向对应的哈希桶中分发这个块
        bkt = bhash(b->blockno);
        b->bktord = bkt->entry_num;
        bkt->bentry[bkt->entry_num++] = b;
    }
}


// delete lab8-2
/*
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;


  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  panic("bget: no buffers");
  
}
*/
// add lab8-2
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno) {

    // Is the block already cached?
    struct bbucket* bkt = bhash(blockno);
    acquire(&bkt->lock);
    for (int i = 0; i < bkt->entry_num; i++) {
        struct buf* b = bkt->bentry[i];
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bkt->lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
    // Release bkt->lock at first, preventing dead locks
    release(&bkt->lock);
    acquire(&bcache.lock);
    acquire(&bkt->lock);

    // Recheck for existance of the block
    for (int i = 0; i < bkt->entry_num; i++) {
        struct buf* b = bkt->bentry[i];
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.lock);
            release(&bkt->lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // Loop for the eviction
    while (1) {
        struct buf* evict_buf = 0;
        uint min_tstamp = 0xffffffff;
        for (struct buf* b = bcache.buf; b < bcache.buf + NBUF; b++) {
            if (b->refcnt == 0 && b->tstamp < min_tstamp) {
                min_tstamp = b->tstamp;
                evict_buf = b;
            }
        }
        if (!evict_buf) {
            panic("bget: no buffers");
        }
        struct bbucket* evict_bkt = bhash(evict_buf->blockno);
        if (evict_bkt != bkt) {
            acquire(&evict_bkt->lock);
        }
        // Recheck refcnt after obtaining the lock
        if (evict_buf->refcnt != 0) {
            if (evict_bkt != bkt) {
                release(&evict_bkt->lock);
            }
            // refcnt is not 0, reseek for an available buffer block
            continue;
        }
        // This block can be evicted
        uint num = evict_bkt->entry_num;
        if (evict_buf->bktord < num - 1) {
            evict_bkt->bentry[evict_buf->bktord] = evict_bkt->bentry[num - 1];
            evict_bkt->bentry[evict_buf->bktord]->bktord = evict_buf->bktord;
        }
        evict_bkt->entry_num--;
        if (evict_bkt != bkt) {
            release(&evict_bkt->lock);
        }

        evict_buf->bktord = bkt->entry_num;
        bkt->bentry[bkt->entry_num++] = evict_buf;
        evict_buf->dev = dev;
        evict_buf->blockno = blockno;
        evict_buf->valid = 0;
        evict_buf->refcnt = 1;
        acquiresleep(&evict_buf->lock);
        release(&bcache.lock);
        release(&bkt->lock);
        return evict_buf;
    }
}


//delete lab8-2
/*
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

  
  acquire(&bcache.lock);
  b->refcnt--;
  
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
  

}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
 
}
*/


// add lab8-2

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno) {
    struct buf* b;
    b = bget(dev, blockno);
    b->tstamp = ++bcache.glob_tstamp;
    if (!b->valid) {
        // 如果 valid 为 0，需要从磁盘读
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf* b) {
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    b->tstamp = ++bcache.glob_tstamp;
    virtio_disk_rw(b, 1);
}

void
brelse(struct buf* b) {
    if (!holdingsleep(&b->lock))
        panic("brelse");

    struct bbucket* bkt = bhash(b->blockno);
    releasesleep(&b->lock);

    acquire(&bkt->lock);
    b->refcnt--;
    if (b->refcnt == 0) {
        // no one is waiting for it.
        b->tstamp = 0;
    }

    release(&bkt->lock);
}

void
bpin(struct buf* b) {
    struct bbucket* bkt = bhash(b->blockno);
    acquire(&bkt->lock);
    b->refcnt++;
    b->tstamp = ++bcache.glob_tstamp;
    release(&bkt->lock);
}

void
bunpin(struct buf* b) {
    struct bbucket* bkt = bhash(b->blockno);
    acquire(&bkt->lock);
    b->refcnt--;
    release(&bkt->lock);
}