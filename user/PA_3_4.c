// test_mlfq_eviction.c
// Test 4: SC-MLFQ priority-aware eviction
//
// What this tests:
//   - Pages belonging to low-priority processes are preferred for eviction
//   - High-priority (interactive) processes retain their working sets
//
// Strategy:
//   - Fork two children:
//       Child A (HIGH priority): does many short system calls to stay in Q0
//       Child B (LOW  priority): does pure computation (CPU-hog) to sink to Q2
//   - Both children allocate HALF_FRAMES pages each.
//       Together they fill all MAXFRAMES.
//   - A third child (PRESSURE) allocates EXTRA pages to force evictions.
//   - After the pressure phase, each child queries its own vmstats.
//   - Expected: child B has more evictions than child A.
//
// The test uses pipes for synchronisation and shared-memory-free communication.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE   4096
#define MAXFRAMES   32
#define HALF_FRAMES (MAXFRAMES / 2)   // each child fills half
#define EXTRA       8

// pipe message tags
#define MSG_READY  'R'
#define MSG_GO     'G'
#define MSG_DONE   'D'

static void dump_child(const char *name, struct vmstats *s) {
    printf("[mlfq] %s evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
           name, s->pages_evicted, s->pages_swapped_out,
           s->pages_swapped_in, s->resident_pages);
}

// Burn CPU: used by low-priority child to exhaust its MLFQ quantum
static void burn_cpu(int iters) {
    volatile long x = 1;
    for (int i = 0; i < iters; i++) x = x * 6364136223846793005LL + 1442695040888963407LL;
    (void)x;
}

// Issue many cheap syscalls: used by high-priority child to stay in Q0
static void do_syscalls(int n) {
    for (int i = 0; i < n; i++) getpid();
}

