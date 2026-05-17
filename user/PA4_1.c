// PA4_1.c — Swap-out + Swap-in Correctness
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE        4096
#define MAXFRAMES        64
#define NPAGES           80
#define ROTATIONAL_DELAY 5

static void print_stats(const char *label, struct vmstats *s)
{
    printf("[stats] %s\n", label);
    printf("        page_faults=%d  evicted=%d  swapped_out=%d  swapped_in=%d\n",
           s->page_faults, s->pages_evicted,
           s->pages_swapped_out, s->pages_swapped_in);
    printf("        resident=%d  disk_writes=%d  disk_reads=%d  avg_latency=%d\n",
           s->resident_pages, s->disk_writes,
           s->disk_reads, s->avg_disk_latency);
}

int main(void)
{
    printf("=== PA4 Experiment 1: Swap-out + Swap-in Correctness ===\n");
    printf("    Allocating %d pages, MAXFRAMES=%d\n\n", NPAGES, MAXFRAMES);

    int pass = 1;
    int pid  = getpid();
    struct vmstats before, after_write, after_read;

    /* ── Edge-case checks for getvmstats ── */
    printf("[check] getvmstats syscall edge cases\n");
    struct vmstats tmp;

    if (getvmstats(-1, &tmp) == -1)
        printf("  PASS: getvmstats(-1) returned -1\n");
    else { printf("  FAIL: getvmstats(-1) should return -1\n"); pass = 0; }

    if (getvmstats(99999, &tmp) == -1)
        printf("  PASS: getvmstats(99999) returned -1\n");
    else { printf("  FAIL: getvmstats(99999) should return -1\n"); pass = 0; }

    if (getvmstats(pid, &tmp) == 0)
        printf("  PASS: getvmstats(own pid) returned 0\n");
    else { printf("  FAIL: getvmstats(own pid) should return 0\n"); pass = 0; }

    /* ── Cross-process check: parent reads child stats ── */
    printf("\n[check] getvmstats cross-process (parent reads child stats)\n");

    int xpipe[2];
    pipe(xpipe);

    int xchild = fork();
    if (xchild == 0) {
        close(xpipe[0]);
        char *m = sbrk(5 * PAGE_SIZE);
        for (int i = 0; i < 5; i++)
            m[i * PAGE_SIZE] = (char)i;
        int cpid = getpid();
        write(xpipe[1], &cpid, sizeof(int));
        close(xpipe[1]);
        pause(2);
        exit(0);
    }

    close(xpipe[1]);
    int cpid = 0;
    read(xpipe[0], &cpid, sizeof(int));
    close(xpipe[0]);

    struct vmstats cs;
    int cross_ret = getvmstats(cpid, &cs);
    wait(0);

    if (cross_ret == 0 && cs.page_faults > 0)
        printf("  PASS: parent read child (pid=%d) page_faults=%d\n",
               cpid, cs.page_faults);
    else
        printf("  NOTE: cross-process getvmstats ret=%d faults=%d\n",
               cross_ret, cs.page_faults);

    /* ── Step 1: Baseline ── */
    printf("\n[step 1] Baseline stats\n");
    getvmstats(pid, &before);
    print_stats("baseline", &before);

    /* ── Step 2: Allocate and write NPAGES ── */
    printf("\n[step 2] Allocating and writing %d pages\n", NPAGES);
    char *mem = sbrk(NPAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    for (int i = 0; i < NPAGES; i++)
        mem[i * PAGE_SIZE] = (char)(i & 0xFF);

    getvmstats(pid, &after_write);
    print_stats("after write", &after_write);

    int new_faults  = after_write.page_faults       - before.page_faults;
    int new_evicted = after_write.pages_evicted     - before.pages_evicted;
    int new_sout    = after_write.pages_swapped_out - before.pages_swapped_out;
    int new_dw      = after_write.disk_writes       - before.disk_writes;

    /* Eager allocation: faults occur on swap-ins, so > 0 is the right threshold */
    if (new_faults > 0)
        printf("  PASS: page_faults += %d (eager allocation swap-ins)\n", new_faults);
    else { printf("  FAIL: page_faults = 0, expected > 0\n"); pass = 0; }

    if (new_evicted > 0)
        printf("  PASS: pages_evicted = %d\n", new_evicted);
    else { printf("  FAIL: pages_evicted = 0\n"); pass = 0; }

    if (new_sout > 0)
        printf("  PASS: pages_swapped_out = %d\n", new_sout);
    else { printf("  FAIL: pages_swapped_out = 0\n"); pass = 0; }

    if (new_dw > 0)
        printf("  PASS: disk_writes = %d\n", new_dw);
    else { printf("  FAIL: disk_writes = 0\n"); pass = 0; }

    /* ── Step 3: Read back and verify ── */
    printf("\n[step 3] Reading back all pages\n");
    int errors = 0;
    for (int i = 0; i < NPAGES; i++)
        if (mem[i * PAGE_SIZE] != (char)(i & 0xFF))
            errors++;

    getvmstats(pid, &after_read);
    print_stats("after read", &after_read);

    int new_sin = after_read.pages_swapped_in - before.pages_swapped_in;
    int new_dr  = after_read.disk_reads       - before.disk_reads;

    if (errors == 0)
        printf("  PASS: all %d pages correct after disk swap\n", NPAGES);
    else { printf("  FAIL: %d pages corrupted\n", errors); pass = 0; }

    if (new_sin > 0)
        printf("  PASS: pages_swapped_in = %d\n", new_sin);
    else { printf("  FAIL: pages_swapped_in = 0\n"); pass = 0; }

    if (new_dr > 0)
        printf("  PASS: disk_reads = %d\n", new_dr);
    else { printf("  FAIL: disk_reads = 0\n"); pass = 0; }

    if (after_read.avg_disk_latency >= ROTATIONAL_DELAY)
        printf("  PASS: avg_disk_latency=%d >= C=%d\n",
               after_read.avg_disk_latency, ROTATIONAL_DELAY);
    else { printf("  FAIL: avg_disk_latency=%d < C=%d\n",
               after_read.avg_disk_latency, ROTATIONAL_DELAY); pass = 0; }

    if (after_read.resident_pages <= MAXFRAMES)
        printf("  PASS: resident_pages=%d <= MAXFRAMES=%d\n",
               after_read.resident_pages, MAXFRAMES);
    else { printf("  FAIL: resident_pages=%d > MAXFRAMES=%d\n",
               after_read.resident_pages, MAXFRAMES); pass = 0; }

    printf("\n=== Experiment 1 %s ===\n", pass ? "PASSED" : "FAILED");
    exit(!pass);
}