#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

extern struct proc proc[NPROC];
extern struct spinlock wait_lock;

int sys_getppid()
{
  if (myproc()->parent)
  {
    acquire(&wait_lock);
    int ans = myproc()->parent->pid;
    release(&wait_lock);
    return ans;
  }
  return -1;
}

int sys_getnumchild()
{
  int ans = 0;
  struct proc *p = myproc();

  for (struct proc *pi = proc; pi < &proc[NPROC]; pi++)
  {
    acquire(&pi->lock);
    if (pi->parent == p && pi->state != ZOMBIE)
    {
      ans++;
    }
    release(&pi->lock);
  }

  return ans;
}

int sys_getsyscount()
{
  acquire(&wait_lock);
  int ans = myproc()->syscount;
  release(&wait_lock);
  return ans;
}

int sys_getchildsyscount()
{
  int arg, ans = -1;
  argint(0, &arg);

  struct proc *p = myproc();

  for (struct proc *pi = proc; pi < &proc[NPROC]; pi++)
  {
    acquire(&pi->lock);
    if (pi->parent == p)
    {
      ans = pi->syscount;
      release(&pi->lock);
      break;
    }
    release(&pi->lock);
  }

  return ans;
}

uint64 sys_getlevel()
{
  acquire(&wait_lock);
  int ans = myproc()->cur_queue_lvl;
  release(&wait_lock);
  return ans;
}

uint64 sys_getmlfqinfo()
{
  int pid;
  uint64 uaddr;

  argint(0, &pid);
  argaddr(1, &uaddr);

  struct mlfqinfo info;

  for (struct proc *pi = proc; pi < &proc[NPROC]; pi++)
  {
    acquire(&pi->lock);
    if (pi->pid == pid)
    {
      info.level = pi->cur_queue_lvl;
      for (int i = 0; i < 4; i++)
      {
        info.ticks[i] = pi->ticks[i];
      }
      info.times_scheduled = pi->no_of_schedules;
      info.total_syscalls = pi->syscount;
      release(&pi->lock);
      copyout(myproc()->pagetable, uaddr, (char *)&info, sizeof(info));
      return 0;
    }
    release(&pi->lock);
  }
  return -1;
}

uint64 sys_getvmstats()
{
  int pid;
  uint64 uaddr;

  argint(0, &pid);
  argaddr(1, &uaddr);

  struct vmstats info;

  for (struct proc *pi = proc; pi < &proc[NPROC]; pi++)
  {
    acquire(&pi->lock);
    if (pi->pid == pid)
    {
      info.page_faults = pi->page_faults;
      info.pages_evicted = pi->pages_evicted;
      info.pages_swapped_in = pi->pages_swapped_in;
      info.pages_swapped_out = pi->pages_swapped_out;
      info.resident_pages = pi->resident_pages;
      release(&pi->lock);
      copyout(myproc()->pagetable, uaddr, (char *)&info, sizeof(info));
      return 0;
    }
    release(&pi->lock);
  }
  return -1;
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0)
  {
    if (growproc(n) < 0)
    {
      return -1;
    }
  }
  else
  {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
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

  argint(0, &pid);
  return kkill(pid);
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
sys_hello(void)
{
  printf("Hello from the kernel!\n");
  return 0;
}

uint64
sys_getpid2(void)
{
  return myproc()->pid;
}