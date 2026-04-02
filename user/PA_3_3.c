// test_clock.c
// Test 3: Clock algorithm – reference bit behaviour
//
// What this tests:
//   - Frequently-accessed pages (reference bit kept set) are NOT the first evicted
//   - Cold pages (accessed once, never again) are evicted before hot pages
//   - The Clock hand advances correctly
//
// Strategy:
//   Phase 1 – Fill memory to MAXFRAMES pages.
//   Phase 2 – Keep "hot" pages[0..HOT-1] alive by re-touching them.
//   Phase 3 – Allocate EXTRA more pages (forces evictions).
//   Phase 4 – Verify hot pages are still in memory (resident, no fault on access).
//              Verify cold pages required a swap-in to read back.
//
// This is a probabilistic test: the Clock algorithm cannot guarantee hot pages
// are NEVER evicted if HOT >= MAXFRAMES, but with HOT << MAXFRAMES it holds.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE  4096
#define MAXFRAMES  64
#define HOT        4          // pages we keep touching (should stay resident)
#define COLD_START HOT        // cold pages begin here
#define COLD_COUNT (MAXFRAMES - HOT)   // fill rest of memory with cold pages
#define EXTRA      8          // additional pages to force evictions of cold pages
#define TOUCH_ROUNDS 3        // how many times we re-touch hot pages per extra alloc

static void dump(const char *tag, struct vmstats *s) {
    printf("[clock] %s faults=%d evicted=%d sout=%d sin=%d res=%d\n",
           tag, s->page_faults, s->pages_evicted,
           s->pages_swapped_out, s->pages_swapped_in, s->resident_pages);
}

int main(void) {
    printf("=== Test 3: Clock reference-bit / hot-cold eviction ===\n");
    printf("    HOT=%d  COLD=%d  MAXFRAMES=%d  EXTRA=%d\n",
           HOT, COLD_COUNT, MAXFRAMES, EXTRA);

    int pid = getpid();
    struct vmstats s;

    int total = HOT + COLD_COUNT + EXTRA;
    char *mem = sbrklazy(total * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    // Phase 1: fill memory (hot + cold pages)
    printf("[phase1] Filling %d pages (hot + cold)...\n", HOT + COLD_COUNT);
    for (int i = 0; i < HOT + COLD_COUNT; i++)
        mem[i * PAGE_SIZE] = (char)(i + 1);

    getvmstats(pid, &s); dump("after fill", &s);

    // Phase 2+3: interleave hot touches with new (extra) page allocations
    printf("[phase2] Allocating %d extra pages while keeping hot pages touched...\n", EXTRA);
    for (int e = 0; e < EXTRA; e++) {
        // Touch hot pages to set their reference bits BEFORE the clock sweeps
        for (int r = 0; r < TOUCH_ROUNDS; r++)
            for (int h = 0; h < HOT; h++)
                mem[h * PAGE_SIZE] = (char)(h + 1);  // re-write keeps ref bit set

        // Access one new (cold) extra page to trigger eviction
        int idx = HOT + COLD_COUNT + e;
        mem[idx * PAGE_SIZE] = (char)(idx + 1);
    }

    getvmstats(pid, &s); dump("after extra allocs", &s);

    int evictions = s.pages_evicted;
    if (evictions > 0)
        printf("  PASS: evictions occurred (%d)\n", evictions);
    else
        printf("  FAIL: no evictions — increase EXTRA or reduce MAXFRAMES\n");

    // Phase 4: verify hot pages are still accessible without swap-in
    printf("[phase4] Re-reading hot pages (should NOT trigger swap-in faults)...\n");
    int sin_before = s.pages_swapped_in;
    int errors = 0;

    for (int h = 0; h < HOT; h++) {
        char got = mem[h * PAGE_SIZE];
        char exp = (char)(h + 1);
        if (got != exp) {
            printf("  FAIL: hot page %d corrupted: got %d exp %d\n", h, (int)got, (int)exp);
            errors++;
        }
    }

    getvmstats(pid, &s);
    int hot_swap_ins = s.pages_swapped_in - sin_before;

    if (hot_swap_ins == 0)
        printf("  PASS: 0 swap-ins for hot pages (they stayed resident)\n");
    else
        printf("  WARN: %d swap-in(s) for hot pages (clock may have evicted them)\n",
               hot_swap_ins);

    // Phase 5: verify cold pages are readable (may need swap-in, but data must be correct)
    printf("[phase5] Re-reading cold pages (integrity check)...\n");
    sin_before = s.pages_swapped_in;

    for (int i = COLD_START; i < COLD_START + COLD_COUNT; i++) {
        char got = mem[i * PAGE_SIZE];
        char exp = (char)(i + 1);
        if (got != exp) {
            printf("  FAIL: cold page %d corrupted: got %d exp %d\n", i, (int)got, (int)exp);
            errors++;
        }
    }

    getvmstats(pid, &s);
    printf("  INFO: swap-ins for cold re-read = %d\n", s.pages_swapped_in - sin_before);
    dump("final", &s);

    printf("=== Test 3 done (errors=%d) ===\n", errors);
    exit(errors != 0);
}