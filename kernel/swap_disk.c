#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "proc.h"
#include "swap_disk.h"
#include "memlayout.h"

#define MAX_DISK_REQS 128
#define ROTATIONAL_DELAY 5
#define NDISKS 4
// RAID 0: max slot 3583 → max phys block = (3583*8+7)/4 = 7167  < 16384 OK
// RAID 1: max slot 3583 → max phys block = (3583*8+7)%16384+2048 = 14335 < 16384 OK
// RAID 5: max slot 3583 → max phys block = (3583*8+7)/3+4096 = 13653 < 16384 OK
#define DISK_BLOCKS_PER_DISK 16384
#define MAX_SWAP_SLOTS MAX_SWAP // matches MAX_SWAP in memlayout.h
#define BLOCKS_PER_PAGE (PGSIZE / BSIZE)

struct disk_req
{
    int valid;
    int blockno;
    int write;
    struct proc *proc;
    int priority;
};

static struct
{
    struct spinlock lock;
    struct disk_req reqs[MAX_DISK_REQS];
    int head;
    int policy;
    int total_latency;
    int total_requests;
} diskq;

static char simulated_disks[NDISKS][DISK_BLOCKS_PER_DISK][BSIZE];
int raid_mode;
static int swap_slot_raid_mode[MAX_SWAP_SLOTS];

// ── Physical I/O ──────────────────────────────────────────────────────────────
static void phys_write(int disk, int block, char *data, int mode)
{
    if (mode == RAID_1)
        block += 2048;
    if (mode == RAID_5)
        block += 4096;

    if (disk < 0 || disk >= NDISKS || block < 0 || block >= DISK_BLOCKS_PER_DISK)
        panic("phys_write: out of range");
    memmove(simulated_disks[disk][block], data, BSIZE);
}

static void phys_read(int disk, int block, char *data, int mode)
{
    if (mode == RAID_1)
        block += 2048;
    if (mode == RAID_5)
        block += 4096;

    if (disk < 0 || disk >= NDISKS || block < 0 || block >= DISK_BLOCKS_PER_DISK)
        panic("phys_read: out of range");
    memmove(data, simulated_disks[disk][block], BSIZE);
}

// ── Scheduler: pick next request (called with diskq.lock held) ───────────────
static int disk_schedule_locked(void)
{
    int chosen = -1;

    if (diskq.policy == DISK_SCHED_FCFS)
    {
        for (int i = 0; i < MAX_DISK_REQS; i++)
        {
            if (diskq.reqs[i].valid)
            {
                chosen = i;
                break;
            }
        }
    }
    else
    { // SSTF: closest block; priority breaks distance ties
        int min_dist = -1, best_pri = -1;
        for (int i = 0; i < MAX_DISK_REQS; i++)
        {
            if (!diskq.reqs[i].valid)
                continue;
            int dist = diskq.reqs[i].blockno - diskq.head;
            if (dist < 0)
                dist = -dist;
            if (chosen == -1 || dist < min_dist ||
                (dist == min_dist && diskq.reqs[i].priority < best_pri))
            {
                chosen = i;
                min_dist = dist;
                best_pri = diskq.reqs[i].priority;
            }
        }
    }
    return chosen;
}

// ── Submit a request and immediately execute it (synchronous) ─────────────────
// blockno is the physical block number for latency accounting.
// Actual data transfer is done by the caller via phys_write/phys_read.
static void disk_submit_and_dispatch(int blockno, int write)
{
    acquire(&diskq.lock);

    int slot = -1;
    for (int i = 0; i < MAX_DISK_REQS; i++)
    {
        if (!diskq.reqs[i].valid)
        {
            slot = i;
            break;
        }
    }
    if (slot == -1)
        panic("disk_submit: queue full");

    diskq.reqs[slot].valid = 1;
    diskq.reqs[slot].blockno = blockno;
    diskq.reqs[slot].write = write;
    diskq.reqs[slot].proc = myproc();
    diskq.reqs[slot].priority = myproc()->cur_queue_lvl;

    int idx = disk_schedule_locked();
    if (idx == -1)
    {
        release(&diskq.lock);
        return;
    }

    struct disk_req req = diskq.reqs[idx];
    diskq.reqs[idx].valid = 0;

    // Latency = seek distance + rotational delay constant
    int dist = req.blockno - diskq.head;
    if (dist < 0)
        dist = -dist;
    int latency = dist + ROTATIONAL_DELAY;
    diskq.head = req.blockno;
    diskq.total_latency += latency;
    diskq.total_requests += 1;

    release(&diskq.lock);

    // Update per-process stats
    struct proc *p = req.proc;
    if (p)
    {
        if (req.write)
            p->disk_writes++;
        else
            p->disk_reads++;
        acquire(&diskq.lock);
        int tl = diskq.total_latency;
        int tr = diskq.total_requests;
        release(&diskq.lock);
        p->avg_disk_latency = tr ? tl / tr : 0;
    }
}

