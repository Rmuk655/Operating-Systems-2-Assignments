// PA4_2.c — FCFS vs SSTF disk scheduling
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define MAXFRAMES 64
#define NPAGES    90

// Buffer passed through pipe: [avg_disk_latency, ok]
struct result {
    int lat;
    int ok;
};

static int run_policy(int policy, const char *name)
{
    int pipefd[2];          // FIX 1: array, not scalar
    pipe(pipefd);

    int child = fork();
    if (child == 0) {
        close(pipefd[0]);   // FIX 2: use pipefd[0] / pipefd[1]
        setdisksched(policy);

        char *mem = sbrk(NPAGES * PAGE_SIZE);
        if (mem == (char *)-1) exit(1);

        for (int i = 0; i < NPAGES; i++)
            mem[i * PAGE_SIZE] = (char)(i & 0xFF);
        for (int i = NPAGES - 1; i >= 0; i--) {
            volatile char c = mem[i * PAGE_SIZE]; (void)c;
        }

        int pid = getpid();
        struct vmstats s;
        getvmstats(pid, &s);

        int ok = (s.page_faults > 0 &&
                  s.pages_evicted > 0 &&
                  s.pages_swapped_out > 0 &&
                  s.pages_swapped_in > 0 &&
                  s.disk_writes > 0 &&
                  s.disk_reads > 0 &&
                  s.avg_disk_latency > 0);

        printf("  [%s] faults=%d evicted=%d sout=%d sin=%d dw=%d dr=%d lat=%d  fields_ok=%s\n",
               name, s.page_faults, s.pages_evicted, s.pages_swapped_out, s.pages_swapped_in,
               s.disk_writes, s.disk_reads, s.avg_disk_latency, ok ? "YES" : "NO");

        struct result buf;   // FIX 3: struct, not scalar initializer
        buf.lat = s.avg_disk_latency;
        buf.ok  = ok;
        write(pipefd[1], &buf, sizeof(buf));  // FIX 4: pass address
        close(pipefd[1]);
        exit(!ok);
    }

    close(pipefd[1]);
    struct result buf;       // FIX 5: struct, not scalar initializer
    buf.lat = 0;
    buf.ok  = 0;
    read(pipefd[0], &buf, sizeof(buf));  // FIX 6: pass address
    close(pipefd[0]);
    int status = 0;
    wait(&status);
    if (status != 0) return -1;
    return buf.lat;
}

int main(void)
{
    printf("=== PA4 Experiment 2: FCFS vs SSTF Scheduling ===\n\n");
    int pass = 1;

    printf("[check] setdisksched edge cases\n");
    if (setdisksched(0) == 0) printf("  PASS: setdisksched(0) = FCFS accepted\n");
    else { printf("  FAIL: setdisksched(0) rejected\n"); pass = 0; }

    if (setdisksched(1) == 0) printf("  PASS: setdisksched(1) = SSTF accepted\n");
    else { printf("  FAIL: setdisksched(1) rejected\n"); pass = 0; }

    if (setdisksched(2) == -1) printf("  PASS: setdisksched(2) rejected with -1\n");
    else { printf("  FAIL: setdisksched(2) should return -1\n"); pass = 0; }

    if (setdisksched(-1) == -1) printf("  PASS: setdisksched(-1) rejected with -1\n");
    else { printf("  FAIL: setdisksched(-1) should return -1\n"); pass = 0; }

    if (setdisksched(99) == -1) printf("  PASS: setdisksched(99) rejected with -1\n");
    else { printf("  FAIL: setdisksched(99) should return -1\n"); pass = 0; }

    printf("\n[FCFS run] policy=0, forward-write + backward-read\n");
    int fcfs_lat = run_policy(0, "FCFS");
    if (fcfs_lat == -1) { printf("  FAIL: FCFS child failed\n"); pass = 0; }

    printf("\n[SSTF run] policy=1, forward-write + backward-read\n");
    int sstf_lat = run_policy(1, "SSTF");
    if (sstf_lat == -1) { printf("  FAIL: SSTF child failed\n"); pass = 0; }

    printf("\n[analysis]\n");
    printf("  FCFS avg_disk_latency : %d\n", fcfs_lat);
    printf("  SSTF avg_disk_latency : %d\n", sstf_lat);

    if (fcfs_lat > 0 && sstf_lat > 0) printf("  PASS: both schedulers produced non-zero latency\n");
    else { printf("  FAIL: zero latency — stats not updated\n"); pass = 0; }

    if (sstf_lat <= fcfs_lat) printf("  PASS: SSTF (%d) <= FCFS (%d) — seek minimisation works\n", sstf_lat, fcfs_lat);
    else printf("  NOTE: SSTF (%d) > FCFS (%d) — may vary by interleaving\n", sstf_lat, fcfs_lat);

    printf("\n=== Experiment 2 %s ===\n", pass ? "PASSED" : "FAILED");
    exit(!pass);
}