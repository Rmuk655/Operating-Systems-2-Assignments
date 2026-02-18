#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    // Test 1: getsyscount() basic call
    printf("C1 Test 1: getsyscount() basic call\n");
    printf("%d\n", getsyscount());

    // Test 2: Checking and verifying getsyscount() before and after a write call
    printf("C1 Test 2: Checking and verifying getsyscount() before and after a write call\n");

    int before = getsyscount();

    getpid();
    getpid();
    write(1, "", 0);

    int after = getsyscount();

    printf("Syscount before = %d\n", before);
    printf("Syscount after  = %d\n", after);
    printf("Syscount difference between before and after  = %d\n", after - before);

    printf("C1 tests done\n");
    exit(0);
}