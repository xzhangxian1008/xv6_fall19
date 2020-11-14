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

#define SLOT_NUM 15

struct {
  // struct spinlock lock;
  struct buf buf[NBUF];

  struct buf* hash_table[SLOT_NUM];
  struct spinlock ht_locks[SLOT_NUM];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;
  struct buf *head;
  int num = 0; // used for allocating the buf

  for (int i = 0; i < SLOT_NUM; i++, num++) {
    initlock(&(bcache.ht_locks[i]), "bcache.slot");
    b = &(bcache.buf[num]);
    bcache.hash_table[i] = b;
    b->prev = b;
    b->next = b;
    b->ht_index = i;
  }

  int ht_index = 0; // evenly seperate the buffers on the hash table with ht_index
  for (; num < NBUF; num++) {
    b = &(bcache.buf[num]);
    head = bcache.hash_table[ht_index];
    b->next = head;
    b->prev = head->prev;
    head->prev->next = b;
    head->prev = b;
    bcache.hash_table[ht_index] = b;
    b->ht_index = ht_index;
    ht_index = (ht_index + 1) % SLOT_NUM;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *head;
  uint ht_index = blockno % SLOT_NUM;
  uint block_ht_index = ht_index;

  acquire(&(bcache.ht_locks[ht_index]));

  head = bcache.hash_table[ht_index];
  b = head;
  do {
    if (b == 0)
      break;

    // Is the block already cached?
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&(bcache.ht_locks[ht_index]));
      acquiresleep(&b->lock);
      return b;
    }
    b = b->next;
  } while (b != head);

  // Not cached; recycle an unused buffer in this slot, then in other slots.
  // b = head; // NOTICE we can comment it
  do {
    if (b == 0)
      break;

    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&(bcache.ht_locks[ht_index]));
      acquiresleep(&b->lock);
      return b;
    }
    b = b->next;
  } while (b != head);

  release(&(bcache.ht_locks[ht_index]));

  // search for a buffer continously
  do {
    ht_index = (ht_index + 1) % SLOT_NUM;
    acquire(&(bcache.ht_locks[ht_index]));

    head = bcache.hash_table[ht_index];
    b = head;

    // search a buffer in this slot
    do {
      if (b == 0)
        break;
      
      // check if it's an empty buffer
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // we should put the buffer into correct slot
        if (ht_index != block_ht_index) {

          // take the buffer away from the original slot
          if (b->next == b) 
            bcache.hash_table[ht_index] = 0; // it's the last buffer in slot
          else {
            b->next->prev = b->prev;
            b->prev->next = b->next;
          }

          // put the buffer into correct slot
          acquire(&(bcache.ht_locks[block_ht_index]));
          struct buf *tmp_head = bcache.hash_table[block_ht_index];
          if (tmp_head == 0)
            bcache.hash_table[block_ht_index] = b; // put the buffer directly, when the slot is empty
          else {
            b->next = tmp_head;
            b->prev = tmp_head->prev;
            tmp_head->prev->next = b;
            tmp_head->prev = b;
          }
          b->ht_index = block_ht_index;
          release(&(bcache.ht_locks[block_ht_index]));
        }

        release(&(bcache.ht_locks[ht_index]));
        acquiresleep(&b->lock);
        return b;
      }
      b = b->next;
    } while (b != head);

    release(&(bcache.ht_locks[ht_index]));
  } while (ht_index != block_ht_index);

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
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
  virtio_disk_rw(b->dev, b, 1);
}

void
lock_buf_slot(struct buf *b)
{
  uint ht_index;
  for (;;) {
    ht_index = b->ht_index;
    acquire(&(bcache.ht_locks[ht_index]));

    // ensure b->ht_index is not modified
    if (ht_index != b->ht_index) {
      release(&(bcache.ht_locks[ht_index]));
      continue;
    }
    break;
  }
}

void
unlock_buf_slot(struct buf *b)
{
  uint ht_index = b->ht_index;
  release(&(bcache.ht_locks[ht_index]));
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  lock_buf_slot(b);
  b->refcnt--;
  unlock_buf_slot(b);
}

void
bpin(struct buf *b) {
  lock_buf_slot(b);
  b->refcnt++;
  unlock_buf_slot(b);
}

void
bunpin(struct buf *b) {
  lock_buf_slot(b);
  b->refcnt--;
  unlock_buf_slot(b);
}


