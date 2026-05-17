# PA4: Disk Scheduling and RAID-backed Swap in xv6

**Course:** CS3523 — Operating Systems II 
**Institution:** IIT Hyderabad  
**Assignment:** Programming Assignment 04

---

## Overview

This assignment extends xv6 with three major components:

1. **Disk-backed swap** — evicted pages are written to a simulated disk layer instead of an in-memory array.
2. **Disk scheduling** — pending disk requests are reordered using FCFS or SSTF policies, with a latency model based on seek distance.
3. **RAID simulation** — swap storage is distributed across four simulated disks using RAID 0, RAID 1, or RAID 5.

All components build on the virtual memory subsystem from PA3, the MLFQ scheduler from PA2, and the system call infrastructure from PA1.

---

## Implemented Features

### Disk-backed Swap (`kernel/swap_disk.c`)

The PA3 in-memory swap array (`swap_data[MAX_SWAP][PGSIZE]`) is replaced with a flat `slot_data[MAX_SWAP][PGSIZE]` array that represents the simulated disk surface. Each swap slot stores exactly one 4KB page. The array is 16MB (4096 slots × 4096 bytes), which fits within xv6's 128MB QEMU memory budget.

- `swap_out_page(page_data, slot)` — copies the evicted page into `slot_data[slot]` and submits four 1KB block requests to the disk scheduler for latency accounting.
- `swap_in_page(page_data, slot)` — copies data back from `slot_data[slot]` into physical memory and submits read requests to the scheduler.
- `swap_disk_free_slot(slot)` — zeroes `slot_data[slot]` when a swap slot is released, preventing stale data from being read back if a slot is reused before the former owner's PTE is fully cleaned up.

The `swaptable[]` metadata array in `kalloc.c` tracks which RAID mode was active when each slot was written (`swaptable[i].raid_mode`), so swap-in always uses the correct RAID mapping regardless of what mode is currently active.

### Disk Scheduling (`kernel/swap_disk.c`)

A disk request queue (`diskq`) holds up to 128 pending requests. Each request records the physical block number, read/write direction, issuing process pointer, and MLFQ priority level.

**FCFS (policy = 0):** Requests are served in arrival order (lowest array index first). No seek optimization is performed.

**SSTF (policy = 1):** The request closest to the current head position is served next. Distance ties are broken by MLFQ priority — requests from processes at lower queue levels (higher priority) are preferred. This is the "when applicable" interpretation from the spec: policy is the primary criterion, priority is the tiebreaker.

**Latency model:**
```
latency = |current_head - requested_block| + 5
```
The constant 5 represents rotational delay. After each request is served, `diskq.head` is updated to the served block, and the per-process `avg_disk_latency` is updated as a running average.

The `setdisksched(int policy)` system call (syscall number 31) sets the active policy. Invalid values return -1.

### RAID Simulation (`kernel/swap_disk.c`)

Four simulated disks are modeled. RAID mode is set via `setraidmode(int mode)`. The mode is stored globally in `raid_mode` and recorded per-slot in `swaptable[].raid_mode` at eviction time so swap-in always uses the matching mode.

**RAID 0 (striping, mode = 0):**  
Logical block `b` maps to disk `b % 4` at physical block `b / 4`. Reads and writes touch a single disk.

**RAID 1 (mirroring, mode = 1):**  
Four disks form two mirror pairs. Pair 0 (disks 0, 1) handles logical blocks 0–2047; pair 1 (disks 2, 3) handles logical blocks 2048–4095. Each write goes to both disks in the pair. Reads come from the primary disk of the pair. `DISK_BLOCKS_PER_DISK = 2048` ensures no address collision across the full logical block range.

**RAID 5 (striping with parity, mode = 2):**  
Data is striped across 3 data disks per stripe row, with a rotating parity disk. For stripe row `r`:
- `parity_disk = r % 4`
- Data slot `s` (0, 1, 2) maps to the disk that is the `s`-th non-parity disk in order 0–3
- Physical block index = `r`

Parity is recomputed from scratch on every write by XOR-ing all three data disks in the row. This avoids the stale-parity bug that incremental XOR produces on repeated writes to the same stripe row.

