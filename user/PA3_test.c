// PA3 Test: Scheduler-Aware Page Replacement in xv6
//
// Covers all required experiments:
//   1. Allocating large memory regions using sbrk()
//   2. Triggering page faults
//   3. Forcing page replacement (eviction)
//   4. Verifying correct eviction behavior (data integrity)
//   5. Demonstrating lower-priority processes lose pages earlier
//   6. Reusing previously evicted pages (swap-in)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

struct vmstats {
  int page_faults;
  int pages_evicted;
  int pages_swapped_in;
  int pages_swapped_out;
  int resident_pages;
};

#define PGSIZE 4096

static void
print_stats(int pid, const char *label)
{
  struct vmstats s;
  if (getvmstats(pid, &s) < 0) {
    printf("  getvmstats failed for pid=%d\n", pid);
    return;
  }
  printf("  [%s] faults=%d evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
         label,
         s.page_faults, s.pages_evicted,
         s.pages_swapped_out, s.pages_swapped_in,
         s.resident_pages);
}

// -----------------------------------------------------------------------
// Experiment 1 + 2 + 3:
//   Allocate large region via sbrk(), access pages sequentially (triggering
//   page faults), and once the 64-frame limit is hit, page replacement kicks
//   in automatically.
// -----------------------------------------------------------------------
static void
test_sbrk_faults_and_replacement(void)
{
  printf("\n=== Exp 1-3: sbrk + page faults + replacement ===\n");
  int pid = getpid();

  // Exp 1: Allocate large region with lazy sbrk (no physical pages yet).
  int npages = 80;          // deliberately exceeds MAX_USER_FRAMES (64)
  char *buf = sbrklazy(npages * PGSIZE);
  if (buf == (char*)-1) { printf("sbrklazy failed\n"); return; }
  printf("  Allocated %d pages via sbrklazy() -- no physical pages yet\n", npages);
  print_stats(pid, "after sbrklazy (before any access)");

  // Exp 2: Access each page sequentially -- each access is a page fault.
  printf("  Touching all %d pages sequentially...\n", npages);
  for (int i = 0; i < npages; i++)
    buf[i * PGSIZE] = (char)(i & 0xff);

  print_stats(pid, "after sequential write (faults + evictions expected)");
  // Expect: faults=80, evicted=16 (80-64), resident=64

  // Exp 3 verification: eviction count should equal npages - MAX_USER_FRAMES.
  struct vmstats s;
  getvmstats(pid, &s);
  int expected_evictions = npages - 64; // 64 = MAX_USER_FRAMES
  printf("  Expected evictions >= %d, got %d -- %s\n",
         expected_evictions, s.pages_evicted,
         s.pages_evicted >= expected_evictions ? "PASS" : "FAIL");
}

// -----------------------------------------------------------------------
// Experiment 4: Verify correct eviction behavior -- data written before
//               eviction must be readable after swap-in.
// -----------------------------------------------------------------------
static void
test_data_integrity(void)
{
  printf("\n=== Exp 4: Correct eviction behavior (data integrity) ===\n");
  int pid = getpid();
  struct vmstats before, after;
  getvmstats(pid, &before);

  // Allocate another 80-page region to force more evictions.
  int npages = 80;
  char *buf = sbrklazy(npages * PGSIZE);
  if (buf == (char*)-1) { printf("sbrklazy failed\n"); return; }

  // Write distinct values to every page.
  for (int i = 0; i < npages; i++)
    buf[i * PGSIZE] = (char)((i * 7) & 0xff);

  printf("  Wrote to %d pages. Reading back (forces swap-in of evicted pages)...\n", npages);
  int errors = 0;
  for (int i = 0; i < npages; i++) {
    if (buf[i * PGSIZE] != (char)((i * 7) & 0xff))
      errors++;
  }

  getvmstats(pid, &after);
  printf("  Data integrity: %d errors (expected 0) -- %s\n",
         errors, errors == 0 ? "PASS" : "FAIL");
  printf("  Swap-ins during read-back: %d\n",
         after.pages_swapped_in - before.pages_swapped_in);
  print_stats(pid, "after integrity check");
}

