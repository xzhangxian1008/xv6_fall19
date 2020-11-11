#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_sigalarm(void)
{
  int interval;
  uint64 handler;

  if (argint(0, &interval) < 0)
    return -1;
  if (argaddr(1, &handler) < 0)
    return -1;
  
  if (interval < 0) {
    return -1;
  }

  struct proc *p = myproc();
  p->interval = interval;
  p->handler_func = handler;
  acquire(&tickslock);
  p->last_time = ticks;
  release(&tickslock);

  p->cxt->kernel_satp = p->tf->kernel_satp;
  p->cxt->kernel_sp = p->tf->kernel_sp;
  p->cxt->kernel_trap = p->tf->kernel_trap;
  p->cxt->epc = p->tf->epc;
  p->cxt->kernel_hartid = p->tf->kernel_hartid;
  p->cxt->ra = p->tf->ra;
  p->cxt->sp = p->tf->sp;
  p->cxt->gp = p->tf->gp;
  p->cxt->tp = p->tf->tp;
  p->cxt->t0 = p->tf->t0;
  p->cxt->t1 = p->tf->t1;
  p->cxt->t2 = p->tf->t2;
  p->cxt->s0 = p->tf->s0;
  p->cxt->s1 = p->tf->s1;
  p->cxt->a0 = p->tf->a0;
  p->cxt->a1 = p->tf->a1;
  p->cxt->a2 = p->tf->a2;
  p->cxt->a3 = p->tf->a3;
  p->cxt->a4 = p->tf->a4;
  p->cxt->a5 = p->tf->a5;
  p->cxt->a6 = p->tf->a6;
  p->cxt->a7 = p->tf->a7;
  p->cxt->s2 = p->tf->s2;
  p->cxt->s3 = p->tf->s3;
  p->cxt->s4 = p->tf->s4;
  p->cxt->s5 = p->tf->s5;
  p->cxt->s6 = p->tf->s6;
  p->cxt->s7 = p->tf->s7;
  p->cxt->s8 = p->tf->s8;
  p->cxt->s9 = p->tf->s9;
  p->cxt->s10 = p->tf->s10;
  p->cxt->s11 = p->tf->s11;
  p->cxt->t3 = p->tf->t3;
  p->cxt->t4 = p->tf->t4;
  p->cxt->t5 = p->tf->t5;
  p->cxt->t6 = p->tf->t6;

  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  
  p->tf->kernel_satp = p->cxt->kernel_satp;
  p->tf->kernel_sp = p->cxt->kernel_sp;
  p->tf->kernel_trap = p->cxt->kernel_trap;
  p->tf->epc = p->cxt->epc;
  p->tf->kernel_hartid = p->cxt->kernel_hartid;
  p->tf->ra = p->cxt->ra;
  p->tf->sp = p->cxt->sp;
  p->tf->gp = p->cxt->gp;
  p->tf->tp = p->cxt->tp;
  p->tf->t0 = p->cxt->t0;
  p->tf->t1 = p->cxt->t1;
  p->tf->t2 = p->cxt->t2;
  p->tf->s0 = p->cxt->s0;
  p->tf->s1 = p->cxt->s1;
  p->tf->a0 = p->cxt->a0;
  p->tf->a1 = p->cxt->a1;
  p->tf->a2 = p->cxt->a2;
  p->tf->a3 = p->cxt->a3;
  p->tf->a4 = p->cxt->a4;
  p->tf->a5 = p->cxt->a5;
  p->tf->a6 = p->cxt->a6;
  p->tf->a7 = p->cxt->a7;
  p->tf->s2 = p->cxt->s2;
  p->tf->s3 = p->cxt->s3;
  p->tf->s4 = p->cxt->s4;
  p->tf->s5 = p->cxt->s5;
  p->tf->s6 = p->cxt->s6;
  p->tf->s7 = p->cxt->s7;
  p->tf->s8 = p->cxt->s8;
  p->tf->s9 = p->cxt->s9;
  p->tf->s10 = p->cxt->s10;
  p->tf->s11 = p->cxt->s11;
  p->tf->t3 = p->cxt->t3;
  p->tf->t4 = p->cxt->t4;
  p->tf->t5 = p->cxt->t5;
  p->tf->t6 = p->cxt->t6;

  return 0;
}