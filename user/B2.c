#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    // Test 1: getnumchild() basic call
    printf("B2 Test 1: getnumchild() basic call\n");
    printf("%d\n", getnumchild());

    // Test 2: getnumchild() while forking parent process and reaping the children
    printf("B2 Test 2: getnumchild() while forking parent process and reaping the children\n");
    printf("Number of children before fork = %d\n", getnumchild());

    if (fork() == 0)
    {
        pause(20);
        exit(0);
    }

    if (fork() == 0)
    {
        pause(20);
        exit(0);
    }

    pause(5);

    printf("Number of children after fork = %d\n", getnumchild());

    wait(0);
    wait(0);

    printf("Number of children after wait = %d\n", getnumchild());

    exit(0);
}