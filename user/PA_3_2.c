// PA_3_2.c (Test 2: Force page replacement by exceeding MAXFRAMES)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE   4096
#define MAXFRAMES   64        // must match kernel/kalloc.c MAXFRAMES
#define EXTRA       8         // pages beyond capacity to force evictions
#define TOTAL_PAGES (MAXFRAMES + EXTRA)

static void dump(const char *tag, struct vmstats *s) {
    printf("[repl] %s faults=%d evicted=%d sout=%d sin=%d resident=%d\n",
           tag,
           s->page_faults, s->pages_evicted,
           s->pages_swapped_out, s->pages_swapped_in,
           s->resident_pages);
}

int main(void) {
    printf("=== Test 2: Page replacement (MAXFRAMES=%d, TOTAL=%d) ===\n",
           MAXFRAMES, TOTAL_PAGES);

    int pid = getpid();
    struct vmstats s0, s1;

    // Capture baseline BEFORE sbrk
    if (getvmstats(pid, &s0) != 0) {
        printf("FAIL: getvmstats returned error\n");
        exit(1);
    }

    char *mem = sbrklazy(TOTAL_PAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    // Write a unique sentinel to page of each page
    printf("Writing %d pages...\n", TOTAL_PAGES);
    for (int i = 0; i < TOTAL_PAGES; i++) {
        mem[i * PAGE_SIZE] = (char)(i + 1);   // sentinel = page_index+1
    }

    getvmstats(pid, &s1);
    dump("after writing all pages", &s1);

    int evictions = s1.pages_evicted - s0.pages_evicted;
    if (evictions >= EXTRA)
        printf("  PASS: at least %d evictions occurred (%d)\n", EXTRA, evictions);
    else
        printf("  FAIL: expected >= %d evictions, got %d\n", EXTRA, evictions);

    // Compare delta of swap-outs to delta of evictions
    int swap_outs = s1.pages_swapped_out - s0.pages_swapped_out;
    if (swap_outs >= evictions)
        printf("  PASS: pages_swapped_out delta=%d >= evictions=%d\n",
               swap_outs, evictions);
    else
        printf("  WARN: swapped_out delta=%d < evictions=%d\n",
               swap_outs, evictions);

    // ----------------------------------------------------------------
    // Verify data integrity: re-read ALL pages.
    // Pages that were evicted must be swapped back in with correct data.
    // ----------------------------------------------------------------
    printf("Re-reading all %d pages (checking sentinels)...\n", TOTAL_PAGES);
    int swap_in_before = s1.pages_swapped_in;
    int errors = 0;

    for (int i = 0; i < TOTAL_PAGES; i++) {
        char expected = (char)(i + 1);
        char got = mem[i * PAGE_SIZE];
        if (got != expected) {
            printf("  FAIL: page %d: expected %d got %d\n", i, (int)expected, (int)got);
            errors++;
        }
    }

    getvmstats(pid, &s1);
    dump("after re-reading all pages", &s1);

    int swap_ins = s1.pages_swapped_in - swap_in_before;
    if (swap_ins > 0)
        printf("  PASS: %d swap-ins occurred during re-read\n", swap_ins);
    else
        printf("  WARN: 0 swap-ins – either pages still resident or stat not updated\n");

    if (errors == 0)
        printf("  PASS: all %d page sentinels intact (data survives eviction)\n", TOTAL_PAGES);
    else
        printf("  FAIL: %d data corruption(s) detected\n", errors);

    // ----------------------------------------------------------------
    // Resident pages should not exceed MAXFRAMES
    // ----------------------------------------------------------------
    if (s1.resident_pages <= MAXFRAMES)
        printf("  PASS: resident_pages=%d <= MAXFRAMES=%d\n",
               s1.resident_pages, MAXFRAMES);
    else
        printf("  FAIL: resident_pages=%d > MAXFRAMES=%d (over-committed)\n",
               s1.resident_pages, MAXFRAMES);

    printf("=== Test 2 done (errors=%d) ===\n", errors);
    exit(errors != 0);
}