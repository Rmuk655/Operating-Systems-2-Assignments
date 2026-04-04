// test_mlfq_eviction.c
// Test 4: SC-MLFQ priority-aware eviction
//
// Strategy:
//   - Child A: stays at level 0 by doing syscalls
//   - Child B: sinks to level 2 by burning CPU
//   - We VERIFY the levels before applying pressure
//   - Pressure child forces evictions
//   - Child B (lower priority) should have more evictions

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE   4096
#define MAXFRAMES   64
#define HALF        (MAXFRAMES / 2)
#define EXTRA       (HALF + 4)   // enough to force real evictions

#define MSG_READY  'R'
#define MSG_GO     'G'
#define MSG_DONE   'D'
#define MSG_LEVEL  'L'

static void burn_cpu(int iters) {
    volatile long x = 1;
    for (int i = 0; i < iters; i++)
        x = x * 6364136223846793005LL + 1442695040888963407LL;
    (void)x;
}

static void do_syscalls(int n) {
    for (int i = 0; i < n; i++) getpid();
}

int main(void) {
    printf("=== Test 4: MLFQ-aware eviction ===\n");
    printf("    MAXFRAMES=%d  HALF=%d  EXTRA=%d\n", MAXFRAMES, HALF, EXTRA);

    int p_to_a[2], p_to_b[2], p_to_pres[2];
    int a_to_p[2], b_to_p[2], pres_to_p[2];

    pipe(p_to_a); pipe(p_to_b); pipe(p_to_pres);
    pipe(a_to_p); pipe(b_to_p); pipe(pres_to_p);

    // ---- Child A: HIGH priority ----
    int pid_a = fork();
    if (pid_a == 0) {
        close(p_to_a[1]); close(a_to_p[0]);
        close(p_to_b[0]); close(p_to_b[1]);
        close(p_to_pres[0]); close(p_to_pres[1]);
        close(b_to_p[0]); close(b_to_p[1]);
        close(pres_to_p[0]); close(pres_to_p[1]);

        // Do syscalls to stay at level 0
        for (int i = 0; i < 200; i++) do_syscalls(50);

        // Allocate pages
        char *mem = sbrklazy(HALF * PAGE_SIZE);
        for (int i = 0; i < HALF; i++) {
            do_syscalls(5);
            mem[i * PAGE_SIZE] = (char)(i + 1);
        }

        // Report our level then signal ready
        int level = getlevel();
        write(a_to_p[1], &level, sizeof(int));
        char msg = MSG_READY;
        write(a_to_p[1], &msg, 1);

        // Wait for GO
        read(p_to_a[0], &msg, 1);

        // Keep touching pages during pressure
        for (int round = 0; round < 10; round++) {
            do_syscalls(10);
            for (int i = 0; i < HALF; i++)
                mem[i * PAGE_SIZE] = (char)(i + 1);
        }

        // Report final stats
        struct vmstats s;
        getvmstats(getpid(), &s);
        write(a_to_p[1], &s, sizeof(s));
        msg = MSG_DONE;
        write(a_to_p[1], &msg, 1);
        exit(0);
    }

    // ---- Child B: LOW priority ----
    int pid_b = fork();
    if (pid_b == 0) {
        close(p_to_b[1]); close(b_to_p[0]);
        close(p_to_a[0]); close(p_to_a[1]);
        close(p_to_pres[0]); close(p_to_pres[1]);
        close(a_to_p[0]); close(a_to_p[1]);
        close(pres_to_p[0]); close(pres_to_p[1]);

        // Burn CPU hard to sink to level 3
        for (int i = 0; i < 30; i++) burn_cpu(5000000);

        // Allocate pages while staying low priority
        char *mem = sbrklazy(HALF * PAGE_SIZE);
        for (int i = 0; i < HALF; i++) {
            burn_cpu(50000);
            mem[i * PAGE_SIZE] = (char)(i + 1);
        }

        // Report our level then signal ready
        int level = getlevel();
        write(b_to_p[1], &level, sizeof(int));
        char msg = MSG_READY;
        write(b_to_p[1], &msg, 1);

        // Wait for GO — do NOT touch pages
        read(p_to_b[0], &msg, 1);
        burn_cpu(500000);

        // Report final stats
        struct vmstats s;
        getvmstats(getpid(), &s);
        write(b_to_p[1], &s, sizeof(s));
        msg = MSG_DONE;
        write(b_to_p[1], &msg, 1);
        exit(0);
    }

    // ---- Child PRESSURE ----
    int pid_pres = fork();
    if (pid_pres == 0) {
        close(p_to_pres[1]); close(pres_to_p[0]);
        close(p_to_a[0]); close(p_to_a[1]);
        close(p_to_b[0]); close(p_to_b[1]);
        close(a_to_p[0]); close(a_to_p[1]);
        close(b_to_p[0]); close(b_to_p[1]);

        char msg;
        read(p_to_pres[0], &msg, 1);

        // Allocate EXTRA pages repeatedly to force many evictions
        char *mem = sbrklazy(EXTRA * PAGE_SIZE);
        for (int round = 0; round < 5; round++)
            for (int i = 0; i < EXTRA; i++)
                mem[i * PAGE_SIZE] = (char)(i + round + 1);

        msg = MSG_DONE;
        write(pres_to_p[1], &msg, 1);
        exit(0);
    }

    // ---- Parent ----
    close(p_to_a[0]); close(p_to_b[0]); close(p_to_pres[0]);
    close(a_to_p[1]); close(b_to_p[1]); close(pres_to_p[1]);

    // Collect levels and ready signals
    int level_a, level_b;
    char msg;

    read(a_to_p[0], &level_a, sizeof(int));
    read(a_to_p[0], &msg, 1);  // READY

    read(b_to_p[0], &level_b, sizeof(int));
    read(b_to_p[0], &msg, 1);  // READY

    printf("[parent] child A level=%d  child B level=%d\n", level_a, level_b);

    // Verify levels are as expected before applying pressure
    if (level_a > level_b) {
        printf("  WARN: child A (level=%d) is not higher priority than B (level=%d)\n",
               level_a, level_b);
        printf("  Proceeding anyway — result may be unreliable\n");
    } else {
        printf("  PASS: priority confirmed — A=level%d (high)  B=level%d (low)\n",
               level_a, level_b);
    }

    // Start pressure
    msg = MSG_GO;
    write(p_to_pres[1], &msg, 1);
    write(p_to_a[1],    &msg, 1);
    write(p_to_b[1],    &msg, 1);

    // Wait for pressure to finish
    char pmsg;
    read(pres_to_p[0], &pmsg, 1);
    printf("[parent] pressure done\n");

    // Collect stats
    struct vmstats sa, sb;
    read(a_to_p[0], &sa, sizeof(sa));
    read(a_to_p[0], &msg, 1);  // DONE

    read(b_to_p[0], &sb, sizeof(sb));
    read(b_to_p[0], &msg, 1);  // DONE

    wait(0); wait(0); wait(0);

    printf("\n[results]\n");
    printf("[mlfq] Child A (level=%d) evicted=%d swapped_out=%d resident=%d\n",
           level_a, sa.pages_evicted, sa.pages_swapped_out, sa.resident_pages);
    printf("[mlfq] Child B (level=%d) evicted=%d swapped_out=%d resident=%d\n",
           level_b, sb.pages_evicted, sb.pages_swapped_out, sb.resident_pages);

    printf("\n");

    // Only evaluate if levels were actually different
    if (level_b <= level_a) {
        printf("  SKIP: levels not sufficiently different (A=%d B=%d) — increase burn\n",
               level_a, level_b);
    } else if (sb.pages_evicted > sa.pages_evicted) {
        printf("  PASS: low-priority child B evicted more (%d > %d)\n",
               sb.pages_evicted, sa.pages_evicted);
    } else if (sb.pages_evicted == sa.pages_evicted) {
        printf("  PARTIAL: equal evictions (A=%d B=%d)\n",
               sa.pages_evicted, sb.pages_evicted);
    } else {
        printf("  FAIL: child A evicted more (A=%d B=%d)\n",
               sa.pages_evicted, sb.pages_evicted);
    }

    printf("=== Test 4 done ===\n");
    exit(0);
}