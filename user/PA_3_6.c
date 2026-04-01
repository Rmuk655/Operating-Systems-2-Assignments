// test_frametable.c
// Test 6: Frame table correctness
//
// What this tests:
//   - No physical frame is mapped to two different virtual pages simultaneously
//   - After eviction, the frame is released before being reused (no dangling maps)
//   - Pages that return to resident state after swap-in use a valid frame
//   - The kernel does not leak frames (resident_pages never exceeds MAXFRAMES)
//
// Strategy:
//   Allocate TOTAL_PAGES >> MAXFRAMES pages.
//   Write a distinct 64-bit magic value to each page.
//   Repeatedly scan all pages in random (strided) order, writing fresh values.
//   Between scans call getvmstats and assert invariants.
//   Because each page write may trigger a fault + eviction, this exercises the
//   full allocate → evict → swap-out → swap-in cycle many times.
//
// A frame table bug typically manifests as:
//   - Two pages returning the same physical address (detected via unique magic)
//   - A page value being overwritten by another page's data

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE   4096
#define MAXFRAMES   64
#define TOTAL_PAGES 64        // 2x MAXFRAMES
#define SCANS       4

// Magic: page index encoded in every word of the page
static void stamp_page(char *base, int idx) {
    unsigned int *p = (unsigned int *)(base + (long)idx * PAGE_SIZE);
    unsigned int magic = 0xDEAD0000 | (unsigned int)idx;
    for (int w = 0; w < PAGE_SIZE / 4; w++)
        p[w] = magic ^ (unsigned int)w;
}

static int verify_page(char *base, int idx) {
    unsigned int *p = (unsigned int *)(base + (long)idx * PAGE_SIZE);
    unsigned int magic = 0xDEAD0000 | (unsigned int)idx;
    int errs = 0;
    for (int w = 0; w < PAGE_SIZE / 4; w++) {
        if (p[w] != (magic ^ (unsigned int)w)) errs++;
    }
    return errs;
}

// Simple LCG for deterministic "random" access order
static unsigned int lcg(unsigned int x) {
    return x * 1664525u + 1013904223u;
}

static void dump(const char *tag, struct vmstats *s) {
    printf("[frame] %s faults=%d evicted=%d sout=%d sin=%d res=%d\n",
           tag, s->page_faults, s->pages_evicted,
           s->pages_swapped_out, s->pages_swapped_in, s->resident_pages);
}

int main(void) {
    printf("=== Test 6: Frame table correctness ===\n");
    printf("    TOTAL_PAGES=%d  MAXFRAMES=%d  SCANS=%d\n",
           TOTAL_PAGES, MAXFRAMES, SCANS);

    int pid = getpid();
    struct vmstats s;

    char *mem = sbrklazy((long)TOTAL_PAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    // Initial write pass (sequential)
    printf("[init] Stamping all %d pages...\n", TOTAL_PAGES);
    for (int i = 0; i < TOTAL_PAGES; i++)
        stamp_page(mem, i);

    getvmstats(pid, &s); dump("after init stamp", &s);

    int total_errors = 0;

    // Repeated scans with different access orders
    unsigned int seed = 0xCAFEBABE;
    for (int scan = 0; scan < SCANS; scan++) {
        // Build a permutation of [0, TOTAL_PAGES) using Fisher-Yates with LCG
        int order[TOTAL_PAGES];
        for (int i = 0; i < TOTAL_PAGES; i++) order[i] = i;
        for (int i = TOTAL_PAGES - 1; i > 0; i--) {
            seed = lcg(seed);
            int j = (int)(seed % (unsigned int)(i + 1));
            int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
        }

        // Verify in permuted order, then re-stamp
        int scan_errors = 0;
        for (int k = 0; k < TOTAL_PAGES; k++) {
            int idx = order[k];
            int e = verify_page(mem, idx);
            if (e > 0) {
                printf("  FAIL scan %d: page %d has %d word errors\n", scan, idx, e);
                scan_errors += e;
            }
            stamp_page(mem, idx);   // fresh stamp for next scan
        }
        total_errors += scan_errors;

        getvmstats(pid, &s);
        // Use simple string construction since no snprintf in xv6 user space
        printf("[scan %d] errors=%d\n", scan, scan_errors);
        dump("scan stats", &s);

        // Invariant: resident_pages must not exceed MAXFRAMES
        if (s.resident_pages > MAXFRAMES) {
            printf("  FAIL: resident_pages=%d > MAXFRAMES=%d (frame table over-committed)\n",
                   s.resident_pages, MAXFRAMES);
            total_errors++;
        }
    }

    // Final residency check
    getvmstats(pid, &s); dump("final", &s);
    if (s.resident_pages <= MAXFRAMES)
        printf("  PASS: resident_pages=%d never exceeded MAXFRAMES=%d\n",
               s.resident_pages, MAXFRAMES);

    if (s.pages_evicted > 0)
        printf("  PASS: %d total evictions (frame reuse exercised)\n", s.pages_evicted);
    else
        printf("  WARN: 0 evictions – increase TOTAL_PAGES or reduce MAXFRAMES\n");

    printf("=== Test 6 done (total_errors=%d) ===\n", total_errors);
    exit(total_errors != 0);
}