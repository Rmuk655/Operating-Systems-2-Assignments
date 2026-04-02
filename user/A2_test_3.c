#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void cpu_worker()
{
    int pid = getpid();
    volatile unsigned long x = 0;

    for (int round = 0; round < 30; round++)
    {
        for (int i = 0; i < 50000000; i++)
        {
            x += i;
        }

        // printf("[CPU %d] FINAL level=%d\n", pid, getlevel());
    }

    // Only print ONCE at end
    printf("[CPU %d] FINAL level=%d\n", pid, getlevel());

    struct mlfqinfo info;
    if (getmlfqinfo(pid, &info) == 0)
    {
        printf("\n[CPU %d FINAL STATS]\n", pid);
        printf("Level: %d\n", info.level);
        printf("Times scheduled: %d\n", info.times_scheduled);
        printf("Syscalls: %d\n", info.total_syscalls);
        for (int i = 0; i < 4; i++)
            printf("Ticks at level %d: %d\n", i, info.ticks[i]);
    }

    exit(0);
}

void interactive_worker()
{
    int pid = getpid();

    for (int i = 0; i < 20; i++)
    {
        pause(2); // syscall heavy
        printf("[INT %d] level=%d\n", pid, getlevel());
    }

    struct mlfqinfo info;
    if (getmlfqinfo(pid, &info) == 0)
    {
        printf("\n[INT %d FINAL STATS]\n", pid);
        printf("Level: %d\n", info.level);
        printf("Times scheduled: %d\n", info.times_scheduled);
        printf("Syscalls: %d\n", info.total_syscalls);
        for (int i = 0; i < 4; i++)
            printf("Ticks at level %d: %d\n", i, info.ticks[i]);
    }

    exit(0);
}

int main()
{
    printf("=== SC-MLFQ VERIFICATION START ===\n\n");

    // spawn 3 CPU-bound processes
    for (int i = 0; i < 4; i++)
    {
        if (fork() == 0)
        {
            cpu_worker();
        }
    }

    // spawn 2 interactive process
    for (int i = 0; i < 2; i++)
    {
        if (fork() == 0)
        {
            interactive_worker();
        }
    }

    // wait for all children
    for (int i = 0; i < 6; i++)
        wait(0);

    printf("\n=== VERIFICATION COMPLETE ===\n");
    exit(0);
}