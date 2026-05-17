// PA4_7.c — All RAID modes side-by-side with full vmstats check
// Each mode verifies every vmstats field

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE        4096
#define NDISKS           4
#define DISK_BLOCKS      512
#define MAXFRAMES        64
#define NPAGES           90

static void print_mapping(int mode)
{
    if (mode == 0) {
        printf("  Mapping: b → disk=b%%%d, phys=b/%d\n", NDISKS, NDISKS);
        printf("  Page i → logical blocks [i*4..i*4+3] striped across 4 disks\n");
    } else if (mode == 1) {
        printf("  Mapping: pair=(b/%d)%%2, disk_a=pair*2, disk_b=pair*2+1\n",
               DISK_BLOCKS);
        printf("  pair0=(disk0,disk1) blocks 0..%d | pair1=(disk2,disk3) blocks %d..%d\n",
               DISK_BLOCKS-1, DISK_BLOCKS, 2*DISK_BLOCKS-1);
    } else {
        printf("  Mapping: parity_disk=b%%%d, data_disk=b%%3 (skip parity)\n", NDISKS);
        printf("  phys_block=b/3  |  parity = XOR of all data blocks\n");
    }
}

static int run_mode(int mode, const char *name, int seed)
{
    printf("\n[%s] mode=%d\n", name, mode);
    print_mapping(mode);

    setraidmode(mode);
    setdisksched(0); // FCFS

    int pid = getpid();
    struct vmstats before, after;
    getvmstats(pid, &before);

    char *mem = sbrk(NPAGES * PAGE_SIZE);
    if (mem == (char *)-1) { printf("FAIL: sbrk\n"); exit(1); }

    for (int i = 0; i < NPAGES; i++)
        for (int b = 0; b < PAGE_SIZE; b++)
            mem[i * PAGE_SIZE + b] = (char)((i * seed + b) & 0xFF);

    for (int i = NPAGES-1; i >= 0; i--) {
        volatile char c = mem[i * PAGE_SIZE]; (void)c;
    }

    int errors = 0;
    for (int i = 0; i < NPAGES; i++)
        for (int b = 0; b < PAGE_SIZE; b++) {
            char exp = (char)((i * seed + b) & 0xFF);
            if (mem[i * PAGE_SIZE + b] != exp) errors++;
        }

    getvmstats(pid, &after);
    int df  = after.page_faults     - before.page_faults;
    int de  = after.pages_evicted   - before.pages_evicted;
    int dso = after.pages_swapped_out - before.pages_swapped_out;
    int dsi = after.pages_swapped_in  - before.pages_swapped_in;
    int ddw = after.disk_writes      - before.disk_writes;
    int ddr = after.disk_reads       - before.disk_reads;
    int lat = after.avg_disk_latency;
    int res = after.resident_pages;

    printf("  faults+=%d  evicted+=%d  sout+=%d  sin+=%d\n",
           df, de, dso, dsi);
    printf("  dw+=%d  dr+=%d  lat=%d  resident=%d  errors=%d\n",
           ddw, ddr, lat, res, errors);

    int ok = 1;
    if (errors == 0) printf("  PASS: data integrity\n");
    else { printf("  FAIL: %d errors\n", errors); ok = 0; }
    if (df >= 0) printf("  PASS: page_faults >= 0\n");
    else { printf("  FAIL: page_faults += %d\n", df); ok = 0; }
    if (de > 0) printf("  PASS: pages_evicted > 0\n");
    else { printf("  FAIL: pages_evicted = 0\n"); ok = 0; }
    if (dso > 0) printf("  PASS: pages_swapped_out > 0\n");
    else { printf("  FAIL: pages_swapped_out = 0\n"); ok = 0; }
    if (dsi > 0) printf("  PASS: pages_swapped_in > 0\n");
    else { printf("  FAIL: pages_swapped_in = 0\n"); ok = 0; }
    if (ddw > 0) printf("  PASS: disk_writes > 0\n");
    else { printf("  FAIL: disk_writes = 0\n"); ok = 0; }
    if (ddr > 0) printf("  PASS: disk_reads > 0\n");
    else { printf("  FAIL: disk_reads = 0\n"); ok = 0; }
    if (lat >= 5) printf("  PASS: avg_disk_latency=%d >= C=5\n", lat);
    else { printf("  FAIL: avg_disk_latency=%d < 5\n", lat); ok = 0; }
    if (res <= MAXFRAMES) printf("  PASS: resident_pages=%d <= MAXFRAMES\n", res);
    else { printf("  FAIL: resident_pages=%d > MAXFRAMES\n", res); ok = 0; }

    sbrk(-(NPAGES * PAGE_SIZE));
    return ok;
}

int main(void)
{
    printf("=== PA4 Experiment 7: All RAID Modes — Full vmstats Check ===\n");
    printf("    Every field of getvmstats verified for each RAID mode\n");

    int pass = 1;
    pass &= run_mode(0, "RAID 0 (striping)",         7);
    pass &= run_mode(1, "RAID 1 (mirroring)",        11);
    pass &= run_mode(2, "RAID 5 (striping+parity)",  13);

    printf("\n[summary]\n");
    printf("  RAID 0: best throughput, no redundancy\n");
    printf("  RAID 1: full redundancy, 2x disk writes\n");
    printf("  RAID 5: 1-disk fault tolerance, XOR parity\n");

    printf("\n=== Experiment 7 %s ===\n", pass ? "PASSED" : "FAILED");
    exit(!pass);
}