// ── RAID write ────────────────────────────────────────────────────────────────
static void raid_write(int logical_block, char *chunk, int mode)
{
    if (mode == RAID_0)
    {
        int disk = logical_block % NDISKS;
        int block = logical_block / NDISKS;
        disk_submit_and_dispatch(block, 1);
        phys_write(disk, block, chunk, mode);
    }
    else if (mode == RAID_1)
    {
        // pair 0 (disks 0,1): logical blocks 0 .. DISK_BLOCKS_PER_DISK-1
        // pair 1 (disks 2,3): logical blocks DISK_BLOCKS_PER_DISK .. 2*D-1
        // No collision: 2 * DISK_BLOCKS_PER_DISK(2048) = 4096 = total needed
        int pair = (logical_block / DISK_BLOCKS_PER_DISK) % 2;
        int block = logical_block % DISK_BLOCKS_PER_DISK;
        int disk_a = pair * 2;
        int disk_b = pair * 2 + 1;
        disk_submit_and_dispatch(block, 1);
        phys_write(disk_a, block, chunk, mode);
        phys_write(disk_b, block, chunk, mode);
    }
    else if (mode == RAID_5)
    {
        // 4 disks, 3 data + 1 rotating parity per stripe row
        // row = logical_block / 3,  slot-in-row = logical_block % 3
        // parity_disk = row % 4
        // data_disk: skip parity_disk in sequence 0..3
        int row = logical_block / (NDISKS - 1);
        int slot = logical_block % (NDISKS - 1);
        int parity_disk = row % NDISKS;
        int data_disk = slot < parity_disk ? slot : slot + 1;
        int block = row;

        disk_submit_and_dispatch(block, 1);
        phys_write(data_disk, block, chunk, mode);

        int phys_block = block + 4096; // <-- ADD THIS OFFSET

        // Recompute parity from scratch — incremental XOR is wrong on re-writes
        char parity[BSIZE];
        memset(parity, 0, BSIZE);
        for (int d = 0; d < NDISKS; d++)
        {
            if (d == parity_disk)
                continue;
            for (int b = 0; b < BSIZE; b++)
                parity[b] ^= simulated_disks[d][phys_block][b];
        }
        phys_write(parity_disk, block, parity, mode);
    }
}