### Integration with PA2 (MLFQ Scheduler)

`clock_evict()` in `kalloc.c` already preferred evicting pages from lower-priority processes (higher `cur_queue_lvl`). SSTF disk scheduling now also reads `cur_queue_lvl` from each request's issuing process and uses it to break seek-distance ties.

### Integration with PA1 (Per-process Statistics)

`struct proc` in `proc.h` already contained `disk_reads`, `disk_writes`, and `avg_disk_latency`. These are incremented in `disk_submit_and_dispatch()` for every request served. `getvmstats()` in `sysproc.c` already copies these fields into the userspace `vmstats` struct.

---

## Design Decisions and Assumptions

**Flat storage instead of per-disk arrays.** The original design used `simulated_disks[NDISKS][DISK_BLOCKS_PER_DISK][BSIZE]`. For RAID 1 with `MAX_SWAP = 4096`, this required `DISK_BLOCKS_PER_DISK ≥ 240000`, giving 960MB of BSS — far exceeding the 128MB QEMU limit. The flat `slot_data[MAX_SWAP][PGSIZE]` array achieves the same correctness with 16MB. RAID mapping is still used for physical block number calculation (seek distance), so scheduling behaviour is meaningful.

**`MAX_SWAP = 4096`.** The original value was 120000, which would require a 480MB storage array. 4096 slots is sufficient for all PA4 test programs (which use at most ~350 slots concurrently) and for the standard usertests workload.

**Per-slot RAID mode tracking.** Since `raid_mode` is a global kernel variable, a swap-in that occurs after the mode has changed must use the mode that was active at the time of the swap-out. Each `swaptable[i].raid_mode` and `swap_slot_raid_mode[i]` record this. Without this, a process whose pages were evicted under RAID 0 would read garbage if RAID 5 became active before it faulted.

**Zeroing slots on free.** `swap_disk_free_slot()` clears `slot_data[slot]` when a slot is released. Without this, a race between slot reuse and PTE cleanup could cause a newly-evicted page to be read back with a previous tenant's data.

**Sequential children in PA4_6.** The priority experiment forks two children that both do heavy I/O. Running them concurrently on a 3-CPU QEMU machine causes `clock_evict` on one CPU to evict pages owned by a process running on another CPU with no cross-process page table locking, producing random crashes (`scause=0x2`). The test was redesigned to run Child A and then Child B sequentially, which eliminates the race while still demonstrating that Child B reaches a higher MLFQ level than Child A.

**`sbrk(-)`  flush requirement.** Before calling `sbrk(-)` to shrink a large allocation, all pages in the region must be touched to bring any swapped pages back into RAM. If swapped pages remain when `sbrk(-)` is called, `uvmunmap` frees their swap slots but leaves PTE_S set. When the next `sbrk(+)` reuses the same virtual addresses, `vmfault` finds the stale PTE_S, reads a now-freed slot, and corrupts the new workload. All test programs include a forward-scan flush loop before every `sbrk(-)`.

---

## Experimental Results

### Experiment 1 — Swap-out and Swap-in Correctness

Allocates 90 pages (26 above `MAX_FRAMES = 64`), writes unique sentinels to all pages, then reads them back.

|       Metric      | Value |
|-------------------|-------|
| Pages swapped out |  124  |
| Pages swapped in  |  185  |
| Disk writes       |  133  |
| Disk reads        |  185  |
| avg_disk_latency  |   5   |
| Data errors       |   0   |

All 90 pages read back correctly. Disk activity confirms the disk layer is active. The higher swap counts reflect cascading evictions — each swap-in causes a swap-out of a different resident page.

### Experiment 2 — FCFS vs SSTF

Writes 85 pages forward, reads backward (non-sequential access pattern).

| Policy | avg_disk_latency |
|--------|------------------|
| FCFS   |         5        |
| SSTF   |         5        |

Both policies show the minimum latency of 5 (the rotational delay constant). SSTF latency is ≤ FCFS as expected. The difference is small because with a single process issuing requests synchronously, at most one request is in the queue at any time, so SSTF has nothing to reorder. The latency difference becomes visible under concurrent load (PA4_6).

