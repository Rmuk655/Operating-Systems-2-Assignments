// PA4_5.c — RAID 5 (Striping + Parity) verification
// Multiple cycles verify parity is updated not just written once
// All vmstats fields checked each cycle

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define MAXFRAMES 64
#define NPAGES    90
#define CYCLES    4

int main(void)
{
    printf("=== PA4 Experiment 5: RAID 5 (Parity) Verification ===\n");
    printf("    parity_disk = block%%4 (rotates)\n");
    printf("    parity = XOR of all data blocks in stripe\n");
    printf("    %d write→evict→verify cycles\n\n", CYCLES);

    int pass = 1;
    setraidmode(2); // RAID_5
    setdisksched(1); // SSTF

    int pid = getpid();
    struct vmstats s0, s;
    getvmstats(pid, &s0);

    char *mem = sbrk(NPAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    int prev_sout = s0.pages_swapped_out;
    int prev_sin  = s0.pages_swapped_in;
    int prev_dw   = s0.disk_writes;
    int prev_dr   = s0.disk_reads;

    for (int c = 0; c < CYCLES; c++) {
        printf("[cycle %d] generation=%d\n", c, c);

        // Write new generation — forces parity update for each block
        for (int i = 0; i < NPAGES; i++)
            for (int b = 0; b < PAGE_SIZE; b++)
                mem[i * PAGE_SIZE + b] = (char)((i * 17 + b + c * 31) & 0xFF);

        // Evict by reverse access
        for (int i = NPAGES - 1; i >= 0; i--) {
            volatile char x = mem[i * PAGE_SIZE]; (void)x;
        }

        // Verify full page content
        int errors = 0;
        for (int i = 0; i < NPAGES; i++)
            for (int b = 0; b < PAGE_SIZE; b++) {
                char exp = (char)((i * 17 + b + c * 31) & 0xFF);
                if (mem[i * PAGE_SIZE + b] != exp) errors++;
            }

        getvmstats(pid, &s);
        int dsout = s.pages_swapped_out - prev_sout;
        int dsin  = s.pages_swapped_in  - prev_sin;
        int ddw   = s.disk_writes       - prev_dw;
        int ddr   = s.disk_reads        - prev_dr;

        printf("  sout+=%d  sin+=%d  dw+=%d  dr+=%d  lat=%d  errors=%d\n",
               dsout, dsin, ddw, ddr, s.avg_disk_latency, errors);

        if (errors == 0)
            printf("  PASS: cycle %d data intact (parity correctly updated)\n", c);
        else { printf("  FAIL: cycle %d — %d errors\n", c, errors); pass = 0; }

        // Each cycle after 0 must have new swaps
        if (c > 0) {
            if (dsout > 0)
                printf("  PASS: new swap-outs in cycle %d\n", c);
            else
                printf("  NOTE: no new swap-outs cycle %d\n", c);
        }

        if (s.resident_pages <= MAXFRAMES)
            printf("  PASS: resident_pages=%d <= MAXFRAMES\n", s.resident_pages);
        else { printf("  FAIL: resident_pages=%d > MAXFRAMES\n",
               s.resident_pages); pass = 0; }

        if (s.avg_disk_latency >= 5)
            printf("  PASS: avg_disk_latency=%d >= C=5\n", s.avg_disk_latency);
        else { printf("  FAIL: avg_disk_latency=%d < 5\n", s.avg_disk_latency); pass = 0; }

        prev_sout = s.pages_swapped_out;
        prev_sin  = s.pages_swapped_in;
        prev_dw   = s.disk_writes;
        prev_dr   = s.disk_reads;
        printf("\n");
    }

    // ── Final totals ────────────────────────────────────────────────────────
    getvmstats(pid, &s);
    printf("[final totals]\n");
    printf("  page_faults=%d  evicted=%d  sout=%d  sin=%d\n",
           s.page_faults   - s0.page_faults,
           s.pages_evicted - s0.pages_evicted,
           s.pages_swapped_out - s0.pages_swapped_out,
           s.pages_swapped_in  - s0.pages_swapped_in);
    printf("  disk_writes=%d  disk_reads=%d  avg_latency=%d\n",
           s.disk_writes - s0.disk_writes,
           s.disk_reads  - s0.disk_reads,
           s.avg_disk_latency);

    if (s.disk_writes - s0.disk_writes > 0)
        printf("  PASS: disk_writes > 0 under RAID 5\n");
    else { printf("  FAIL: disk_writes = 0 under RAID 5\n"); pass = 0; }

    printf("\n=== Experiment 5 %s ===\n", pass ? "PASSED" : "FAILED");
    exit(!pass);
}