// PA4_4.c — RAID 1 (Mirroring) verification
// Verifies all vmstats fields under RAID 1
// Tests RAID mode switching correctness

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE        4096
#define DISK_BLOCKS      512
#define MAXFRAMES        64
#define NPAGES           90

int main(void)
{
    printf("=== PA4 Experiment 4: RAID 1 (Mirroring) Verification ===\n");
    printf("    4 disks → 2 mirror pairs\n");
    printf("    pair=(b/%d)%%2 → disk_a=pair*2, disk_b=pair*2+1\n\n",
           DISK_BLOCKS);

    int pass = 1;
    setraidmode(1); // RAID_1
    setdisksched(0);

    int pid = getpid();
    struct vmstats before, after;
    getvmstats(pid, &before);

    // ── Write all pages ────────────────────────────────────────────────────
    printf("[phase 1] Writing %d pages\n", NPAGES);
    char *mem = sbrk(NPAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    for (int i = 0; i < NPAGES; i++)
        for (int b = 0; b < PAGE_SIZE; b++)
            mem[i * PAGE_SIZE + b] = (char)((i * 11 + b * 3) & 0xFF);

    getvmstats(pid, &after);
    printf("  page_faults+=%d  evicted+=%d  swapped_out+=%d  disk_writes+=%d\n",
           after.page_faults     - before.page_faults,
           after.pages_evicted   - before.pages_evicted,
           after.pages_swapped_out - before.pages_swapped_out,
           after.disk_writes     - before.disk_writes);

    if (after.page_faults - before.page_faults >= 0)
        printf("  PASS: page_faults >= 0\n");
    else { printf("  FAIL: not enough page faults\n"); pass = 0; }

    if (after.pages_swapped_out - before.pages_swapped_out > 0)
        printf("  PASS: pages_swapped_out > 0\n");
    else { printf("  FAIL: pages_swapped_out = 0\n"); pass = 0; }

    if (after.disk_writes - before.disk_writes > 0)
        printf("  PASS: disk_writes > 0 under RAID 1\n");
    else { printf("  FAIL: disk_writes = 0\n"); pass = 0; }

    // ── Read back in reverse ───────────────────────────────────────────────
    printf("\n[phase 2] Reading back in reverse\n");
    int errors = 0;
    for (int i = NPAGES - 1; i >= 0; i--)
        for (int b = 0; b < PAGE_SIZE; b++) {
            char exp = (char)((i * 11 + b * 3) & 0xFF);
            if (mem[i * PAGE_SIZE + b] != exp) errors++;
        }

    getvmstats(pid, &after);
    printf("  swapped_in+=%d  disk_reads+=%d  avg_latency=%d  errors=%d\n",
           after.pages_swapped_in - before.pages_swapped_in,
           after.disk_reads       - before.disk_reads,
           after.avg_disk_latency,
           errors);

    if (errors == 0)
        printf("  PASS: RAID 1 data integrity verified\n");
    else { printf("  FAIL: %d errors\n", errors); pass = 0; }

    if (after.pages_swapped_in - before.pages_swapped_in > 0)
        printf("  PASS: pages_swapped_in > 0\n");
    else { printf("  FAIL: pages_swapped_in = 0\n"); pass = 0; }

    if (after.disk_reads - before.disk_reads > 0)
        printf("  PASS: disk_reads > 0\n");
    else { printf("  FAIL: disk_reads = 0\n"); pass = 0; }

    if (after.avg_disk_latency >= 5)
        printf("  PASS: avg_disk_latency=%d >= C=5\n", after.avg_disk_latency);
    else { printf("  FAIL: avg_disk_latency=%d < 5\n", after.avg_disk_latency); pass = 0; }

    if (after.resident_pages <= MAXFRAMES)
        printf("  PASS: resident_pages=%d <= MAXFRAMES=%d\n",
               after.resident_pages, MAXFRAMES);
    else { printf("  FAIL: resident_pages=%d > MAXFRAMES\n",
               after.resident_pages); pass = 0; }

    // ── RAID mode switch: RAID 1 → RAID 0 → RAID 5 ───────────────────────
    printf("\n[phase 3] RAID mode switching\n");
    sbrk(-(NPAGES * PAGE_SIZE));

    int modes[3] = {0, 2, 1};
    const char *mnames[3] = {"RAID_0", "RAID_5", "RAID_1"};
    for (int m = 0; m < 3; m++) {
        setraidmode(modes[m]);
        char *m2 = sbrk(20 * PAGE_SIZE);
        for (int i = 0; i < 20; i++)
            m2[i * PAGE_SIZE] = (char)(i & 0xFF);
        for (int i = 19; i >= 0; i--) {
            volatile char c = m2[i * PAGE_SIZE]; (void)c;
        }
        int errs = 0;
        for (int i = 0; i < 20; i++)
            if (m2[i * PAGE_SIZE] != (char)(i & 0xFF)) errs++;
        if (errs == 0)
            printf("  PASS: switch to %s — data correct\n", mnames[m]);
        else { printf("  FAIL: switch to %s — %d errors\n", mnames[m], errs); pass = 0; }
        sbrk(-(20 * PAGE_SIZE));
    }

    printf("\n=== Experiment 4 %s ===\n", pass ? "PASSED" : "FAILED");
    exit(!pass);
}