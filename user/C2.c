#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    // Test 1: getsyscount() basic call
    printf("C2 Test 1: getsyscount() basic call\n");
    printf("Number of system calls before = %d\n", getsyscount());

    // Test 2: getsyscount() before and after 3 getpid() calls
    printf("C2 Test 2: getsyscount() before and after 3 getpid() calls\n");

    int before = getsyscount();

    getpid();
    getpid();
    getpid();

    int after = getsyscount();
    printf("Difference between the number of system calls before and after = %d\n", after - before);
    printf("Number of system calls before = %d\n", before);
    printf("Number of system calls after = %d\n", after);

    // Test 3: Syscount in parent and child processes
    printf("C2 Test 3: Syscount in parent and child processes\n");

    printf("parent count = %d\n", getsyscount());

    if (fork() == 0)
    {
        getpid();
        getpid();
        printf("child count = %d\n", getsyscount());
        exit(0);
    }

    wait(0);
    printf("parent final count = %d\n", getsyscount());

    printf("C2 tests done\n");
    exit(0);
}