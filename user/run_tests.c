// run_tests.c -- runs PA_3_1..PA_3_8 and A..G sequentially and prints a summary.
// Usage: run_tests          (inside xv6 shell)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static struct
{
  const char *name;
  const char *path;
} tests[] = {
    {"PA_3_1", "PA_3_1"},
    {"PA_3_2", "PA_3_2"},
    {"PA_3_3", "PA_3_3"},
    {"PA_3_4", "PA_3_4"},
    {"PA_3_5", "PA_3_5"},
    {"PA_3_6", "PA_3_6"},
    {"PA_3_7", "PA_3_7"},
    {"A", "A"},
    {"B", "B"},
    {"C", "C"},
    {"D", "D"},
    {"E", "E"},
    {"F", "F"},
    {"G", "G"},
};

#define NTESTS ((int)(sizeof(tests) / sizeof(tests[0])))
#define n 1

int main(void)
{
  int passed = 0, failed = 0, total = NTESTS;
  int results[NTESTS];

  printf("\n========================================\n");
  printf("   xv6 PA3 Test Runner                  \n");
  printf("========================================\n\n");

  for (int i = 0; i < total; i++)
  {
    for (int j = 0; j < n; j++)
    {
      printf("--- %s ---\n", tests[i].name);
      int pid = fork();
      if (pid < 0)
      {
        printf("  fork failed\n");
        results[i] = -1;
        failed++;
        continue;
      }
      if (pid == 0)
      {
        char *argv[2];
        argv[0] = (char *)tests[i].path;
        argv[1] = 0;
        exec(tests[i].path, argv);
        printf("  exec failed: %s\n", tests[i].path);
        exit(1);
      }

      int status = 0;
      wait(&status);
      if (status == 0)
      {
        printf("  => DONE (exit 0)\n\n");
        results[i] = 0;
        passed++;
      }
      else
      {
        printf("  => FAILED (exit %d)\n\n", status);
        results[i] = status;
        failed++;
      }
    }
  }

  printf("========================================\n");
  printf("  SUMMARY\n");
  printf("========================================\n");
  for (int i = 0; i < total; i++)
  {
    printf("  %s: %s\n",
           tests[i].name,
           results[i] == 0 ? "DONE" : "FAILED");
  }
  printf("\n  Total: %d  |  Done: %d  |  Failed: %d\n", total, passed, failed);
  printf("========================================\n\n");

  exit(0);
}
