#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    // Test 1: getppid() basic call
    printf("B1 Test 1: getppid() basic call\n");
    printf("%d\n", getppid());

    // Test 2: Comparison of getpid(), getpid2(), getppid()
    printf("B1 Test 2: Comparison of getpid(), getpid2(), getppid()\n");
    printf("PID = %d\n", getpid());
    printf("PID2 = %d\n", getpid2());
    printf("PPID = %d\n", getppid());

    // Test 3: Parent pid in child vs initial value
    printf("B1 Test 3: Parent pid in child vs initial value\n");

    int parent = getpid();
    printf("Parent PID initially = %d\n", parent);

    if (fork() == 0)
    {
        int ppid = getppid();
        printf("child pid = %d, child ppid = %d (expected %d)\n", getpid(), ppid, parent);
        exit(0);
    }

    wait(0);

    printf("B1 tests done\n");
    exit(0);
}