#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 1. CPU-Bound Process
// Goal: Consume entire time slices. Should rapidly demote to Level 3.
void cpu_bound() {
    volatile int i;
    for(;;) {
        // A simple busy loop that won't trigger unused variable warnings
        for(i = 0; i < 10000000; i++); 
    }
}

// 2. Syscall-Heavy (Interactive) Process
// Goal: Make many system calls to trigger the interactive retention rule.
// If Delta S >= Delta T, the process must NOT be demoted.
void syscall_heavy() {
    for(;;) {
        getpid(); // Lightweight system call to increment getsyscount()
    }
}

// 3. Mixed Workload Process
// Goal: Alternate between CPU bursts and system calls without blocking.
void mixed_workload() {
    volatile int i;
    for(;;) {
        for(i = 0; i < 50000; i++); // Short CPU burst
        for(i = 0; i < 10; i++) {   // Syscall burst
            getpid(); 
        }
    }
}

void print_stats(int pid, char* name) {
    struct mlfqinfo info;
    if(getmlfqinfo(pid, &info) == 0) {
        // Removed the file descriptor '1' from printf
        printf("[%s PID: %d] Level: %d | Ticks: [%d, %d, %d, %d] | Sched: %d | Syscalls: %d\n", 
               name, pid, info.level, info.ticks[0], info.ticks[1], info.ticks[2], info.ticks[3], 
               info.times_scheduled, info.total_syscalls);
    }
}

int main(int argc, char *argv[]) {
    printf("Starting SC-MLFQ Experimental Evaluation...\n");

    int pid_cpu = fork();
    if (pid_cpu == 0) { 
        cpu_bound(); exit(0); 
    }

    int pid_sys = fork();
    if(pid_sys == 0) {
        syscall_heavy(); exit(0); 
    }

    int pid_mixed = fork();
    if(pid_mixed == 0) {
        mixed_workload(); exit(0); 
    }

    // Parent monitors the children periodically
    for(int time = 0; time < 10; time++) {
        // Wait roughly 50 ticks before sampling again
        pause(50); 
        
        printf("\n--- Sampling Interval %d ---\n", time);
        print_stats(pid_cpu, "CPU-Bound");
        print_stats(pid_sys, "Sys-Heavy");
        print_stats(pid_mixed, "Mixed-WkL");
    }

    // Clean up
    kill(pid_cpu);
    kill(pid_sys);
    kill(pid_mixed);

    wait(0); 
    wait(0); 
    wait(0);
    
    printf("Evaluation Complete.\n");
    exit(0);
}