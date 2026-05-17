#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "swap_disk.h" 

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
      info.disk_reads = pi->disk_reads;
      info.disk_writes = pi->disk_writes;
      info.avg_disk_latency = pi->avg_disk_latency;
      release(&pi->lock);
      copyout(myproc()->pagetable, uaddr, (char *)&info, sizeof(info));
      return 0;
    }
    release(&pi->lock);
  }
  return -1;
}

uint64 sys_setdisksched()
{
  int policy;
  argint(0, &policy);
  if (policy != DISK_SCHED_FCFS && policy != DISK_SCHED_SSTF)
  {
    return -1;
  }
  return setdisksched(policy);
}

uint64 sys_setraidmode(void)
{
    int mode;
    argint(0, &mode);
    return setraidmode(mode);
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
  // printf("Working 2!\n");
  uint64 addr;
  int t;
  int n;

  // printf("Working 3!\n");
  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  // printf("Working 4!\n");
  if (t == SBRK_EAGER || n < 0)
  {
    // printf("Working 5!\n");
    if (growproc(n) < 0)
    {
      // printf("Working !\n");
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