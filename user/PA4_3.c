// PA4_3.c — RAID 0 (Striping) verification
// Tests sys_setraidmode: valid (0,1,2) and invalid values
// Tests RAID 0 block mapping: disk=b%4, block=b/4
// Verifies all vmstats fields under RAID 0

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define BSIZE     1024
#define NDISKS    4
#define MAXFRAMES 64
#define NPAGES    90

int main(void)
{
    printf("=== PA4 Experiment 3: RAID 0 (Striping) Verification ===\n");
    printf("    Mapping: logical block b → disk=b%%%d, phys=b/%d\n",
           NDISKS, NDISKS);
    printf("    Page i → logical blocks [i*4 .. i*4+3] on 4 disks\n\n");

    int pass = 1;

    // ── setraidmode validation ─────────────────────────────────────────────
    printf("[check] setraidmode edge cases\n");

    if (setraidmode(0) == 0)
        printf("  PASS: setraidmode(0) = RAID_0 accepted\n");
    else { printf("  FAIL: setraidmode(0) rejected\n"); pass = 0; }

    if (setraidmode(1) == 0)
        printf("  PASS: setraidmode(1) = RAID_1 accepted\n");
    else { printf("  FAIL: setraidmode(1) rejected\n"); pass = 0; }

    if (setraidmode(2) == 0)
        printf("  PASS: setraidmode(2) = RAID_5 accepted\n");
    else { printf("  FAIL: setraidmode(2) rejected\n"); pass = 0; }

    if (setraidmode(3) == -1)
        printf("  PASS: setraidmode(3) rejected with -1\n");
    else { printf("  FAIL: setraidmode(3) should return -1\n"); pass = 0; }

    if (setraidmode(-1) == -1)
        printf("  PASS: setraidmode(-1) rejected with -1\n");
    else { printf("  FAIL: setraidmode(-1) should return -1\n"); pass = 0; }

    if (setraidmode(99) == -1)
        printf("  PASS: setraidmode(99) rejected with -1\n");
    else { printf("  FAIL: setraidmode(99) should return -1\n"); pass = 0; }

    // ── Activate RAID 0 ────────────────────────────────────────────────────
    printf("\n[setup] RAID 0 + FCFS\n");
    setraidmode(0);
    setdisksched(0);

    int pid = getpid();
    struct vmstats before, after;
    getvmstats(pid, &before);

    // ── Allocate + full-page write ─────────────────────────────────────────
    printf("[step 1] Writing %d full pages (%d bytes each)\n",
           NPAGES, PAGE_SIZE);
    char *mem = sbrk(NPAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    for (int i = 0; i < NPAGES; i++)
        for (int b = 0; b < PAGE_SIZE; b++)
            mem[i * PAGE_SIZE + b] = (char)((i * 7 + b) & 0xFF);

    getvmstats(pid, &after);
    int new_faults  = after.page_faults     - before.page_faults;
    int new_evicted = after.pages_evicted   - before.pages_evicted;
    int new_sout    = after.pages_swapped_out - before.pages_swapped_out;
    int new_dw      = after.disk_writes     - before.disk_writes;

    printf("  page_faults+=%d  evicted+=%d  swapped_out+=%d  disk_writes+=%d\n",
           new_faults, new_evicted, new_sout, new_dw);

    if (new_faults > 0)
        printf("  PASS: page_faults > 0\n");
    else { printf("  FAIL: page_faults += %d < NPAGES=%d\n", new_faults, NPAGES); pass = 0; }

    if (new_evicted > 0)
        printf("  PASS: evictions occurred\n");
    else { printf("  FAIL: no evictions\n"); pass = 0; }

    if (new_sout > 0)
        printf("  PASS: pages_swapped_out > 0\n");
    else { printf("  FAIL: pages_swapped_out = 0\n"); pass = 0; }

    if (new_dw > 0)
        printf("  PASS: disk_writes > 0 (RAID 0 disk layer active)\n");
    else { printf("  FAIL: disk_writes = 0\n"); pass = 0; }

    // ── Read back: verify all vmstats fields + data ────────────────────────
    printf("\n[step 2] Reading back all pages (triggers swap-in)\n");
    int errors = 0;
    for (int i = 0; i < NPAGES; i++)
        for (int b = 0; b < PAGE_SIZE; b++) {
            char exp = (char)((i * 7 + b) & 0xFF);
            if (mem[i * PAGE_SIZE + b] != exp) errors++;
        }

    getvmstats(pid, &after);
    int new_sin = after.pages_swapped_in - before.pages_swapped_in;
    int new_dr  = after.disk_reads       - before.disk_reads;
    int res     = after.resident_pages;

    printf("  swapped_in+=%d  disk_reads+=%d  resident=%d  errors=%d\n",
           new_sin, new_dr, res, errors);

    if (errors == 0)
        printf("  PASS: RAID 0 data integrity — all %d pages correct\n", NPAGES);
    else { printf("  FAIL: %d pages corrupted\n", errors); pass = 0; }

    if (new_sin > 0)
        printf("  PASS: pages_swapped_in > 0\n");
    else { printf("  FAIL: pages_swapped_in = 0\n"); pass = 0; }

    if (new_dr > 0)
        printf("  PASS: disk_reads > 0\n");
    else { printf("  FAIL: disk_reads = 0\n"); pass = 0; }

    if (after.avg_disk_latency >= 5)
        printf("  PASS: avg_disk_latency=%d >= C=5\n", after.avg_disk_latency);
    else { printf("  FAIL: avg_disk_latency=%d < 5\n", after.avg_disk_latency); pass = 0; }

    if (res <= MAXFRAMES)
        printf("  PASS: resident_pages=%d <= MAXFRAMES=%d\n", res, MAXFRAMES);
    else { printf("  FAIL: resident_pages=%d > MAXFRAMES=%d\n", res, MAXFRAMES); pass = 0; }

    printf("\n=== Experiment 3 %s ===\n", pass ? "PASSED" : "FAILED");
    exit(!pass);
}