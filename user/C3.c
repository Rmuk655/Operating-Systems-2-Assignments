#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    // Test 1: getchildsyscount() basic call
    printf("C3 Test 1: getchildsyscount() basic call\n");
    printf("%d\n", getchildsyscount(2));

    // Test 2: getchildsyscount() in a child process
    printf("C3 Test 2: getsyscount() in a child process\n");

    int pid = fork();

    if (pid == 0)
    {
        getpid();
        getpid();
        getpid();
        exit(0);
    }

    pause(5);

    printf("The child system call count = %d\n", getchildsyscount(pid));

    wait(0);

    // Test 3: getchildsyscount() for invalid/non-child processes
    printf("C3 Test 3: getsyscount() for invalid/non-child processes\n");
    printf("invalid pid result = %d\n", getchildsyscount(9999));
    printf("non-child result   = %d\n", getchildsyscount(1));

    wait(0);
    printf("C3 tests done\n");

    exit(0);
}