// swap_disk.h
#ifndef SWAP_DISK_H
#define SWAP_DISK_H

// RAID modes
#define RAID_0  0
#define RAID_1  1
#define RAID_5  2

// Scheduler policies
#define DISK_SCHED_FCFS  0
#define DISK_SCHED_SSTF  1

// public interface
void swap_disk_init(void);
void swap_out_page(char *page_data, int swap_slot);
void swap_in_page(char *page_data, int swap_slot, int raid_mode_override);
int  setdisksched(int policy);
int  setraidmode(int mode);
int  disk_get_avg_latency(void);
int  disk_get_total_requests(void);

#endif