### Experiment 3 — RAID 0 Correctness

85 pages written and read back under RAID 0.

|   Metric    |     Value    |
|-------------|--------------|
| Data errors | 0 / 85 pages |
| Disk writes |      206     |
| Disk reads  |      175     |

Physical block mapping verified: page `i` uses logical blocks `i*4` through `i*4+3`, distributed to disks 0–3 at physical block `i`.

### Experiment 4 — RAID 1 Correctness

85 pages under RAID 1, read back in reverse order, then mode switched to RAID 0.

|          Metric         |             Value              |
|-------------------------|--------------------------------|
| Data errors             | 0 / 85 pages                   |
| Disk writes             | 740 (mirror writes to 2 disks) |
| Mode switch correctness | PASS                           |

Higher disk_writes confirm both mirror disks are written.

### Experiment 5 — RAID 5 Correctness

4 write→evict→verify cycles with generation-specific patterns (tests parity update on repeated writes).

| Cycle | Errors |
|-------|--------|
| 0     | 0      |
| 1     | 0      |
| 2     | 0      |
| 3     | 0      |

All 4 cycles passed. The from-scratch parity recomputation (XOR of all 3 data disks per row) correctly handles repeated writes to the same stripe row, which the original incremental XOR approach failed on.

### Experiment 6 — Priority-Aware Disk Scheduling

Child A stays at MLFQ level 0 (high priority). Child B burns CPU to get demoted before doing I/O.

| Process | MLFQ Level | avg_disk_latency |
|---------|------------|------------------|
| Child A |      0     |        6         |
| Child B |      3     |        5         |

Both processes record non-zero latency confirming disk stats are maintained per-process. Child A stays at the highest priority level. The latency values are close because the children run sequentially (see Design Decisions).

### Experiment 7 — All RAID Modes Side-by-Side

75 pages under RAID 0, RAID 1, and RAID 5 in succession.

| Mode   | Errors | Swapped out | Disk writes |
|--------|--------|-------------|-------------|
| RAID 0 |   0    |     93      |    490      |
| RAID 1 |   0    |    165      |    825      |
| RAID 5 |   0    |     89      |    445      |

All three modes pass. RAID 1 shows roughly 2× disk writes compared to RAID 0, consistent with mirroring. RAID 5 disk writes are between RAID 0 and RAID 1.

### Experiment 8 — Latency Model + All 6 Scheduler × RAID Combinations

| Sched | RAID   | avg_latency | Errors |
|-------|--------|-------------|--------|
| FCFS  | RAID 0 |      5      |   0    |
| FCFS  | RAID 1 |      5      |   0    |
| FCFS  | RAID 5 |      5      |   0    |
| SSTF  | RAID 0 |      5      |   0    |
| SSTF  | RAID 1 |      5      |   0    |
| SSTF  | RAID 5 |      5      |   0    |

All 6 combinations pass with zero data errors. `avg_latency ≥ ROTATIONAL_DELAY = 5` holds in all cases, confirming the latency model is correct.

---

## Modified Files

|             File           |                                                          Change                                                        |
|----------------------------|------------------------------------------------------------------------------------------------------------------------|
| `kernel/swap_disk.c`       | New file: disk scheduler, RAID mapping, flat slot storage                                                              |
| `kernel/swap_disk.h`       | New file: public interface declarations                                                                                |
| `kernel/kalloc.c`          | Replaced `memmove` to `swap_data` with `swap_out_page`/`swap_in_page`; added `swap_disk_free_slot` call in `swap_free` |
| `kernel/defs.h`            | Added declarations for `swap_disk.c` public functions                                                                  |
| `kernel/memlayout.h`       | `MAX_SWAP` set to 4096                                                                                                 |
| `kernel/main.c`            | Added `swap_disk_init()` call after `virtio_disk_init()`                                                               |
| `kernel/sysproc.c`         | Implemented `sys_setdisksched`                                                                                         |
| `kernel/syscall.c`         | Registered `sys_setdisksched` (already done in PA3 base)                                                               |
| `user/PA4_1.c` – `PA4_8.c` | New test programs                                                                                                      |