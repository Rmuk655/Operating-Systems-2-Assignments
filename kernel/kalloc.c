// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "vmstruct.h"
#include "proc.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
struct frame frametable[MAX_FRAMES];
struct spinlock framelock;
int clock_hand = 0;    // for clock algorithm
int active_frames = 0; // number of frames currently allocated to processes

struct swap swaptable[MAX_SWAP];  // 1D array for metadata
char swap_data[MAX_SWAP][PGSIZE]; // 2D array for the 4KB pages
struct spinlock swaplock;

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&framelock, "framelock");
  initlock(&swaplock, "swaplock");
  for (int i = 0; i < MAX_FRAMES; i++)
  {
    frametable[i].used = 0;
    frametable[i].proc = 0;
    frametable[i].va = 0;
    frametable[i].pa = 0;
  }
  for (int i = 0; i < MAX_SWAP; i++)
  {
    swaptable[i].used = 0;
    swaptable[i].page_table = 0;
    swaptable[i].va = 0;
    // swaptable[i].frame_idx = -1;
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
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
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);
  if (!r)
  {
    return (void *)clock_evict();
  }

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

uint64 clock_evict()
{
  // evict a page using clock algorithm and return the index of the evicted frame
  acquire(&framelock);
  int best_idx = -1;
  int best_level = 0;
  int loops = 0;

  while (loops < MAX_FRAMES * 2)
  {
    struct proc *p = frametable[clock_hand].proc;
    uint64 va = frametable[clock_hand].va;
    if (p != 0)
    {
      pte_t *pte = walk(p->pagetable, va, 0);
      if (pte != 0 && ((*pte) & PTE_V))
      {
        if (((*pte) & PTE_A) == 0)
        {
          if (best_idx == -1 || best_level < p->cur_queue_lvl)
          {
            best_idx = clock_hand;
            best_level = p->cur_queue_lvl;
          }
        }
        else
        {
          *pte &= ~PTE_A; // clear reference bit to give a second chance
        }
      }
    }
    clock_hand = (clock_hand + 1) % MAX_FRAMES;
    loops++;
  }

  if (best_idx == -1)
  {
    for (int i = 0; i < MAX_FRAMES; i++)
    {
      if (frametable[i].proc != 0)
      {
        best_idx = i;
        break;
      }
    }
  }

  clock_hand = (best_idx + 1) % MAX_FRAMES;

  // release(&framelock);
  swapout(frametable[best_idx].proc->pagetable, frametable[best_idx].va);
  // acquire(&framelock);
  frametable[best_idx].proc->pages_evicted++;
  frametable[best_idx].proc->pages_swapped_out++;
  frametable[best_idx].proc->resident_pages--; // Decrement resident page count for the owning process

  frametable[best_idx].used = 0;
  frametable[best_idx].proc = 0;
  release(&framelock);

  return frametable[best_idx].pa;
}

void frametable_alloc(uint64 pa, struct proc *p, uint64 va)
{
  acquire(&framelock);
  // allocate a frame and update the frame table
  if (pa == 0)
  {
    printf("Error: kalloc returned 0 in frametable_alloc\n");
    release(&framelock);
    return;
  }
  int allocate = 0;
  for (int i = 0; i < MAX_FRAMES; i++)
  {
    if (!frametable[i].used)
    {
      // release(&framelock);
      // swapout(p->pagetable, va); // swap out the page currently in this frame if necessary
      // acquire(&framelock);
      frametable[i].used = 1;
      frametable[i].proc = p;
      frametable[i].va = va;
      frametable[i].pa = pa;
      p->resident_pages++; // Increment the process's resident page counter
      // frametable[i].ref_bit = 1;
      allocate = 1;
      release(&framelock);
      break;
    }
  }
  if (!allocate)
  {
    release(&framelock);
    panic("frametable_alloc: no free frame available");
  }

  // if (allocate == 0)
  // {
  //   release(&framelock);
  //   uint64 pa2 = clock_evict();
  //   frametable_alloc(pa2, p, va); // Try to allocate again after eviction
  //   // int freed_idx = clock_evict();
  //   // frametable[freed_idx].used = 1;
  //   // frametable[freed_idx].proc = p;
  //   // frametable[freed_idx].va = va;
  //   // frametable[freed_idx].pa = pa;
  // }
  // release(&framelock);
}

void frametablefree(uint64 pa)
{
  // free the frame table entry for the freed frame
  acquire(&framelock);
  for (int i = 0; i < MAX_FRAMES; i++)
  {
    if (frametable[i].pa == pa && frametable[i].used)
    {
      struct proc *p = frametable[i].proc;
      p->resident_pages--; // Decrement the process's resident page counter
      frametable[i].used = 0;
      frametable[i].proc = 0;
      frametable[i].va = 0;
      frametable[i].pa = 0;
      if(p){
        p->resident_pages--; // Decrement the process's resident page counter for the owning process
      }
      active_frames--;
      break;
    }
  }
  release(&framelock);
}

