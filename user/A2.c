#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    // Test 1: getpid2() basic call
    printf("A2 Test 1: getpid2() basic call\n");
    printf("%d\n", getpid2());

    // Test 2: Comparison with inbuilt getpid() system call function
    printf("A2 Test 2: Comparison with inbuilt getpid() system call function\n");
    int pid1 = getpid();
    int pid2 = getpid2();

    printf("getpid()  = %d\n", pid1);
    printf("getpid2() = %d\n", pid2);

    if (pid1 == pid2)
    {
        printf("PASS: PIDs match\n");
    }
    else
    {
        printf("FAIL: PIDs differ\n");
    }

    // Test 3: Comparison with getpid(), getpid2() values in parent and child
    printf("A2 Test 3: Comparison with inbuilt getpid() system call function\n");
    int p3 = getpid();
    int p4 = getpid2();

    printf("parent getpid  = %d\n", p3);
    printf("parent getpid2 = %d\n", p4);

    if (fork() == 0)
    {
        printf("child getpid  = %d\n", getpid());
        printf("child getpid2 = %d\n", getpid2());
        exit(0);
    }

    wait(0);

    printf("A2 tests done\n");
    exit(0);
}