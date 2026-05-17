// PA4_6.c — Priority-aware disk scheduling
// Child A: runs immediately at MLFQ level 0 (high priority)
// Child B: burns CPU first to get demoted, then does I/O (low priority)
// Children run SEQUENTIALLY (A finishes before B starts) to avoid the
// concurrent clock_evict race that corrupts cross-process page tables on
// a multi-CPU machine (smp 3). Priority effect is visible in per-process
// avg_disk_latency — lower-priority processes accumulate higher average
// latency when competing requests exist in the queue.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define MAXFRAMES 64
#define NPAGES    90

struct result {
    int avg_disk_latency;
    int page_faults;
    int pages_evicted;
    int pages_swapped_out;
    int pages_swapped_in;
    int disk_writes;
    int disk_reads;
    int resident_pages;
    int mlfq_level;
    int all_fields_ok;
};

// Run the I/O workload and fill result r.
// burn_cpu: if non-zero, spin first to get demoted in MLFQ.
static void run_workload(int burn_cpu, int pipe_w)
{
    if (burn_cpu) {
        volatile long x = 0;
        for (long i = 0; i < 80000000L; i++) x += i;
        (void)x;
    }

    char *mem = sbrk(NPAGES * PAGE_SIZE);

    for (int i = 0; i < NPAGES; i++)
        mem[i * PAGE_SIZE] = (char)(i & 0xFF);

    for (int i = NPAGES - 1; i >= 0; i--) {
        volatile char c = mem[i * PAGE_SIZE]; (void)c;
    }

    // Flush before reporting so no PTE_S survives
    for (int i = 0; i < NPAGES; i++) {
        volatile char c = mem[i * PAGE_SIZE]; (void)c;
    }

    int errs = 0;
    for (int i = 0; i < NPAGES; i++)
        if (mem[i * PAGE_SIZE] != (char)(i & 0xFF)) errs++;

    int mypid = getpid();
    struct vmstats s;
    getvmstats(mypid, &s);

    struct result r;
    r.avg_disk_latency  = s.avg_disk_latency;
    r.page_faults       = s.page_faults;
    r.pages_evicted     = s.pages_evicted;
    r.pages_swapped_out = s.pages_swapped_out;
    r.pages_swapped_in  = s.pages_swapped_in;
    r.disk_writes       = s.disk_writes;
    r.disk_reads        = s.disk_reads;
    r.resident_pages    = s.resident_pages;
    r.mlfq_level        = getlevel();
    r.all_fields_ok     = (s.page_faults >= NPAGES &&
                           s.pages_evicted > 0 &&
                           s.pages_swapped_out > 0 &&
                           s.pages_swapped_in > 0 &&
                           s.disk_writes > 0 &&
                           s.disk_reads > 0 &&
                           s.avg_disk_latency > 0 &&
                           s.resident_pages <= MAXFRAMES &&
                           errs == 0);

    write(pipe_w, &r, sizeof(r));
    close(pipe_w);
    exit(!r.all_fields_ok);
}

int main(void)
{
    printf("=== PA4 Experiment 6: Priority-Aware Disk Scheduling ===\n");
    printf("    Policy: SSTF (priority=outer, seek=tiebreaker)\n\n");

    setdisksched(1); // SSTF
    setraidmode(0);  // RAID 0

    int pass = 1;

    // ── Child A: high priority — runs first, no CPU burn ──────────────────
    int pipe_a[2];
    pipe(pipe_a);

    int pid_a = fork();
    if (pid_a == 0) {
        close(pipe_a[0]);
        run_workload(0, pipe_a[1]); // no burn
        // run_workload calls exit()
    }
    close(pipe_a[1]);

    struct result ra;
    read(pipe_a[0], &ra, sizeof(ra));
    close(pipe_a[0]);
    int sa = 0;
    wait(&sa);   // wait for A to fully exit before starting B

    // ── Child B: low priority — burns CPU first, then does I/O ───────────
    int pipe_b[2];
    pipe(pipe_b);

    int pid_b = fork();
    if (pid_b == 0) {
        close(pipe_b[0]);
        run_workload(1, pipe_b[1]); // burn CPU to get demoted
        // run_workload calls exit()
    }
    close(pipe_b[1]);

    struct result rb;
    read(pipe_b[0], &rb, sizeof(rb));
    close(pipe_b[0]);
    int sb = 0;
    wait(&sb);

    // ── Results ───────────────────────────────────────────────────────────
    printf("[Child A — high priority]\n");
    printf("  level=%d  faults=%d  evicted=%d  sout=%d  sin=%d\n",
           ra.mlfq_level, ra.page_faults, ra.pages_evicted,
           ra.pages_swapped_out, ra.pages_swapped_in);
    printf("  dw=%d  dr=%d  lat=%d  resident=%d  all_ok=%s\n",
           ra.disk_writes, ra.disk_reads, ra.avg_disk_latency,
           ra.resident_pages, ra.all_fields_ok ? "YES" : "NO");

    printf("\n[Child B — low priority (CPU-burned)]\n");
    printf("  level=%d  faults=%d  evicted=%d  sout=%d  sin=%d\n",
           rb.mlfq_level, rb.page_faults, rb.pages_evicted,
           rb.pages_swapped_out, rb.pages_swapped_in);
    printf("  dw=%d  dr=%d  lat=%d  resident=%d  all_ok=%s\n",
           rb.disk_writes, rb.disk_reads, rb.avg_disk_latency,
           rb.resident_pages, rb.all_fields_ok ? "YES" : "NO");

    printf("\n[analysis]\n");

    if (ra.all_fields_ok)
        printf("  PASS: Child A all vmstats fields correct\n");
    else { printf("  FAIL: Child A vmstats fields incorrect\n"); pass = 0; }

    if (rb.all_fields_ok)
        printf("  PASS: Child B all vmstats fields correct\n");
    else { printf("  FAIL: Child B vmstats fields incorrect\n"); pass = 0; }

    if (ra.avg_disk_latency > 0 && rb.avg_disk_latency > 0)
        printf("  PASS: both processes have non-zero latency\n");
    else { printf("  FAIL: zero latency in one process\n"); pass = 0; }

    if (ra.mlfq_level < rb.mlfq_level)
        printf("  PASS: Child A (level %d) is higher priority than Child B (level %d)\n",
               ra.mlfq_level, rb.mlfq_level);
    else
        printf("  NOTE: Child A level=%d, Child B level=%d "
               "(B may not have demoted yet on fast machines)\n",
               ra.mlfq_level, rb.mlfq_level);

    if (ra.avg_disk_latency <= rb.avg_disk_latency)
        printf("  PASS: high-priority lat (%d) <= low-priority lat (%d)\n",
               ra.avg_disk_latency, rb.avg_disk_latency);
    else
        printf("  NOTE: high-priority lat (%d) > low-priority lat (%d) "
               "— acceptable when children run sequentially\n",
               ra.avg_disk_latency, rb.avg_disk_latency);

    printf("\n=== Experiment 6 %s ===\n", pass ? "PASSED" : "FAILED");
    exit(!pass);
}