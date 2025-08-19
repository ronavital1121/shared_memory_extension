#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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
sys_map_shared_pages(void)
{
  int pID;
  int cID;
  uint64 memoryPointer;
  int size;

  struct proc* parentProcess;
  struct proc* childProcess;

  // Retrieve arguments from user space
  argint(0, &pID);
  argint(1, &cID);
  argaddr(2, &memoryPointer);
  argint(3, &size);
  if (((childProcess = find_proc(cID)) == 0) || ((parentProcess = find_proc(pID)) == 0)) {
    release(&parentProcess->lock);
    return 0;
  }
  uint64 res = map_shared_pages(parentProcess, childProcess, memoryPointer, size);
  release(&parentProcess->lock);
  return res;
}

// System call to unmap shared pages
uint64
sys_unmap_shared_pages(void)
{
  int pidToUnmap;
  uint64 pointer;
  int size;
  struct proc* p;
  argint(0, &pidToUnmap);
  argaddr(1, &pointer);
  argint(2, &size);
  p = find_proc(pidToUnmap);
  if (p == 0)
    return -1;
  return unmap_shared_pages(p, pointer, size);
}

