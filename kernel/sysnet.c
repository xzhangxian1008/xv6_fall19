//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

struct sock {
  struct sock *next; // the next socket in the list
  uint32 raddr;      // the remote IPv4 address
  uint16 lport;      // the local UDP port number
  uint16 rport;      // the remote UDP port number
  struct spinlock lock; // protects the rxq
  struct mbufq rxq;  // a queue of packets waiting to be received
};

static struct spinlock lock;
static struct sock *sockets; // a list of socket

#define min(a, b) ((a) < (b) ? (a) : (b))

void
sockinit(void)
{
  initlock(&lock, "socktbl");
}

int
sockalloc(struct file **f, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *si, *pos;

  si = 0;
  *f = 0;
  if ((*f = filealloc()) == 0)
    goto bad;
  if ((si = (struct sock*)kalloc()) == 0)
    goto bad;

  // initialize objects
  si->raddr = raddr;
  si->lport = lport;
  si->rport = rport;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);
  (*f)->type = FD_SOCK;
  (*f)->readable = 1;
  (*f)->writable = 1;
  (*f)->sock = si;

  // add to list of sockets
  acquire(&lock);
  pos = sockets;
  while (pos) {
    if (pos->raddr == raddr &&
        pos->lport == lport &&
	pos->rport == rport) {
      release(&lock);
      goto bad;
    }
    pos = pos->next;
  }
  si->next = sockets;
  sockets = si;
  release(&lock);
  return 0;

bad:
  if (si)
    kfree((char*)si);
  if (*f)
    fileclose(*f);
  return -1;
}

//
// Your code here.
//
// Add and wire in methods to handle closing, reading,
// and writing for network sockets.
//

void
sockclose(struct sock *s) // NOTIC I'm not sure he parameter
{
  // TODO
}

int
sockread(struct sock *s, uint64 addr, int n)
{
  // TODO
  while (1) {
    acquire(&s->lock);
    if (!mbufq_empty(&s->rxq))
      break;
    release(&s->lock);

    sleep(s, 0);
  }
  
  struct mbuf *m;
  uint num;
  uint sum = 0;
  pagetable_t pagetable = myproc()->pagetable;

  // NOTICE how about the left data if we can't read all of the data in a mbuf?
  // NOTICE we discard them in the current implementation
  while (n > 0) {
    m = mbufq_pophead(&s->rxq);
    num = min(m->len, n);
    if (copyout(pagetable, addr, m->head, num)) {
      printf("sockread: copyout error!\n");
      return sum;
    }

    sum += num;
    addr += num;
    n -= num;
    mbuffree(m);
  }

  return sum;
}

int
sockwrite(struct sock *s, uint64 addr, int n)
{
  // TODO
}

// called by protocol handler layer to deliver UDP packets
void
sockrecvudp(struct mbuf *m, uint32 raddr, uint16 lport, uint16 rport)
{
  //
  // Your code here.
  //
  // Find the socket that handles this mbuf and deliver it, waking
  // any sleeping reader. Free the mbuf if there are no sockets
  // registered to handle it.
  //
  // TODO
  mbuffree(m);
}