// -----------------------------------------------------------------------
// Experiment 5: Lower-priority processes lose pages earlier.
//
// Design:
//   - CPU-bound child: spins to get demoted to MLFQ level > 0, fills
//     ~50 frames with its own pages.
//   - Then the parent (level 0, interactive) allocates pages, triggering
//     evictions.  The clock algorithm prefers the CPU-bound child's pages
//     (lower priority = higher MLFQ level = evicted first).
//   - Result: CPU-bound child has higher pages_evicted than the parent.
// -----------------------------------------------------------------------
static void
test_priority_eviction(void)
{
  printf("\n=== Exp 5: Lower-priority process loses pages earlier ===\n");

  // Run this experiment in a fresh child so it doesn't inherit the large
  // working set from exps 1-4.
  int outer = fork();
  if (outer != 0) { wait(0); return; }

  // ---- We are the fresh outer child ----

  // Step A: Spawn the CPU-bound child.  It spins then allocates pages.
  int cpu_child = fork();
  if (cpu_child == 0) {
    // Spin to get demoted to MLFQ level >= 2.
    volatile long x = 0;
    for (long i = 0; i < 4000000L; i++) x++;

    int level = getlevel();
    printf("  CPU-bound child: MLFQ level=%d (expect > 0)\n", level);

    // Allocate and touch 50 pages (fills frame table partially).
    int npages = 50;
    char *buf = sbrklazy(npages * PGSIZE);
    if (buf != (char*)-1)
      for (int i = 0; i < npages; i++) buf[i * PGSIZE] = (char)(i & 0xff);

    print_stats(getpid(), "cpu-bound child (before parent pressure)");

    // Wait for parent to apply memory pressure, then report final stats.
    pause(8);
    print_stats(getpid(), "cpu-bound child (after parent pressure)");
    exit(0);
  }

  // Step B: Parent waits briefly for the child to spin & fill pages.
  pause(3);

  // Step C: Parent (stays at level 0 -- interactive) allocates 50 more pages,
  // exceeding the 64-frame limit.  Clock will prefer evicting the child's
  // lower-priority pages.
  printf("  Parent: MLFQ level=%d (expect 0)\n", getlevel());
  int npages = 50;
  char *buf = sbrklazy(npages * PGSIZE);
  if (buf != (char*)-1)
    for (int i = 0; i < npages; i++) buf[i * PGSIZE] = (char)(i & 0xff);

  print_stats(getpid(), "parent after allocation (fewer evictions expected)");

  // Give the child time to print its stats.
  pause(2);
  wait(0);

  printf("  Exp 5 key: CPU-bound child should show more evictions than parent.\n");
  exit(0);   // exit outer fork child
}

// -----------------------------------------------------------------------
// Experiment 6: Reusing previously evicted pages (swap-in).
//   Explicitly shows that pages written before eviction are correctly
//   restored when accessed again after being swapped out.
// -----------------------------------------------------------------------
static void
test_swap_reuse(void)
{
  printf("\n=== Exp 6: Reusing previously evicted pages (swap-in) ===\n");
  int pid = getpid();

  // Allocate exactly MAX_USER_FRAMES+1 = 65 pages.
  // The 65th page access triggers eviction of the 1st page.
  int npages = 65;
  char *buf = sbrklazy(npages * PGSIZE);
  if (buf == (char*)-1) { printf("sbrklazy failed\n"); return; }

  // Write a known pattern to ALL pages (causes the first to be evicted).
  for (int i = 0; i < npages; i++)
    buf[i * PGSIZE] = (char)(i & 0xff);

  struct vmstats s1;
  getvmstats(pid, &s1);
  printf("  After writing %d pages: swapped_out=%d\n", npages, s1.pages_swapped_out);

  // Re-access page 0 -- it was the first evicted, so it must be swapped in.
  struct vmstats before;
  getvmstats(pid, &before);

  char val = buf[0];   // triggers swap-in of page 0
  (void)val;

  struct vmstats after;
  getvmstats(pid, &after);
  int swapin_delta = after.pages_swapped_in - before.pages_swapped_in;
  printf("  Re-accessed evicted page 0: swap-ins delta=%d (expect >= 1) -- %s\n",
         swapin_delta, swapin_delta >= 1 ? "PASS" : "FAIL");

  // Verify data is correct after swap-in.
  int errors = 0;
  for (int i = 0; i < npages; i++) {
    if (buf[i * PGSIZE] != (char)(i & 0xff)) errors++;
  }
  printf("  Data after swap-in: %d errors (expected 0) -- %s\n",
         errors, errors == 0 ? "PASS" : "FAIL");
  print_stats(pid, "final");
}

int
main(void)
{
  printf("PA3: Scheduler-Aware Page Replacement -- All Experiments\n");
  printf("  MAX_USER_FRAMES=64, SWAP_SLOTS=512\n");

  test_sbrk_faults_and_replacement();   // Exp 1 + 2 + 3
  test_data_integrity();                // Exp 4
  test_priority_eviction();             // Exp 5
  test_swap_reuse();                    // Exp 6

  printf("\nAll experiments complete.\n");
  exit(0);
}
