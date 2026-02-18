#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    // Test 1: Basic Call
    printf("A1 Test 1: hello() basic call\n");
    hello();

    // Test 2: Test return value of hello
    printf("A1 Test 2: Test return value of hello\n");
    int ret = hello();
    if (ret == 0)
    {
        printf("hello(): success\n");
    }
    else
    {
        printf("hello(): failed\n");
    }

    // Test 3: hello() multiple calls
    printf("A1 Test 3: hello() multiple calls\n");
    hello();
    hello();

    // Test 4: hello() from child
    printf("A1 Test 4: hello() from child\n");
    if (fork() == 0)
    {
        hello();
        exit(0);
    }
    wait(0);
    
    printf("A1 tests done\n");
    exit(0);
}