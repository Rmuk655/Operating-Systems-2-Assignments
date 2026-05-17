// PA4_8.c — Latency model + all 6 sched×RAID combinations
// Every combination verifies all 8 vmstats fields

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE        4096
#define MAXFRAMES        64
#define NPAGES           90
#define ROTATIONAL_DELAY 5

static int run_combo(int sched, int raid,
                     const char *sname, const char *rname, int seed)
{
    setdisksched(sched);
    setraidmode(raid);

    int pid = getpid();
    struct vmstats before, after;
    getvmstats(pid, &before);

    char *mem = sbrk(NPAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    // forward write then backward read → non-sequential
    for (int i = 0; i < NPAGES; i++)
        mem[i * PAGE_SIZE] = (char)((i * seed + sched + raid) & 0xFF);
    for (int i = NPAGES-1; i >= 0; i--) {
        volatile char c = mem[i * PAGE_SIZE]; (void)c;
    }

    int errors = 0;
    for (int i = 0; i < NPAGES; i++) {
        char exp = (char)((i * seed + sched + raid) & 0xFF);
        if (mem[i * PAGE_SIZE] != exp) errors++;
    }

    getvmstats(pid, &after);
    int df  = after.page_faults      - before.page_faults;
    int de  = after.pages_evicted    - before.pages_evicted;
    int dso = after.pages_swapped_out - before.pages_swapped_out;
    int dsi = after.pages_swapped_in  - before.pages_swapped_in;
    int ddw = after.disk_writes       - before.disk_writes;
    int ddr = after.disk_reads        - before.disk_reads;
    int lat = after.avg_disk_latency;
    int res = after.resident_pages;

    int ok = (errors == 0 &&
              df  >= 0 &&
              de  > 0 &&
              dso > 0 &&
              dsi > 0 &&
              ddw > 0 &&
              ddr > 0 &&
              lat >= ROTATIONAL_DELAY &&
              res <= MAXFRAMES);

    printf("  %s + %s : faults=%d evicted=%d sout=%d sin=%d "
           "dw=%d dr=%d lat=%d res=%d err=%d  %s\n",
           sname, rname,
           df, de, dso, dsi, ddw, ddr, lat, res, errors,
           ok ? "PASS" : "FAIL");

    sbrk(-(NPAGES * PAGE_SIZE));
    return ok;
}

int main(void)
{
    printf("=== PA4 Experiment 8: Latency Model + All sched x RAID ===\n\n");

    int pass = 1;

    // ── Latency model verification ─────────────────────────────────────────
    printf("[phase 1] Latency model: avg_latency = |head-block| + C=%d\n\n",
           ROTATIONAL_DELAY);
    printf("          Every request adds at least C=%d\n",
           ROTATIONAL_DELAY);
    printf("          Therefore avg_latency >= %d always\n\n",
           ROTATIONAL_DELAY);

    setdisksched(0); setraidmode(0);
    int pid = getpid();
    struct vmstats s0, s1, s2;
    getvmstats(pid, &s0);

    char *mem = sbrk(NPAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    for (int i = 0; i < NPAGES; i++)
        mem[i * PAGE_SIZE] = (char)(i & 0xFF);
    getvmstats(pid, &s1);
    printf("  after write:  dw=%d  lat=%d\n",
           s1.disk_writes - s0.disk_writes, s1.avg_disk_latency);

    for (int i = NPAGES-1; i >= 0; i--) {
        volatile char c = mem[i * PAGE_SIZE]; (void)c;
    }
    getvmstats(pid, &s2);
    printf("  after read:   dr=%d  lat=%d\n",
           s2.disk_reads - s0.disk_reads, s2.avg_disk_latency);

    if (s2.avg_disk_latency >= ROTATIONAL_DELAY)
        printf("  PASS: avg_latency=%d >= C=%d\n",
               s2.avg_disk_latency, ROTATIONAL_DELAY);
    else { printf("  FAIL: avg_latency=%d < C=%d\n",
               s2.avg_disk_latency, ROTATIONAL_DELAY); pass = 0; }

    if (s2.disk_writes > s1.disk_writes || s2.disk_reads > s0.disk_reads)
        printf("  PASS: disk stats grow with more I/O\n");
    else { printf("  FAIL: disk stats not growing\n"); pass = 0; }

    sbrk(-(NPAGES * PAGE_SIZE));

    // ── All 6 combinations ─────────────────────────────────────────────────
    printf("\n[phase 2] All sched x RAID combinations (%d pages, "
           "all 8 vmstats fields checked)\n", NPAGES);
    printf("  %s   %s   %s %s %s %s %s %s %s %s %s  result\n",
           "sched", "raid",
           "fault","evict","sout","sin","dw","dr","lat","res","err");
    printf("  %s\n",
           "-------------------------------------------------------------------");

    pass &= run_combo(0, 0, "FCFS", "RAID0", 7);
    pass &= run_combo(0, 1, "FCFS", "RAID1", 11);
    pass &= run_combo(0, 2, "FCFS", "RAID5", 13);
    pass &= run_combo(1, 0, "SSTF", "RAID0", 17);
    pass &= run_combo(1, 1, "SSTF", "RAID1", 19);
    pass &= run_combo(1, 2, "SSTF", "RAID5", 23);

    printf("\n=== Experiment 8 %s ===\n", pass ? "PASSED" : "FAILED");
    exit(!pass);
}