// ── RAID read ─────────────────────────────────────────────────────────────────
static void raid_read(int logical_block, char *chunk, int mode)
{
    if (mode == RAID_0)
    {
        int disk = logical_block % NDISKS;
        int block = logical_block / NDISKS;
        disk_submit_and_dispatch(block, 0);
        phys_read(disk, block, chunk, mode);
    }
    else if (mode == RAID_1)
    {
        int pair = (logical_block / DISK_BLOCKS_PER_DISK) % 2;
        int block = logical_block % DISK_BLOCKS_PER_DISK;
        int disk_a = pair * 2;
        disk_submit_and_dispatch(block, 0);
        phys_read(disk_a, block, chunk, mode);
    }
    else if (mode == RAID_5)
    {
        int row = logical_block / (NDISKS - 1);
        int slot = logical_block % (NDISKS - 1);
        int parity_disk = row % NDISKS;
        int data_disk = slot < parity_disk ? slot : slot + 1;
        int block = row;
        disk_submit_and_dispatch(block, 0);
        phys_read(data_disk, block, chunk, mode);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void swap_disk_init(void)
{
    initlock(&diskq.lock, "diskq");
    diskq.head = 0;
    diskq.policy = DISK_SCHED_FCFS;
    diskq.total_latency = 0;
    diskq.total_requests = 0;
    raid_mode = RAID_0;
    memset(simulated_disks, 0, sizeof(simulated_disks));
    memset(swap_slot_raid_mode, 0, sizeof(swap_slot_raid_mode));
}

// void swap_disk_reset_stats(void)
// {
//     acquire(&diskq.lock);
//     diskq.total_latency  = 0;
//     diskq.total_requests = 0;
//     diskq.head           = 0;
//     release(&diskq.lock);
// }

void swap_out_page(char *page_data, int swap_slot)
{
    if (swap_slot < 0 || swap_slot >= MAX_SWAP_SLOTS)
        panic("swap_out_page: invalid slot");

    int logical_block = swap_slot * BLOCKS_PER_PAGE;

    acquire(&diskq.lock);
    int mode = raid_mode;
    swap_slot_raid_mode[swap_slot] = mode;
    release(&diskq.lock);

    for (int i = 0; i < BLOCKS_PER_PAGE; i++)
        raid_write(logical_block + i, page_data + i * BSIZE, mode);
}

void swap_in_page(char *page_data, int swap_slot, int unused)
{
    if (swap_slot < 0 || swap_slot >= MAX_SWAP_SLOTS)
        panic("swap_in_page: invalid slot");

    int logical_block = swap_slot * BLOCKS_PER_PAGE;
    int mode = swap_slot_raid_mode[swap_slot];

    for (int i = 0; i < BLOCKS_PER_PAGE; i++)
        raid_read(logical_block + i, page_data + i * BSIZE, mode);
}

// Called from swap_free() in kalloc.c.
// Zeroes the physical disk blocks so stale data cannot be read back
// if a slot is reused before the former owner's PTE is fully cleaned up.
void swap_disk_free_slot(int swap_slot)
{
    if (swap_slot < 0 || swap_slot >= MAX_SWAP_SLOTS)
        return;

    int logical_block = swap_slot * BLOCKS_PER_PAGE;
    int mode = swap_slot_raid_mode[swap_slot];

    for (int i = 0; i < BLOCKS_PER_PAGE; i++)
    {
        int lb = logical_block + i;
        if (mode == RAID_0)
        {
            int disk = lb % NDISKS;
            int block = lb / NDISKS;
            memset(simulated_disks[disk][block], 0, BSIZE);
        }
        else if (mode == RAID_1)
        {
            int pair = (lb / DISK_BLOCKS_PER_DISK) % 2;
            int block = lb % DISK_BLOCKS_PER_DISK + 2048;
            memset(simulated_disks[pair * 2][block], 0, BSIZE);
            memset(simulated_disks[pair * 2 + 1][block], 0, BSIZE);
        }
        else if (mode == RAID_5)
        {
            int row = lb / (NDISKS - 1);
            int slot = lb % (NDISKS - 1);
            int parity_disk = row % NDISKS;
            int data_disk = slot < parity_disk ? slot : slot + 1;
            int block = row + 4096;
            memset(simulated_disks[data_disk][block], 0, BSIZE);
            memset(simulated_disks[parity_disk][block], 0, BSIZE);
        }
    }
    swap_slot_raid_mode[swap_slot] = 0;
}

int setdisksched(int policy)
{
    if (policy != DISK_SCHED_FCFS && policy != DISK_SCHED_SSTF)
        return -1;
    acquire(&diskq.lock);
    diskq.policy = policy;
    release(&diskq.lock);
    return 0;
}

int setraidmode(int mode)
{
    if (mode != RAID_0 && mode != RAID_1 && mode != RAID_5)
        return -1;
    acquire(&diskq.lock);
    raid_mode = mode;
    release(&diskq.lock);
    return 0;
}

int disk_get_avg_latency(void)
{
    if (diskq.total_requests == 0)
        return 0;
    return diskq.total_latency / diskq.total_requests;
}

int disk_get_total_requests(void)
{
    return diskq.total_requests;
}