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
sockclose(struct sock *s) // NOTICE I'm not sure the parameter
{
  if (s == 0)
    return;

  struct sock *tmp;
  acquire(&lock);

  acquire(&s->lock);
  struct mbuf *m = mbufq_pophead(&s->rxq);
  while (m) {
    mbuffree(m);
    m = mbufq_pophead(&s->rxq);
  }
  release(&s->lock);

  // pick the socket from sockets list out
  while (1) {
    if (sockets == s) {
      sockets = s->next;
      break;
    }

    tmp = sockets;
    while (tmp != 0 && tmp->next != s)
      tmp = tmp->next;
    
    if (tmp == 0) {
      release(&lock);
      return;
    }

    tmp->next = tmp->next->next;
  }
  release(&lock);
}

// from the test's requirement, we can know that the read function reads only one packet each time
int
sockread(struct sock *s, uint64 addr, int n)
{
  while (1) {
    acquire(&s->lock);
    if (!mbufq_empty(&s->rxq))
      break;

    sleep(s, &s->lock);
    release(&s->lock);
  }
  
  struct mbuf *m;
  uint num;
  uint sum = 0;
  pagetable_t pagetable = myproc()->pagetable;

  m = mbufq_pophead(&s->rxq);
  if (m) {
    num = min(m->len, n);

    if (copyout(pagetable, addr, m->head, num)) {
      printf("sockread: copyout error!\n");
      release(&s->lock);
      return sum;
    }

    sum += num;
    mbuffree(m);
  }

  release(&s->lock);
  return sum;
}

int
sockwrite(struct sock *s, uint64 addr, int n)
{
  uint header_size = sizeof(struct udp) + sizeof(struct ip) + sizeof(struct eth);
  pagetable_t pagetable = myproc()->pagetable;
  struct mbuf *m;
  uint sum = 0;
  uint len;
  
  while (n > 0) {
    m = mbufalloc(header_size);
    len = MBUF_SIZE - header_size;
    len = min(len, n);
    mbufput(m, len);
    if (copyin(pagetable, m->head, addr, len))
      panic("sockwrite: copyin error!");
    net_tx_udp(m, s->raddr, s->lport, s->rport);
    addr += len;
    n -= len;
    sum += len;
  }

  return sum;
}

struct sock*
findsock(uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *s = sockets;
  acquire(&lock);
  while (s) {
    if (s->raddr == raddr && s->lport == lport && s->rport == rport)
      break;
    s = s->next; 
  }
  release(&lock);
  return s;
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
  struct sock *s = findsock(raddr, lport, rport);
  if (s) { // discard the packets without corresponding socket
    acquire(&s->lock);
    mbufq_pushtail(&s->rxq, m);
    release(&s->lock);

    wakeup(s);
  }
}