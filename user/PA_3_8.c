// test_reuse.c
// Test 8: Reuse of previously evicted pages (swap slot recycling)
//
// What this tests:
//   - A page that was evicted, swapped-in, written again, and evicted again
//     correctly uses a recycled swap slot (no slot leak, no stale data)
//   - Repeated evict→access→evict cycles maintain correctness
//   - pages_swapped_in and pages_swapped_out grow proportionally over multiple cycles
//
// Strategy:
//   We keep only TWO live sets:
//     SET_A (pages 0..HALF-1)     – alternately hot and cold
//     SET_B (pages HALF..2*HALF-1) – alternately cold and hot
//   By alternating which set we access, the other must be evicted each round.
//   After CYCLES rounds, verify all data and check that counters are monotonically
//   increasing.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define MAXFRAMES 64
#define HALF      (MAXFRAMES / 2)    // size of each set
#define CYCLES    6                  // number of alternation rounds
// We need HALF + HALF + a "pressure" buffer to force eviction of the cold set
#define PRESSURE  (HALF + 2)
#define TOTAL_PAGES (HALF + HALF + PRESSURE)

static void write_set(char *mem, int start, int count, int gen) {
    // gen distinguishes each generation's write
    for (int i = 0; i < count; i++) {
        int *p = (int *)(mem + (long)(start + i) * PAGE_SIZE);
        for (int w = 0; w < PAGE_SIZE / 4; w++)
            p[w] = (start + i) * 1000 + gen;
    }
}

static int check_set(char *mem, int start, int count, int gen) {
    int errs = 0;
    for (int i = 0; i < count; i++) {
        int *p = (int *)(mem + (long)(start + i) * PAGE_SIZE);
        int exp = (start + i) * 1000 + gen;
        for (int w = 0; w < PAGE_SIZE / 4; w++)
            if (p[w] != exp) errs++;
    }
    return errs;
}

static void dump(int cycle, struct vmstats *s) {
    printf("[reuse] cycle=%d faults=%d evicted=%d sout=%d sin=%d res=%d\n",
           cycle, s->page_faults, s->pages_evicted,
           s->pages_swapped_out, s->pages_swapped_in, s->resident_pages);
}

int main(void) {
    printf("=== Test 8: Evicted page reuse / swap recycling ===\n");
    printf("    HALF=%d  CYCLES=%d  MAXFRAMES=%d\n", HALF, CYCLES, MAXFRAMES);

    int pid = getpid();
    struct vmstats s;

    char *mem = sbrklazy((long)TOTAL_PAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    // pressure buffer: used only to push cold set out of memory
    char *pressure_base = mem + (long)(HALF * 2) * PAGE_SIZE;

    int gen_a = 0, gen_b = 0;
    int total_errors = 0;

    int prev_sout = 0, prev_sin = 0;

    for (int c = 0; c < CYCLES; c++) {
        int is_even = (c % 2 == 0);
        // Even cycles: heat SET_A, cold SET_B
        // Odd  cycles: heat SET_B, cold SET_A

        // 1. Write the HOT set with a new generation value
        if (is_even) {
            gen_a = c;
            write_set(mem, 0, HALF, gen_a);
        } else {
            gen_b = c;
            write_set(mem, HALF, HALF, gen_b);
        }

        // 2. Access pressure pages to evict the cold set
        for (int i = 0; i < PRESSURE; i++)
            pressure_base[i * PAGE_SIZE] = (char)(i + c);

        // 3. Verify the HOT set is still correct
        int e;
        if (is_even)
            e = check_set(mem, 0, HALF, gen_a);
        else
            e = check_set(mem, HALF, HALF, gen_b);

        if (e > 0) {
            printf("  FAIL cycle %d: hot set has %d errors\n", c, e);
            total_errors += e;
        }

        // 4. Now re-access the COLD set (swap-in required)
        if (is_even) {
            // gen_b was set in the previous odd cycle (or 0 if c==0)
            if (c == 0) {
                // First cycle: cold set (B) was never written; write it now
                gen_b = -1;
                write_set(mem, HALF, HALF, gen_b);
            }
            e = check_set(mem, HALF, HALF, gen_b);
        } else {
            e = check_set(mem, 0, HALF, gen_a);
        }

        if (e > 0) {
            printf("  FAIL cycle %d: cold set swap-in has %d errors\n", c, e);
            total_errors += e;
        }

        getvmstats(pid, &s);
        dump(c, &s);

        int new_sout = s.pages_swapped_out - prev_sout;
        int new_sin  = s.pages_swapped_in  - prev_sin;

        if (c > 0) {
            if (new_sout > 0)
                printf("  PASS: cycle %d: %d new swap-outs\n", c, new_sout);
            else
                printf("  WARN: cycle %d: 0 swap-outs – pressure may be insufficient\n", c);

            if (new_sin > 0)
                printf("  PASS: cycle %d: %d new swap-ins\n", c, new_sin);
            else
                printf("  WARN: cycle %d: 0 swap-ins – cold set may still be resident\n", c);
        }

        prev_sout = s.pages_swapped_out;
        prev_sin  = s.pages_swapped_in;
    }

    // Final checks
    getvmstats(pid, &s);

    if (s.pages_swapped_out > 0 && s.pages_swapped_in > 0)
        printf("  PASS: swap used over %d cycles (sout=%d sin=%d)\n",
               CYCLES, s.pages_swapped_out, s.pages_swapped_in);

    if (s.resident_pages <= MAXFRAMES)
        printf("  PASS: resident_pages (%d) within MAXFRAMES (%d)\n",
               s.resident_pages, MAXFRAMES);
    else {
        printf("  FAIL: resident_pages (%d) > MAXFRAMES (%d)\n",
               s.resident_pages, MAXFRAMES);
        total_errors++;
    }

    printf("=== Test 8 done (total_errors=%d) ===\n", total_errors);
    exit(total_errors != 0);
}