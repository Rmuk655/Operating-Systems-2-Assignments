## Operating Systems
# Programming Assignment 2: System-Call-Aware Multi-Level Feedback Queue Scheduler in xv6

### Objective

The objective of the assignment is to extend the features of the xv6 operating system by adding a System-Call-Aware Multi-Level Feedback Queue Scheduler and 2 system calls related to MLFQ level of the process in the MLFQ scheduler implemented, get MLFQ info using a struct mlfqinfo defined in types.h which stores information about the MLFQ like level, ticks[4], times_scheduled, total_syscalls.
The Scheduler and the system calls were implemented in the xv6 kernel and validated using user level test programs.

------------------------------------------------------------------------

## Implemented System Calls

### 1. `getlevel()`
**Description**
Returns the current MLFQ queue level of the calling process.
**Return Value**
-   Integer between **0 and 3**
-   `0` → Highest priority queue\
-   `3` → Lowest priority queue
**Implementation**
-   Implemented `sys_getlevel()` in `sysproc.c`.
-   Uses `myproc()` to retrieve the calling process.
-   Returns the value of `p->cur_queue_lvl`.
Example usage: printf("Current Level: %d\n", getlevel());

### 2. `getmlfqinfo(int pid, struct mlfqinfo *info)`
**Description**
Returns scheduling statistics of a process with the given PID.
The kernel fills the following structure:
    struct mlfqinfo {
        int level;                 // current queue level
        int ticks[4];              // total ticks consumed at each level
        int times_scheduled;       // number of times scheduled
        int total_syscalls;        // total system calls executed
    };
**Return Value**
-   `0` on success\
-   `-1` if the PID is invalid
**Implementation**
The system call:
1.  Searches the process table for the specified PID
2.  Copies scheduling statistics from the kernel process structure
3.  Returns them to user space

------------------------------------------------------------------------

### Files modified 

1. kernel/defs.h
2. kernel/proc.c
3. kernel/proc.h
4. kernel/syscall.c
5. kernel/syscall.h
6. kernel/sysproc.c
7. kernel/trap.c
8. kernel/types.h
9. user/user.h
10. user/usys.pl
11. Makefile

------------------------------------------------------------------------

### Test files added

1. user/A2_test_1.c
2. user/A2_test_2.c
3. user/A2_test_3.c
4. user/README.md

------------------------------------------------------------------------

### Folder structure

CS24BTECH11036
├── kernel
│   ├── defs.h
│   ├── proc.c
│   ├── proc.h
│   ├── syscall.c
│   ├── syscall.h
│   ├── sysproc.c
│   ├── trap.c
│   └── types.h
│
├── user
│   ├── user.h
│   ├── usys.pl
│   ├── README.md
│   ├── A2_test_1.c
│   ├── A2_test_2.c
│   └── A2_test_3.c
│
└── Makefile

------------------------------------------------------------------------

### Approach

The System-Call-Aware Multi-Level Feedback Queue Scheduler was implemented the following way:
1. In proc.h in the struct proc, I added variables signifying MLFQ parameters like cur_queue_lvl, ticks_each_level, ticks[4], no_of_schedules, temp.
2. The scheduler iterates over the four queue levels and selects RUNNABLE processes using a round-robin policy within each level in the Scheduler. We use the Round-Robin scheduling logic in each loop, which simulates each Queue level individually.
3. Added a function named priority_boost() which shifts all RUNNABLE processes to level 0 after every 128 ticks. I added its function definition in the file defs.h.
4. I added the definition of the struct mlfqinfo in the file types.h.
5. Defined an extern int time_slice[4] variable in trap.c that keeps track of the value of ΔT for each level of the MLFQ.
6. Modified the usertrap(void) function in trap.c, specifically in the if (which_dev == 2) block, to implement the timer-based scheduling logic. The code ensures that the OS takes away the CPU time from time consuming processes after they have consumed the time slot given to them and given to another process of the same level.
7. The code above also performs global boost after 128 ticks using the if (no_of_ticks - last_boost >= 128) condition.
8. We use variables to keep track of the number of ticks consumed using time_slice[4], the number of syscalls using the syscall counter inside the struct proc and the variable last_boost which tracks the last time when global boost occurred.

Each system call was implemented by following using the same steps as used in Assignment 1. 

User-level test programs are written for every system call and executed from the xv6 shell to validate the correctness of the system call defined.

Process-related system calls traverse the process table using proper locking to ensure kernel safety. This is done by the appropriate use of acquire(&pi->lock), release(&pi->lock) and acquire(&wait->lock), release(&wait->lock).

------------------------------------------------------------------------

# Design Decisions and Assumptions

## Queue Structure

The scheduler maintains **four priority queues**:
  Queue Level   Priority   Time Quantum
  Level 0       Highest    2 ticks
  Level 1       High       4 ticks
  Level 2       Medium     8 ticks
  Level 3       Lowest     16 ticks
The scheduler always selects a **RUNNABLE process from the highest
non-empty queue**.
Within each queue, **round-robin scheduling** is used.

## Process Accounting
Each process maintains the following fields:
-   `cur_queue_lvl` -- current queue level\
-   `ticks_each_level` -- ticks consumed in the current level\
-   `ticks[4]` -- total ticks consumed at each level\
-   `times_scheduled` -- number of times the process has been scheduled
These values are updated during **timer interrupts** and scheduling
events.

