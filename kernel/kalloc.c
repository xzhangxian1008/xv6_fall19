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

char lock_names[NCPU][10];

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

static struct kmem mems[NCPU];

// struct {
//   struct spinlock lock;
//   struct run *freelist;
// } kmem;

// void
// kinit()
// {
//   initlock(&kmem.lock, "kmem");
//   freerange(end, (void*)PHYSTOP);
// }

void
kinit()
{
  char prefix[6] = "kmem_";
  for (int i = 0; i < NCPU; i++) {
    // stupid way to allocate name
    for (int j = 0; j < 5; j++) {
      lock_names[i][j] = prefix[j];
    }
    lock_names[i][5] = 48 + i;
    lock_names[i][6] = 0;
  }

  for (int i = 0; i < NCPU; i++) {
    initlock(&(mems[i].lock), lock_names[i]);
  }
  // initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

// void
// freerange(void *pa_start, void *pa_end)
// {
//   char *p;
//   p = (char*)PGROUNDUP((uint64)pa_start);
//   for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) 
//     kfree(p);
// }

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  char *size = (char*)(((char*)pa_end - p) / NCPU); // how many memory a cpu can get
  size = (char*)PGROUNDUP((uint64)size);

  for (int i = 0; i < NCPU; i++) {
    char *start = i * (uint64)size + p;
    char *mem = start;
    char *end = (char*)((uint64)start + (uint64)size);
    if (end > (char*)pa_end)
      end = pa_end;
    for (; mem + PGSIZE <= end; mem += PGSIZE) {
      // printf("mem %p\n", mem);
      struct run *r;
      memset(mem, 1, PGSIZE);
      r = (struct run*)mem;
      acquire(&(mems[i].lock));
      r->next = mems[i].freelist;
      mems[i].freelist = r;
      release(&(mems[i].lock));
    }
  }
}

// void
// kfree(void *pa)
// {
//   struct run *r;

//   if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
//     panic("kfree");

//   // Fill with junk to catch dangling refs.
//   memset(pa, 1, PGSIZE);

//   r = (struct run*)pa;

//   acquire(&kmem.lock);
//   r->next = kmem.freelist;
//   kmem.freelist = r;
//   release(&kmem.lock);
// }

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
    pop_off();
    panic("kfree");
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // int cpu = 0;
  int cpu = cpuid();
  acquire(&(mems[cpu].lock));
  r->next = mems[cpu].freelist;
  mems[cpu].freelist = r;
  release(&(mems[cpu].lock));
  pop_off();
}

// cpuid refers to who calls this function
void*
steal_mem(int cpuid) {
  struct run *r;

  for (int i = 0; i < NCPU; i++) {
    if (i == cpuid)
      continue;

    acquire(&(mems[i].lock));
    r = mems[i].freelist;
    if(r)
      mems[i].freelist = r->next;
    release(&(mems[i].lock));

    if (r) {
      memset((char*)r, 5, PGSIZE); // fill with junk
      return r;
    }
  }

  // printf("steal fail\n");
  return 0;
}

// void *
// kalloc(void)
// {
//   struct run *r;

//   acquire(&kmem.lock);
//   r = kmem.freelist;
//   if(r)
//     kmem.freelist = r->next;
//   release(&kmem.lock);

//   if(r)
//     memset((char*)r, 5, PGSIZE); // fill with junk
//   return (void*)r;
// }

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  struct run *r;

  // int cpu = 0;
  int cpu = cpuid();
  acquire(&(mems[cpu].lock));
  r = mems[cpu].freelist;
  if(r)
    mems[cpu].freelist = r->next;
  release(&(mems[cpu].lock));

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  else {
    // steal memory from other cpus
    r = steal_mem(cpu);
  }
  pop_off();

  return (void*)r;
}