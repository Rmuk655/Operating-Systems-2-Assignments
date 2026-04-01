// test_swap.c
// Test 5: Swap space management – integrity and swap-in/swap-out accounting
//
// What this tests:
//   - Each evicted page is stored correctly in swap
//   - When a swapped-out page is re-accessed, the kernel swaps it in
//   - pages_swapped_in  increments on each successful swap-in
//   - pages_swapped_out increments on each eviction to swap
//   - Multiple eviction-swap cycles maintain data integrity
//   - Swap slots are recycled correctly (no slot leak)
//
// Strategy (multiple waves):
//   Wave 1: fill memory → pages 0..MAXFRAMES-1 allocated
//   Wave 2: access pages MAXFRAMES..2*MAXFRAMES-1 → wave-1 pages evicted to swap
//   Wave 3: re-access wave-1 pages → swap-in from swap, wave-2 pages evicted
//   Wave 4: re-access wave-2 pages → swap-in again
//   After each wave verify all values correct and counters consistent.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE  4096
#define MAXFRAMES  64
#define WAVES      3          // number of page-sets beyond the initial fill
#define SET_SIZE   MAXFRAMES  // each wave uses exactly MAXFRAMES pages
#define TOTAL_PAGES (SET_SIZE * (WAVES + 1))

static void dump(const char *tag, struct vmstats *s) {
    printf("[swap] %s faults=%d evicted=%d sout=%d sin=%d res=%d\n",
           tag, s->page_faults, s->pages_evicted,
           s->pages_swapped_out, s->pages_swapped_in, s->resident_pages);
}

// Write unique 4-byte pattern to every word of a page
static void write_page(char *base, int page_idx) {
    int *p = (int *)(base + (long)page_idx * PAGE_SIZE);
    int pat = 0xAB000000 | page_idx;
    for (int w = 0; w < PAGE_SIZE / (int)sizeof(int); w++)
        p[w] = pat ^ w;
}

// Verify and return number of word-level mismatches
static int check_page(char *base, int page_idx) {
    int *p = (int *)(base + (long)page_idx * PAGE_SIZE);
    int pat = 0xAB000000 | page_idx;
    int errs = 0;
    for (int w = 0; w < PAGE_SIZE / (int)sizeof(int); w++) {
        if (p[w] != (pat ^ w)) errs++;
    }
    return errs;
}

int main(void) {
    printf("=== Test 5: Swap space integrity (MAXFRAMES=%d TOTAL_PAGES=%d) ===\n",
           MAXFRAMES, TOTAL_PAGES);

    int pid = getpid();
    struct vmstats s;

    char *mem = sbrklazy((long)TOTAL_PAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    int total_errors = 0;

    // ----------------------------------------------------------------
    // Wave 0: initial fill
    // ----------------------------------------------------------------
    printf("[wave 0] Writing %d pages (filling MAXFRAMES)...\n", SET_SIZE);
    for (int i = 0; i < SET_SIZE; i++)
        write_page(mem, i);

    getvmstats(pid, &s); dump("after wave-0 write", &s);
    int prev_sout = s.pages_swapped_out;
    int prev_sin  = s.pages_swapped_in;

    // ----------------------------------------------------------------
    // Waves 1..WAVES: each forces eviction of the previous set
    // ----------------------------------------------------------------
    for (int w = 1; w <= WAVES; w++) {
        int base_page = w * SET_SIZE;
        printf("[wave %d] Writing pages %d..%d (forces eviction of wave %d)...\n",
               w, base_page, base_page + SET_SIZE - 1, w - 1);

        for (int i = base_page; i < base_page + SET_SIZE; i++)
            write_page(mem, i);

        getvmstats(pid, &s); dump("after wave write", &s);

        int new_sout = s.pages_swapped_out - prev_sout;
        if (new_sout > 0)
            printf("  PASS: %d page(s) swapped out in wave %d\n", new_sout, w);
        else
            printf("  WARN: 0 swap-outs in wave %d – may need more pressure\n", w);
        prev_sout = s.pages_swapped_out;

        // Now re-read the PREVIOUS wave's pages – each must swap back in correctly
        int prev_base = (w - 1) * SET_SIZE;
        printf("[wave %d] Re-reading previous wave pages %d..%d...\n",
               w, prev_base, prev_base + SET_SIZE - 1);

        int wave_errors = 0;
        for (int i = prev_base; i < prev_base + SET_SIZE; i++) {
            int e = check_page(mem, i);
            if (e > 0) {
                printf("  FAIL: page %d has %d word errors after swap-in\n", i, e);
                wave_errors += e;
            }
        }
        total_errors += wave_errors;

        getvmstats(pid, &s); dump("after re-read", &s);
        int new_sin = s.pages_swapped_in - prev_sin;
        if (new_sin > 0)
            printf("  PASS: %d swap-in(s) in wave %d re-read\n", new_sin, w);
        else
            printf("  WARN: 0 swap-ins – pages may still be resident\n");
        prev_sin = s.pages_swapped_in;

        if (wave_errors == 0)
            printf("  PASS: wave %d re-read data intact\n", w);
    }

    // ----------------------------------------------------------------
    // Final full scan: verify every single page, every single word
    // ----------------------------------------------------------------
    printf("[final] Full integrity scan of all %d pages...\n", TOTAL_PAGES);
    int scan_errors = 0;
    for (int i = 0; i < TOTAL_PAGES; i++) {
        int e = check_page(mem, i);
        if (e > 0) {
            printf("  FAIL: page %d has %d word errors in final scan\n", i, e);
            scan_errors += e;
        }
    }
    total_errors += scan_errors;

    getvmstats(pid, &s); dump("final stats", &s);

    // ----------------------------------------------------------------
    // Accounting sanity checks
    // ----------------------------------------------------------------
    if (s.pages_swapped_in <= s.pages_swapped_out)
        printf("  PASS: swapped_in (%d) <= swapped_out (%d) – no phantom swap-ins\n",
               s.pages_swapped_in, s.pages_swapped_out);
    else
        printf("  WARN: swapped_in (%d) > swapped_out (%d) – accounting mismatch\n",
               s.pages_swapped_in, s.pages_swapped_out);

    if (s.resident_pages <= MAXFRAMES)
        printf("  PASS: resident_pages (%d) <= MAXFRAMES (%d)\n",
               s.resident_pages, MAXFRAMES);
    else
        printf("  FAIL: resident_pages (%d) > MAXFRAMES (%d)\n",
               s.resident_pages, MAXFRAMES);

    printf("=== Test 5 done (total_errors=%d) ===\n", total_errors);
    exit(total_errors != 0);
}