int main(void) {
    printf("=== Test 4: MLFQ-aware eviction (high vs low priority) ===\n");
    printf("    MAXFRAMES=%d  HALF=%d  EXTRA=%d\n", MAXFRAMES, HALF_FRAMES, EXTRA);

    // Pipes: parent->children (go signal) and children->parent (ready/done + stats)
    int p_to_a[2], p_to_b[2], p_to_pres[2];
    int a_to_p[2], b_to_p[2], pres_to_p[2];

    pipe(p_to_a); pipe(p_to_b); pipe(p_to_pres);
    pipe(a_to_p); pipe(b_to_p); pipe(pres_to_p);

    // ---- Child A: HIGH priority (syscall-heavy) ----
    int pid_a = fork();
    if (pid_a == 0) {
        close(p_to_a[1]); close(a_to_p[0]);
        close(p_to_b[0]); close(p_to_b[1]);
        close(p_to_pres[0]); close(p_to_pres[1]);
        close(b_to_p[0]); close(b_to_p[1]);
        close(pres_to_p[0]); close(pres_to_p[1]);

        // Stay in high MLFQ priority by doing lots of syscalls
        for (int i = 0; i < 500; i++) do_syscalls(20);

        char *mem = sbrklazy(HALF_FRAMES * PAGE_SIZE);
        for (int i = 0; i < HALF_FRAMES; i++) {
            do_syscalls(5);              // stay high-priority while faulting
            mem[i * PAGE_SIZE] = (char)(i + 1);
        }

        // Signal ready
        char msg = MSG_READY;
        write(a_to_p[1], &msg, 1);

        // Wait for GO from parent (pressure phase started)
        read(p_to_a[0], &msg, 1);

        // Keep touching our pages during pressure phase
        for (int round = 0; round < 5; round++) {
            do_syscalls(10);
            for (int i = 0; i < HALF_FRAMES; i++)
                mem[i * PAGE_SIZE] = (char)(i + 1);
        }

        // Report stats
        struct vmstats s;
        getvmstats(getpid(), &s);
        write(a_to_p[1], &s, sizeof(s));

        msg = MSG_DONE;
        write(a_to_p[1], &msg, 1);
        exit(0);
    }

    // ---- Child B: LOW priority (CPU hog) ----
    int pid_b = fork();
    if (pid_b == 0) {
        close(p_to_b[1]); close(b_to_p[0]);
        close(p_to_a[0]); close(p_to_a[1]);
        close(p_to_pres[0]); close(p_to_pres[1]);
        close(a_to_p[0]); close(a_to_p[1]);
        close(pres_to_p[0]); close(pres_to_p[1]);

        // Burn CPU to sink to low MLFQ queue
        for (int i = 0; i < 10; i++) burn_cpu(5000000);

        char *mem = sbrklazy(HALF_FRAMES * PAGE_SIZE);
        for (int i = 0; i < HALF_FRAMES; i++) {
            burn_cpu(100000);           // stay low-priority while faulting
            mem[i * PAGE_SIZE] = (char)(i + 1);
        }

        char msg = MSG_READY;
        write(b_to_p[1], &msg, 1);

        read(p_to_b[0], &msg, 1);

        // Do NOT touch pages during pressure – simulate idle / low-priority process
        burn_cpu(1000000);

        struct vmstats s;
        getvmstats(getpid(), &s);
        write(b_to_p[1], &s, sizeof(s));

        msg = MSG_DONE;
        write(b_to_p[1], &msg, 1);
        exit(0);
    }

    // ---- Child PRESSURE: allocates extra pages to force evictions ----
    int pid_pres = fork();
    if (pid_pres == 0) {
        close(p_to_pres[1]); close(pres_to_p[0]);
        close(p_to_a[0]); close(p_to_a[1]);
        close(p_to_b[0]); close(p_to_b[1]);
        close(a_to_p[0]); close(a_to_p[1]);
        close(b_to_p[0]); close(b_to_p[1]);

        char msg;
        read(p_to_pres[0], &msg, 1);   // wait for GO

        char *mem = sbrklazy(EXTRA * PAGE_SIZE);
        for (int i = 0; i < EXTRA; i++)
            mem[i * PAGE_SIZE] = (char)(i + 1);

        msg = MSG_DONE;
        write(pres_to_p[1], &msg, 1);
        exit(0);
    }

    // ---- Parent: orchestrate ----
    close(p_to_a[0]); close(p_to_b[0]); close(p_to_pres[0]);
    close(a_to_p[1]); close(b_to_p[1]); close(pres_to_p[1]);

    // Wait for both children to finish allocating
    char msg;
    read(a_to_p[0], &msg, 1);   // child A ready
    read(b_to_p[0], &msg, 1);   // child B ready
    printf("[parent] both children allocated their pages\n");

    // Start pressure child
    msg = MSG_GO;
    write(p_to_pres[1], &msg, 1);

    // Signal children that pressure is on
    write(p_to_a[1], &msg, 1);
    write(p_to_b[1], &msg, 1);

    // Wait for pressure child to finish
    char pmsg;
    read(pres_to_p[0], &pmsg, 1);
    printf("[parent] pressure child done\n");

    // Collect stats from A
    struct vmstats sa, sb;
    read(a_to_p[0], &sa, sizeof(sa));
    read(a_to_p[0], &msg, 1);   // DONE

    // Collect stats from B
    read(b_to_p[0], &sb, sizeof(sb));
    read(b_to_p[0], &msg, 1);   // DONE

    wait(0); wait(0); wait(0);

    printf("\n[results]\n");
    dump_child("Child A (HIGH)", &sa);
    dump_child("Child B (LOW) ", &sb);

    printf("\n");
    if (sb.pages_evicted > sa.pages_evicted)
        printf("  PASS: low-priority child B evicted more pages (%d > %d)\n",
               sb.pages_evicted, sa.pages_evicted);
    else if (sb.pages_evicted == sa.pages_evicted && sb.pages_evicted > 0)
        printf("  PARTIAL: equal evictions (B=%d A=%d) – scheduler-aware eviction may need tuning\n",
               sb.pages_evicted, sa.pages_evicted);
    else
        printf("  FAIL: child A evicted more or equal (A=%d B=%d) – check MLFQ priority integration\n",
               sa.pages_evicted, sb.pages_evicted);

    printf("=== Test 4 done ===\n");
    exit(0);
}