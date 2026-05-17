// PA4_run.c
// PA4 Test Runner — runs all 8 PA4 experiments sequentially
// and prints a structured summary with PASS/FAIL for each.
//
// Usage (inside xv6 shell):
//   $ PA4_run
//
// Each test is exec'd in a child process so a failure in one
// test does not affect the others.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NTIMES 5

// ── Test table ────────────────────────────────────────────────────────────────
static struct
{
    const char *name;
    const char *path;
    const char *description;
} tests[] = {
    {"PA4_1", "PA4_1",
     "Swap-out + swap-in correctness (disk-backed)"},
    {"PA4_2", "PA4_2",
     "FCFS vs SSTF scheduling comparison"},
    {"PA4_3", "PA4_3",
     "RAID 0 (striping) mapping + data integrity"},
    {"PA4_4", "PA4_4",
     "RAID 1 (mirroring) mapping + data integrity"},
    {"PA4_5", "PA4_5",
     "RAID 5 (parity) multi-cycle + reconstruction"},
    {"PA4_6", "PA4_6",
     "Priority-aware disk scheduling"},
    {"PA4_7", "PA4_7",
     "All RAID modes side-by-side"},
    {"PA4_8", "PA4_8",
     "Latency model + all sched x RAID combinations"},
};

#define NTESTS ((int)(sizeof(tests) / sizeof(tests[0])))

// ── Banner helpers ─────────────────────────────────────────────────────────────
static void separator(void)
{
    printf("=================================================\n");
}

static void thin(void)
{
    printf("-------------------------------------------------\n");
}

int main(void)
{
    int results[NTESTS];
    int passed = 0, failed = 0;

    separator();
    printf("   PA4: Disk Scheduling + RAID Swap Test Runner\n");
    printf("   %d experiments\n", NTESTS);
    separator();
    printf("\n");

    // ── Run each test ─────────────────────────────────────────────────────────
    for (int i = 0; i < NTESTS; i++)
    {
        for (int j = 0; j < NTIMES; j++)
        {
            printf("[%d/%d] %s — %s\n",
                   i + 1, NTESTS,
                   tests[i].name,
                   tests[i].description);
            thin();

            int pid = fork();
            if (pid < 0)
            {
                printf("  ERROR: fork() failed\n\n");
                results[i] = -1;
                failed++;
                continue;
            }

            if (pid == 0)
            {
                // child: exec the test binary
                char *argv[2];
                argv[0] = (char *)tests[i].path;
                argv[1] = 0;
                exec(tests[i].path, argv);
                // exec only returns on failure
                printf("  ERROR: exec(%s) failed — binary missing from filesystem?\n",
                       tests[i].path);
                exit(2);
            }

            // parent: wait for child
            int status = 0;
            wait(&status);

            if (status == 0)
            {
                printf("\n  => %s: PASS\n\n", tests[i].name);
                results[i] = 0;
                passed++;
            }
            else
            {
                printf("\n  => %s: FAIL (exit code %d)\n\n",
                       tests[i].name, status);
                results[i] = status;
                failed++;
            }
        }
    }

    // ── Summary ──────────────────────────────────────────────────────────────
    separator();
    printf("  SUMMARY\n");
    separator();

    printf("  %s  %s  %s\n", "Test", "Description", "Result");
    thin();

    for (int i = 0; i < NTESTS; i++)
    {
        printf("  %s  %s  %s\n",
               tests[i].name,
               tests[i].description,
               results[i] == 0   ? "PASS"
               : results[i] == 2 ? "EXEC FAILED"
                                 : "FAIL");
    }

    thin();
    printf("  Total: %d  |  Passed: %d  |  Failed: %d\n",
           NTESTS, passed, failed);
    separator();
    printf("\n");

    // exit 0 only if all passed
    exit(failed != 0);
}