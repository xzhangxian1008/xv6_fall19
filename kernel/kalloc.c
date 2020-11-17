// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
  uint ref;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint alloc;
  uint unalloc;
} kmem;

uint
get_alloc() {
  return kmem.alloc;
}

uint
get_unalloc() {
  return kmem.unalloc;
}

void
add_alloc() {
  kmem.alloc++;
}

void
add_unalloc() {
  kmem.unalloc++;
}

void
add_ref(struct run *r) {
  if (r < (struct run*)end || r > (struct run*)PHYSTOP) {
    return;
  }
  acquire(&kmem.lock);
  r->ref += 1;
  release(&kmem.lock);
}

void
decr_ref(struct run *r) {
  if (r < (struct run*)end || r > (struct run*)PHYSTOP) {
    return;
  }
  acquire(&kmem.lock);
  if (r->ref == 0) {
    panic("decr_ref: r->ref==0\n");
  }
  r->ref -= 1;
  release(&kmem.lock);
}

int
is_ref_zero(struct run *r) {
  if (r < (struct run*)end || r > (struct run*)PHYSTOP) {
    panic("is_ref_zero: invalid va!\n");
  }
  acquire(&kmem.lock);
  int ret = 1;
  if (r->ref <= 0) {
    ret = 0;
  }
  release(&kmem.lock);

  return ret;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.alloc = 0;
  kmem.unalloc = 0;
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

// Free the page of physical memory pointed at by v,
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

  acquire(&kmem.lock);
  if (r == 0) {
    panic("kfree: r==0!\n");
  }
  r->next = kmem.freelist;
  kmem.freelist = r;
  r->ref = 0;
  add_unalloc();
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  // if (!r) {
  //   panic("r == 0!\n");
  // }
  if(r) 
    kmem.freelist = r->next;
  
  if(r) {
    add_alloc();
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  
  release(&kmem.lock);
  
  return (void*)r;
}