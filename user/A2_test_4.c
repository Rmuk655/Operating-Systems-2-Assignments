#include "kernel/types.h"
#include "user/user.h"

void PrintMLFQ(struct mlfqinfo info){
    printf("MLFQ INFO:\n");
    printf("1. Level of process in MLFQ: %d\n", info.level);
    printf("2. Ticks consumed per level:\n");
    for(int i=0;i<4;i++){
        char c;
        switch(i){
            case 0:
            c = 'a';
            break;
            case 1:
            c = 'b';
            break;
            case 2:
            c = 'c';
            break;
            case 3:
            c = 'd';
            break;
        }
        printf("\t%c. At level %d: %d\n", c, i, info.ticks[i]);
    }
    printf("3. %d\n", info.times_scheduled);
    printf("4. %d\n", info.total_syscalls);
    // exit(0);
}

int main(int argc, char** argv){
    int num_times = 0;
    if(argc >= 2){
        num_times = atoi(argv[1]);
    }
    for(int i=0;i<num_times;i++){
        hello();
    }
    struct mlfqinfo info1;
    struct mlfqinfo info2;
    struct mlfqinfo info3;
    struct mlfqinfo info4;
    struct mlfqinfo info5;
    int pid = getpid();
    int get;
    // get = getmlfqinfo(pid, &info);
    // int cpid = fork();
    // if(cpid==0){
    //     // int start = getsyscount();
    //     hello();
    //     hello();
    //      hello();
    //      hello();
    //      // int end = getsyscount();
    //     exit(0);
    // } 
    // else{
    //      // pause(10);
    // wait(0);
    // }
    // PrintMLFQ(info);
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    get = getmlfqinfo(pid, &info1);
    if(get < 0){
        printf("Retrieving info failed.\n");
        exit(0);
    }
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    get = getmlfqinfo(pid, &info2);
    if(get < 0){
        printf("Retrieving info failed.\n");
        exit(0);
    }
    for(int i=0;i<2000000000;i++);
    for(int i=0;i<2000000000;i++);
    get = getmlfqinfo(pid, &info3);
    if(get < 0){
        printf("Retrieving info failed.\n");
        exit(0);
    }
    // for(int i=0;i<2000000000;i++);
    // for(int i=0;i<2000000000;i++);
    // // get = getmlfqinfo(pid, &info1);
    // // if(get < 0){
        // //     printf("Retrieving info failed.\n");
        // //     exit(0);
        // // }
        // // for(int i=0;i<100000000;i++);
        // // for(int i=0;i<2000000000;i++);
        // // for(int i=0;i<2000000000;i++);
        // //   2147483647
        // get = getmlfqinfo(pid, &info4);
        // if(get < 0){
            //     printf("Retrieving info failed.\n");
            //     exit(0);
            // }
            // PrintMLFQ(info4);
            int cpid = fork();
            if(cpid<0){
        printf("Fork failed\n");
        exit(0);
    }
    else if (cpid == 0){
        // hello();
        int pid = getpid();
        for(int i=0;i<1000000000;i++);
        // Uncomment this to test interactive
        // for(int i=0;i<200000;i++){
            //     pid = getpid();
            //     pid = getpid();
            //     pid = getpid();
            // }    
        // Uncomment this to test CPU-bound
        // for(int i=0;i<2000000000;i++);
        // for(int i=0;i<2000000000;i++);
        // for(int i=0;i<2000000000;i++);
        get = getmlfqinfo(pid, &info4);
        if(get < 0){
            printf("Retrieving info failed.\n");
            exit(0);
        }
        // PrintMLFQ(info4);
        exit(0);
    }
    else{
        for(int i=0;i<2000000000;i++);
        for(int i=0;i<2000000000;i++);
        get = getmlfqinfo(getpid(), &info5);
        if(get < 0){
            printf("Retrieving info failed.\n");
            exit(0);
        }
        wait(0);
    }
    PrintMLFQ(info1);
    PrintMLFQ(info2);
    PrintMLFQ(info3);
    PrintMLFQ(info5);
    // exit(0);
}   