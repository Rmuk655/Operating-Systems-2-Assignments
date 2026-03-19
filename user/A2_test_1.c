#include "kernel/types.h"
#include "user/user.h"

void busy_loop()
{
    volatile unsigned long x = 0;
    while (1)
    {
        x++; // prevent compiler optimization
    }
}

void print_info(int pid, char *name)
{
    struct mlfqinfo info;
    if (getmlfqinfo(pid, &info) == 0)
    {
        printf("%s (PID %d)\n", name, pid);
        printf("  Level: %d\n", getlevel());
        printf("  Schedules: %d\n", info.times_scheduled);
        printf("  Syscalls: %d\n", info.total_syscalls);
        printf("  Ticks: [%d, %d, %d, %d]\n",
               info.ticks[0],
               info.ticks[1],
               info.ticks[2],
               info.ticks[3]);
    }
    else
    {
        printf("%s (PID %d) info unavailable\n", name, pid);
    }
}

int main()
{
    printf("\n=== CPU-BOUND MLFQ STRESS TEST ===\n\n");

    int p1 = fork();
    if (p1 == 0)
        busy_loop();

    int p2 = fork();
    if (p2 == 0)
        busy_loop();

    int p3 = fork();
    if (p3 == 0)
        busy_loop();

    int p4 = fork();
    if (p4 == 0)
        busy_loop();

    // Let them run and sample 5 times
    for (int k = 0; k < 5; k++)
    {
        // small delay
        for (volatile int i = 0; i < 100000000; i++)
            ;

        printf("\n--- SAMPLE %d ---\n", k);

        print_info(p1, "P0");
        print_info(p2, "P1");
        print_info(p3, "P2");
        print_info(p4, "P3");
    }

    printf("\n=== TERMINATING CHILDREN ===\n");

    kill(p1);
    kill(p2);
    kill(p3);
    kill(p4);

    wait(0);
    wait(0);
    wait(0);
    wait(0);

    printf("\n=== TEST COMPLETE ===\n");
    exit(0);
}