## Demotion Rule
If a process consumes its entire time quantum, it is **demoted to the
next lower queue**.
Example progression: Level 0 → Level 1 → Level 2 → Level 3
Processes already in **Level 3 remain there**.

## System-Call-Aware Scheduling
To distinguish between CPU-bound and interactive processes, the
scheduler uses:
    ΔS = system calls executed during the time slice
    ΔT = timer ticks consumed during the time slice
If ΔS ≥ ΔT, the process is considered **interactive** and **not demoted**.

## Global Priority Boost
To prevent starvation, a **global priority boost occurs every 128 timer
ticks**.
All RUNNABLE processes are moved to Level 0.

------------------------------------------------------------------------

### Assumptions

1. Even though the given design does not define a struct Queue, it simulates the working of the Queue structure with 4 queues effectively.
2. The syscount counter implemented in Assignment 1 in the struct proc keeps track of the number of syscalls correctly.
3. The processes are assumed to run for their full time quanta or until they need an I/O operation to be performed. It is assumed that processes either run for their allotted time slice or voluntarily yield the CPU due to blocking system calls (e.g., I/O).
4. The scheduler assumes the single-CPU execution model of xv6, meaning only one process runs at a time.
5. Since multiple processes print concurrently and printf() is not synchronized in xv6, output from different processes may interleave. This does not affect scheduler correctness.

------------------------------------------------------------------------

## Testing

Download the zip folder, extract it into a location and in your WSL terminal (in windows)/terminal (in linux) 
Run the following commands:
1. cd  /path/to/CS24BTECH11036
2. make clean
3. make qemu
4. ls (Check if A2_test_1.c, A2_test_2.c, A2_test_3.c exist)
5. Run the desired test program:
   A2_test_1
   A2_test_2
   A2_test_3

Alternatively you can move your test files to user, add $U/_X\, where X.c is the name of your test file in the Makefile under    $U/_A2_test_3\ and run them using the above steps.

------------------------------------------------------------------------

### Test Cases Details and their respective Experimental results
Three test programs were used to evaluate the scheduler behavior.

# Test Program 1 -- CPU-Bound Processes
## Description
This program spawns multiple CPU-bound processes that execute a busy
loop:
    while (1) {
        x++;
    }
These processes perform **no system calls**, therefore:
    ΔS = 0
    ΔT > 0
They are classified as **CPU-bound**.

## Observed Results
Example output:
    Level: 3
    Ticks: [2, 4, 8, 31]
Processes gradually demoted: Level 0 → Level 1 → Level 2 → Level 3
Tick distribution matched the expected time quanta.

## Conclusion
This confirms that **CPU-bound processes are correctly demoted to lower
priority queues**.

# Test Program 2 -- Mixed Workload
## Description
This program created three workloads:
1.  CPU-bound process
2.  Syscall-heavy process
3.  Mixed workload process
Example behavior:
    CPU-bound:
        Busy loop
    Syscall-heavy:
        repeated getpid()
    Mixed workload:
        short CPU burst + syscall burst

------------------------------------------------------------------------
## Observed Results
Example output:
    CPU-bound:
    Level 3
    Ticks: [2,4,8,471]

    Syscall-heavy:
    Level 0
    Ticks: [141,0,0,0]

    Mixed workload:
    Level 0
    Ticks: [286,0,0,0]

Observations:
-   CPU-bound processes migrated to **Level 3**
-   Syscall-heavy processes stayed in **Level 0**
-   Mixed workload remained in **Level 0**

## Conclusion
The scheduler correctly differentiates **CPU-bound vs interactive
workloads**.

# Test Program 3 -- Scheduler Verification

## Description
This program created:
-   4 CPU-bound processes
-   2 interactive processes

Interactive processes repeatedly called:
    pause()
    getlevel()

## Observed Results
Interactive processes:
    Level: 0
    Ticks: [0,0,0,0]

CPU-bound processes:
    Ticks: [2,4,8,108]

During execution:
    Promoted process PID 4 from current level 3 to level 0

Tick statistics showed:
    Level 0 ticks: 4
    Level 1 ticks: 8
    Level 2 ticks: 16

## Conclusion
This confirms:
-   Interactive processes remain at high priority
-   CPU-bound processes demote correctly
-   Global priority boost resets queues to Level 0

# Overall Results
The implemented SC-MLFQ scheduler demonstrates:
-   Correct **MLFQ demotion**
-   Correct **system-call-aware scheduling**
-   Fair **round-robin scheduling**
-   Prevention of **starvation**
-   Correct **global priority boost**

------------------------------------------------------------------------

## Responsible use of AI (ChatGPT)

I used AI tools responsibly in this assignment primarily to understand the concepts behind how the given Round-Robin Scheduler in scheduler() code in the proc.c file and the system and kernel trap codes in the trap.c file work - the whole code flow in general and specific code snippets from the existing code to understand the concept and code in depth to understand how a System-Call-Aware Multi-Level Feedback Queue Scheduler works.
I took ChatGPT's help to generate test cases for testing the system calls and the System-Call-Aware Multi-Level Feedback Queue Scheduler and added them along with my own modifications to the test cases in my test files A2_test_1, A2_test_2, A2_test_3. 
I also took the help of ChatGPT for ideas on what to write, what to points to include, etc in this README.md file.

------------------------------------------------------------------------