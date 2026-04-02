## Operating Systems
# Programming Assignment 1: Extending xv6 with Custom System Calls

### Objective

The objective of the assignment is to extend the features of the xv6 operating system by adding a bunch of system calls related to process identification, parent child relationships and kernel maintained system call statistics. 
All system calls were implemented in the xv6 kernel and validated using user level test programs.

---

### Implemented System Calls

The codes for these are present in the file sysproc.c as sys_<sys_call_name>.

Part A - Warm up system calls
A1: hello()
A2: getpid2()

Part B - Process Relationships
B1: getppid()
B2: getnumchild()

Part C - System call accounting
C2: getsyscount()
C3: getchildsyscount(int pid)

---

### Files modified 

1. kernel/proc.c
2. kernel/proc.h
3. kernel/syscall.c
4. kernel/syscall.h
5. kernel/sysproc.c
6. user/user.h
7. user/usys.pl

---

### Test files added

1. user/A1.c
2. user/A2.c
3. user/B1.c
4. user/B2.c
5. user/C1.c
6. user/C2.c
7. user/C3.c

---

### Folder structure

CS24BTECH11036
├── kernel
│   ├── defs.h
│   ├── proc.c
│   ├── proc.h
│   ├── syscall.c
│   ├── syscall.h
│   └── sysproc.c
│
├── user
│   ├── user.h
│   ├── usys.pl
│   ├── README.md
│   ├── A1.c
│   ├── A2.c
│   ├── B1.c
│   ├── B2.c
│   ├── C1.c
│   ├── C2.c
│   └── C3.c
│
└── Makefile

---

### Approach

Each system call was implemented by following the standard xv6 system call flow: 
1. Adding the system call function declarations in the file user.h and added a corresponding new entry("<function_declaration_name>") in the file usys.pl.
2. Defining a system call number for each system call and added a line to the file syscall.h of the format #define SYS_<sys_call_name>           <sys_call_number>.
3. Add the lines extern uint64 sys_<sys_call_name>(arguments); and [SYS_<sys_call_name>]            sys_<sys_call_name> in the file syscall.c
4. Define the function uint64 sys_<sys_call_name>(arguments){} in the file sysproc.c. 
5. Write the corresponding code for each system call in the above function definition.
6. Additionally, create a file X.c, where X is the question number and part for the respective system call and write test cases for that system call.
7. Modify the Makefile to run the files X.c by adding $U/_X\ at the appropriate place so that QEMU recognizes this file as being compiled by the Makefile.
8. Additionally for the C part, we have to add the line int syscount; inside the struct proc definition in the file proc.h and the line  p->syscount = 0; inside the static struct proc *allocproc(void){} function. This is to keep track of the syscount() of each process.

User-level test programs are written for every system call and executed from the xv6 shell to validate the correctness of the system call defined.

Process-related system calls traverse the process table using proper locking to ensure kernel safety. This is done by the appropriate use of acquire(&pi->lock), release(&pi->lock) and acquire(&wait->lock), release(&wait->lock).

---

### Design decisions

1. A per-process system call counter was added as a field in struct proc to track the syscall count for part C.
2. The system call counter is incremented centrally in the system call dispatch path (void syscall(void){} function in syscall.c) to ensures that all system calls are counted uniformly.
3. Existing xv6 locking mechanisms were reused to protect access to shared kernel data structures in the functions int sys_getsyscount(){} and int sys_getchildsyscount(){} in the file sysproc.c
4. Zombie processes are explicitly excluded when counting child processes, as required by the specification. This is done using an if condition check if(pi->state != ZOMBIE) in the function int sys_getnumchild(){} in the file sysproc.c.

---

### Assumptions

1. The parent of a process can be determined using the parent pointer in struct proc.
2. A process without a valid parent returns -1 for getppid().
3. Only currently alive child processes/non-zombie child processes are considered for getnumchild().
4. System call counts are reset when a process is created and are not inherited by child processes.
5. Zombie processes still have a system count since they are still present in the process table. Hence, I have not added the zombie process check while implementing sys_getchildcount(){} function.

---

## Testing

Download the zip folder, extract it into a location and in your WSL terminal (in windows)/terminal (in linux) run the following commands 
1. cd  /path/to/CS24BTECH11036
2. make clean
3. make qemu
4. ls (Check if A1.c, A2.c, B1.c, B2.c, C1.c, C2.c, C3.c exist)
5. X if you are testing X.c depending on the file you to test 
Ex: X = A1/A2/B1/B2/C1/C2/C3 which tests A1/A2/B1/B2/C1/C2/C3 respectively.

Alternatively you can move your test files to user and run them using the above steps.

---

## Responsible use of AI (ChatGPT)

I have used AI very responsibly in this assignment, mainly to understand the concepts behind how a system call works - the whole code flow in general and specific code snippets from the existing code to understand the concept and code in depth. 
I asked ChatGPT to generate test cases for each part and added them along with my own test cases to my test files (which contain around 2-3 test cases each). 
I also took ChatGPT's help in making this README.md file by asking it what points/information to include.

---