int swapout(pagetable_t pt, uint64 va)
{
  // 1. Find the physical address from the page table
  pte_t *pte = walk(pt, va, 0);
  if (pte == 0 || (*pte & PTE_V) == 0)
  {
    return -1; // Page not mapped, nothing to swap out
    // panic("swapout: invalid page table entry");
  }

  uint64 pa = PTE2PA(*pte); // Extract physical address from PTE

  int swap_idx = -1;
  acquire(&swaplock);

  // 2. Find a free slot in your swap table
  for (int i = 0; i < MAX_SWAP; i++)
  {
    if (swaptable[i].used == 0)
    {
      swaptable[i].used = 1;
      swaptable[i].page_table = pt;
      swaptable[i].va = va;
      // swaptable[i].frame_idx = ; // (Optional: if you still need this)
      swap_idx = i;
      break;
    }
  }

  if (swap_idx == -1)
  {
    release(&swaplock);
    panic("swapout: out of swap space");
  }

  // 3. Copy the 4096 bytes of memory into your 2D data array
  memmove((void *)swap_data[swap_idx], (void *)pa, PGSIZE);
  release(&swaplock);

  uint flags = PTE_FLAGS(*pte);
  *pte = (swap_idx << 10) | PTE_S | (flags & ~PTE_V);
  // *pte = (*pte & ~PTE_V) | PTE_S; // Mark the page as swapped out and not valid
  sfence_vma(); // Flush TLB to ensure the change takes effect
  return swap_idx;
}

// Completely replace your old swapin with this simpler one
void swapin(uint64 pa, int swap_idx)
{
  acquire(&swaplock);
  if (swap_idx < 0 || swap_idx >= MAX_SWAP || swaptable[swap_idx].used == 0)
  {
    release(&swaplock);
    panic("swapin: invalid swap index or slot not in use");
  }

  // Restore the data
  memmove((void *)pa, (void *)swap_data[swap_idx], PGSIZE);
  release(&swaplock);

  // Free the swap slot
  swap_free(swap_idx);
}

// Add this function so uvmunmap can use it!
void swap_free(int swap_idx)
{
  acquire(&swaplock);
  if (swap_idx >= 0 && swap_idx < MAX_SWAP)
  {
    swaptable[swap_idx].used = 0;
    swaptable[swap_idx].page_table = 0;
    swaptable[swap_idx].va = 0;
    // No need to memset the data, it will be overwritten next time
  }
  release(&swaplock);
}

// void swapin(pagetable_t pt, uint64 va)
// {
//   // 1. Find the NEW physical address that your trap handler just mapped
//   pte_t *pte = walk(pt, va, 0);
//   if (pte == 0 && (*pte & PTE_V) == 0)
//   {
//     // return; // Page not mapped, cannot swap in
//     panic("swapin: invalid page table entry");
//   }

//   uint64 pa = PTE2PA(*pte); // Extract the new physical address

//   int swap_idx = -1;
//   acquire(&swaplock);

//   // 2. Search your swap table for the slot belonging to this pt and va
//   for (int i = 0; i < MAX_SWAP; i++)
//   {
//     if (swaptable[i].used == 1 && swaptable[i].page_table == pt && swaptable[i].va == va)
//     {
//       swap_idx = i;
//       break;
//     }
//   }

//   if (swap_idx == -1)
//   {
//     release(&swaplock);
//     panic("swapin: page not found in swap space");
//   }

//   // 3. Restore the 4096 bytes of data from your 2D array back into physical memory
//   memmove((void *)pa, (void *)swap_data[swap_idx], PGSIZE);

//   *pte |= PTE_V;  // Mark the page as valid again
//   *pte &= ~PTE_S; // Clear the swapped out bit

//   // 4. Free the swap slot so it can be used again later
//   swaptable[swap_idx].used = 0;
//   swaptable[swap_idx].page_table = 0;
//   swaptable[swap_idx].va = 0;
//   memset((void *)swap_data[swap_idx], 0, PGSIZE); // Clear the swap data for cleanliness

//   release(&swaplock);
// }

// void clearswaptable(uint64 va)
// {
//   // 1. Safety check: if the process has no pagetable, there's nothing to clear
//   if (va == 0)
//     return;

//   acquire(&swaplock);

//   // 2. Scan the entire fixed-size swap table
//   for (int i = 0; i < MAX_SWAP; i++)
//   {

//     // 3. Check if this slot is in use and belongs to the process's pagetable
//     if (swaptable[i].used == 1 && swaptable[i].va == va)
//     {

//       // 4. Clear the metadata to mark the slot as free
//       swaptable[i].used = 0;
//       swaptable[i].page_table = 0;
//       swaptable[i].va = 0;
//       memset((void *)swap_data[i], 0, PGSIZE); // Clear the swap data for cleanliness

//       // Note: We don't need to zero out swap_data[i][PGSIZE].
//       // The next swapout() call will simply overwrite it.
//     }
//   }

//   release(&swaplock);
// }