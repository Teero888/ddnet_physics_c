#include "../src/gamecore.h"
#include "collision.h"
#include "data.h"
#include "utils.h"
#include <getopt.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>

#define ITERATIONS 10000
#define TICKS_PER_ITERATION s_TestRun.m_Ticks
#define TOTAL_TICKS ITERATIONS *TICKS_PER_ITERATION
#define NUM_RUNS 10
#define BAR_WIDTH 50

typedef struct {
  double mean;
  double stddev;
  double min;
  double max;
} SStats;

static SStats calculate_stats(double *values, int count) {
  SStats stats = {0};
  double sum = 0;
  for (int i = 0; i < count; i++)
    sum += values[i];
  stats.mean = sum / count;

  double variance = 0;
  for (int i = 0; i < count; i++) {
    double diff = values[i] - stats.mean;
    variance += diff * diff;
  }
  variance /= count;
  stats.stddev = sqrt(variance);

  stats.min = values[0];
  stats.max = values[0];
  for (int i = 1; i < count; i++) {
    if (values[i] < stats.min)
      stats.min = values[i];
    if (values[i] > stats.max)
      stats.max = values[i];
  }
  return stats;
}

void print_progress(int current, int total, double elapsed_time) {
  float progress = (float)current / total;
  int pos = (int)(BAR_WIDTH * progress);

  printf("\r[");
  for (int i = 0; i < BAR_WIDTH; i++) {
    if (i < pos)
      printf("=");
    else if (i == pos)
      printf(">");
    else
      printf(" ");
  }
  printf("] %3.0f%% (Run %d/%d, %.2fs)", progress * 100, current, total, elapsed_time);
  fflush(stdout);
}

void print_help(const char *prog_name) {
  printf("Usage: %s [OPTIONS]\n", prog_name);
  printf("Benchmark the physics with single or multi-threaded "
         "execution.\n\n");
  printf("Options:\n");
  printf("  --multi    Enable multi-threaded execution with OpenMP (default: "
         "single-threaded)\n");
  printf("  --help     Display this help message and exit\n");
}

int main(int argc, char *argv[]) {
  int use_multi_threaded = 0;

  // Parse command-line options
  while (1) {
    static struct option long_options[] = {
        {"multi", no_argument, 0, 'm'}, {"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 'm':
      use_multi_threaded = 1;
      break;
    case 'h':
      print_help(argv[0]);
      return 0;
    default:
      printf("Unknown option. Use --help for usage.\n");
      return 1;
    }
  }

  SCollision Collision;
  if (!init_collision(&Collision, "maps/test_run.map"))
    return 1;
  SConfig Config;
  init_config(&Config);

  SWorldCore StartWorld;
  wc_init(&StartWorld, &Collision, &Config);
  wc_add_character(&StartWorld);
  for (int t = 0; t < 50; ++t)
    wc_tick(&StartWorld);

  double aTPSValues[NUM_RUNS];
  printf("Benchmarking in %s-threaded mode...\n", use_multi_threaded ? "multi" : "single");
  if (use_multi_threaded)
    printf("Using %d threads with OpenMP.\n", omp_get_max_threads());

  for (int run = 0; run < NUM_RUNS; run++) {
    double StartTime, ElapsedTime;

    if (use_multi_threaded) {
      StartTime = omp_get_wtime();
#pragma omp parallel for
      for (int i = 0; i < ITERATIONS; ++i) {
        SWorldCore World = (SWorldCore){};
        wc_copy_world(&World, &StartWorld);
        for (int t = 0; t < TICKS_PER_ITERATION; ++t) {
          cc_on_input(&World.m_pCharacters[0], &s_TestRun.m_vStates[0][t].m_Input);
          wc_tick(&World);
        }
        // if (!vvcmp(World.m_pCharacters[0].m_Pos, s_TestRun.m_vStates[0][TICKS_PER_ITERATION - 1].m_Pos) ||
        //     !vvcmp(World.m_pCharacters[0].m_Vel, s_TestRun.m_vStates[0][TICKS_PER_ITERATION - 1].m_Vel)) {
        //   printf("Run not valid.\n");
        //   exit(1);
        // }
        wc_free(&World);
      }
      ElapsedTime = omp_get_wtime() - StartTime;
    } else {
      StartTime = omp_get_wtime();
      for (int i = 0; i < ITERATIONS; ++i) {
        SWorldCore World = (SWorldCore){};
        wc_copy_world(&World, &StartWorld);
        for (int t = 0; t < TICKS_PER_ITERATION; ++t) {
          cc_on_input(&World.m_pCharacters[0], &s_TestRun.m_vStates[0][t].m_Input);
          wc_tick(&World);
        }
        // if (!vvcmp(World.m_pCharacters[0].m_Pos, s_TestRun.m_vStates[0][TICKS_PER_ITERATION - 1].m_Pos) ||
        //     !vvcmp(World.m_pCharacters[0].m_Vel, s_TestRun.m_vStates[0][TICKS_PER_ITERATION - 1].m_Vel)) {
        //   printf("Run not valid.\n");
        //   exit(1);
        // }
        wc_free(&World);
      }
      ElapsedTime = omp_get_wtime() - StartTime;
    }

    aTPSValues[run] = (double)TOTAL_TICKS / ElapsedTime;
    print_progress(run + 1, NUM_RUNS, ElapsedTime);
  }
  printf("\n");

  SStats stats = calculate_stats(aTPSValues, NUM_RUNS);

  char aBuf[32];
  char aBuff[32];
  format_int((int)stats.mean, aBuf);
  format_int((int)stats.stddev, aBuff);
  printf("TPS (mean ± σ):\t\t%s ± %s ticks/s\n", aBuf, aBuff);
  format_int((int)stats.min, aBuf);
  printf("Range (min … max):\t%s … ", aBuf);
  format_int((int)stats.max, aBuf);
  printf("%s ticks/s\t%d runs\n", aBuf, NUM_RUNS);

  wc_free(&StartWorld);
  free_collision(&Collision);

  return 0;
}
