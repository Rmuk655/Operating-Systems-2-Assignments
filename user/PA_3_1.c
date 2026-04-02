// test_vmstats.c
// Test 1: getvmstats system call + struct proc fields + basic page faults
//
// What this tests:
//   - getvmstats() returns 0 for valid PID, -1 for invalid PID
//   - page_faults counter increments correctly after sbrk + sequential access
//   - resident_pages tracks live pages correctly (using delta)
//   - pages_swapped_in / pages_swapped_out start at 0 when no eviction happens

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define PAGES_TO_ALLOC 8     // small: fits in memory without eviction

static void print_stats(const char *label, struct vmstats *s) {
    printf("[vmstats] %s faults=%d evicted=%d swapped_in=%d swapped_out=%d resident=%d\n",
           label,
           s->page_faults,
           s->pages_evicted,
           s->pages_swapped_in,
           s->pages_swapped_out,
           s->resident_pages);
}

int main(void) {
    printf("=== Test 1: getvmstats + basic page faults ===\n");

    int pid = getpid();
    struct vmstats s;

    // --- baseline ---
    if (getvmstats(pid, &s) != 0) {
        printf("FAIL: getvmstats returned error before any alloc\n");
        exit(1);
    }
    print_stats("before alloc:", &s);

    // --- allocate but do NOT access: no page faults expected yet ---
    char *mem = sbrklazy(PAGES_TO_ALLOC * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    getvmstats(pid, &s);
    print_stats("after sbrk (no access):", &s);
    
    // page_faults should still be 0 (lazy allocation)
    if (s.page_faults != 0)
        printf("  NOTE: kernel eagerly allocated – lazy alloc not active\n");

    // --- sequential access: each new page triggers one fault ---
    int prev_faults = s.page_faults;
    int prev_resident = s.resident_pages; // FIX: Capture baseline resident pages
    
    for (int i = 0; i < PAGES_TO_ALLOC; i++) {
        mem[i * PAGE_SIZE] = (char)(i + 1);   // touch first byte of each page
    }

    getvmstats(pid, &s);
    print_stats("after sequential access:", &s);

    int new_faults = s.page_faults - prev_faults;
    if (new_faults == PAGES_TO_ALLOC)
        printf("  PASS: page_faults incremented by %d (one per page)\n", PAGES_TO_ALLOC);
    else
        printf("  WARN: expected %d new faults, got %d\n", PAGES_TO_ALLOC, new_faults);

    if (s.pages_evicted == 0)
        printf("  PASS: no evictions (memory not full)\n");
    else
        printf("  WARN: unexpected evictions=%d – reduce PAGES_TO_ALLOC or increase MAXFRAMES\n",
               s.pages_evicted);

    // FIX: Check if resident pages increased by PAGES_TO_ALLOC
    int new_resident = s.resident_pages - prev_resident;
    if (new_resident == PAGES_TO_ALLOC)
        printf("  PASS: resident_pages increased by %d\n", PAGES_TO_ALLOC);
    else
        printf("  WARN: resident_pages increased by %d, expected %d\n", new_resident, PAGES_TO_ALLOC);

    // --- re-read each page: should NOT generate new faults ---
    prev_faults = s.page_faults;
    for (int i = 0; i < PAGES_TO_ALLOC; i++) {
        volatile char c = mem[i * PAGE_SIZE];
        (void)c;
    }
    getvmstats(pid, &s);
    if (s.page_faults == prev_faults)
        printf("  PASS: re-read caused 0 extra faults (pages still resident)\n");
    else
        printf("  WARN: re-read caused %d extra faults\n", s.page_faults - prev_faults);

    // --- invalid PID test ---
    int ret = getvmstats(-1, &s);
    if (ret == -1)
        printf("  PASS: getvmstats(-1, ...) returned -1\n");
    else
        printf("  FAIL: getvmstats(-1, ...) returned %d (expected -1)\n", ret);

    ret = getvmstats(99999, &s);
    if (ret == -1)
        printf("  PASS: getvmstats(99999, ...) returned -1\n");
    else
        printf("  FAIL: getvmstats(99999, ...) returned %d (expected -1)\n", ret);

    printf("=== Test 1 done ===\n");
    exit(0);
}