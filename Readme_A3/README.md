# CS3523: Operating Systems II

## Programming Assignment 03

### Scheduler-Aware Page Replacement in xv6

---

## Overview

This project extends the xv6 operating system by implementing **page replacement with swap support** when physical memory is exhausted. Unlike the default xv6 behavior (which terminates processes on memory exhaustion), this implementation enables:

- Page eviction using the **Clock (Second-Chance) algorithm**
- Swapping pages to a **kernel-managed swap space**
- Restoring pages on demand
- Integration with a **scheduler-aware policy (SC-MLFQ)**

This work builds upon:
- **PA1**: System call accounting
- **PA2**: SC-MLFQ scheduler

---

## Features Implemented

### 1. Frame Table

- A global **frame table** is maintained to track all physical frames.
- Each frame stores:
  - Allocation status
  - Owning process
  - Virtual address mapping
  - Physical address

---

### 2. Clock Page Replacement Algorithm

- Implemented a **circular frame scan**
- Victim selection:
  - If reference bit = 0 → evict
  - If reference bit = 1 → reset and continue
- Efficient approximation of LRU

---

### 3. Swap Space Management

- Implemented **in-memory swap space** (array-based)
- On eviction:
  - Page contents stored in swap
  - PTE updated accordingly
- On page fault:
  - Page restored from swap

---

### 4. Modified Page Fault Handling

- Extended `usertrap()` to:
  - Handle lazy allocation
  - Detect swapped-out pages
  - Trigger swap-in or allocation
- If memory is full:
  - Eviction is triggered instead of process termination

---

### 5. Scheduler-Aware Eviction (PA2 Integration)

- Eviction prefers pages belonging to:
  - **Lower priority processes** (lower MLFQ queues)
- Ensures:
  - Interactive processes retain memory longer

---

### 6. Per-Process Memory Statistics (PA1 Integration)

Extended `struct proc` to track:

```c
struct vmstats {
  int page_faults;
  int pages_evicted;
  int pages_swapped_in;
  int pages_swapped_out;
  int resident_pages;
};
```

---

### 7. New System Call

```c
int getvmstats(int pid, struct vmstats *info);
```

- Returns memory statistics of a process
- Returns:

  - `0` on success
  - `-1` if PID invalid

---

## Files Modified

- `kernel/vm.c` → page table + swap logic
- `kernel/trap.c` → page fault handling
- `kernel/kalloc.c` → frame allocation + eviction trigger
- `kernel/proc.c / proc.h` → process metadata + stats

---

## Experimental Evaluation

### Workloads Tested

- Large memory allocation using `sbrklazy()`
- Sequential page access patterns
- Forced memory exhaustion scenarios

---

### Observations

- Page faults increase significantly under memory pressure
- Evictions occur once physical memory is full
- Pages of **low-priority processes** are evicted earlier
- Swapped-out pages are correctly restored on access

---

### Example Behavior

1. Allocate large memory region
2. Access pages sequentially
3. Observe:
   - Page faults
   - Evictions
   - Swap-in operations

---

## Results Summary

| Metric           | Observation                               |
| ---------------- | ----------------------------------------- |
| Page Faults      | Increase with memory usage                |
| Evictions        | Triggered when memory full                |
| Swap In/Out      | Function correctly                        |
| Scheduler Impact | Lower priority processes lose pages first |

---

## Design Decisions

- Used **Clock algorithm** for simplicity and efficiency
- Swap space implemented in-memory (as per assignment constraints)
- Frame table chosen as central structure for tracking memory
- Scheduler-aware eviction implemented via process priority comparison

---

## Assumptions

- Swap space is limited and resides in kernel memory
- No disk-based swapping required
- Only user pages are considered for eviction

---

## Limitations

- Swap space is fixed size
- No persistence across reboots
- No advanced replacement policies (e.g., LRU variants)

---

## Folder Structure

```
CS24BTECH11036/
├── kernel/
│   ├── defs.h
│   ├── exec.c
│   ├── vm.c
│   ├── vmstruct.h
│   ├── kalloc.c
│   ├── trap.c
│   ├── proc.c
│   ├── proc.h
│   ├── syscall.c
│   └── syscall.h
│
├── user/
│   ├── user.h
│   ├── usys.pl
│   ├── PA_3_1.c
│   ├── PA_3_2.c
│   ├── PA_3_3.c
│   ├── PA_3_4.c
│   ├── PA_3_5.c
│   ├── PA_3_6.c
│   ├── PA_3_7.c
│   └── run_tests.c
│
├── Makefile
└── README.md
```
---

## How to Run

bash:
make clean
make qemu

Run test programs:

bash:
./your_test_program

---

## Conclusion

This implementation successfully extends xv6 with:
- Efficient page replacement
- Swap-based memory extension
- Scheduler-aware eviction policy

It demonstrates practical integration of:
- Virtual memory management
- Scheduling policies
- Kernel-level data structures

---
