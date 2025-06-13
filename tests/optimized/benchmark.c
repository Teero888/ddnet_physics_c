#include "../include/gamecore.h"
#include "../include/collision.h"
#include "../data.h"
#include "../utils.h"
#include <getopt.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>

#define ITERATIONS 200
#define NUM_RUNS 30
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
  printf("Benchmark the physics with single or multi-threaded execution.\n\n");
  printf("Options:\n");
  printf("  --multi            Enable multi-threaded execution with OpenMP (default: single-threaded)\n");
  printf("  --help             Display this help message and exit\n");
  printf("  --test=TEST_NAME   Specify the test to run (required)\n");
}

int main(int argc, char *argv[]) {
  int use_multi_threaded = 0;
  const char *test_name = NULL;

  // Parse command-line options
  while (1) {
    static struct option long_options[] = {{"multi", no_argument, 0, 'm'},
                                           {"help", no_argument, 0, 'h'},
                                           {"test", required_argument, 0, 't'},
                                           {0, 0, 0, 0}};
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
    case 't':
      test_name = optarg;
      break;
    default:
      printf("Unknown option. Use --help for usage.\n");
      return 1;
    }
  }

  // Ensure a test is specified
  if (test_name == NULL) {
    printf("Error: No test specified. Use --test=<test_name>\n");
    printf("Available tests:\n");
    for (int i = 0; i < sizeof(s_aTests) / sizeof(s_aTests[0]); i++) {
      printf("  %s: %s\n", s_aTests[i].m_Name, s_aTests[i].m_Description);
    }
    return 1;
  }

  // Find the selected test
  const STest *selected_test = NULL;
  for (int i = 0; i < sizeof(s_aTests) / sizeof(s_aTests[0]); i++) {
    if (strcmp(s_aTests[i].m_Name, test_name) == 0) {
      selected_test = &s_aTests[i];
      break;
    }
  }
  if (selected_test == NULL) {
    printf("Error: Test '%s' not found.\n", test_name);
    printf("Available tests:\n");
    for (int i = 0; i < sizeof(s_aTests) / sizeof(s_aTests[0]); i++) {
      printf("  %s: %s\n", s_aTests[i].m_Name, s_aTests[i].m_Description);
    }
    return 1;
  }

  const SValidation *selected_validation = selected_test->m_pValidationData;
  int ticks_per_iteration = selected_validation->m_Ticks;
  int total_ticks = ITERATIONS * ticks_per_iteration;

  SCollision Collision;
  char aMapPath[64];
  snprintf(aMapPath, 64, "maps/%s", selected_validation->m_aMapName);
  if (!init_collision(&Collision, aMapPath))
    return 1;
  SConfig Config;
  init_config(&Config);

  SWorldCore StartWorld;
  wc_init(&StartWorld, &Collision, &Config);
  wc_add_character(&StartWorld);
  for (int t = 0; t < 50; ++t)
    wc_tick(&StartWorld);

  // Run benchmark
  double aTPSValues[NUM_RUNS];
  printf("Benchmarking test: %s - %s\n", selected_test->m_Name, selected_test->m_Description);
  printf("Mode: %s-threaded\n", use_multi_threaded ? "multi" : "single");
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
        for (int t = 0; t < ticks_per_iteration; ++t) {
          cc_on_input(&World.m_pCharacters[0], &selected_validation->m_vStates[0][t].m_Input);
          wc_tick(&World);
        }
        wc_free(&World);
      }
      ElapsedTime = omp_get_wtime() - StartTime;
    } else {
      StartTime = omp_get_wtime();
      for (int i = 0; i < ITERATIONS; ++i) {
        SWorldCore World = (SWorldCore){};
        wc_copy_world(&World, &StartWorld);
        for (int t = 0; t < ticks_per_iteration; ++t) {
          cc_on_input(&World.m_pCharacters[0], &selected_validation->m_vStates[0][t].m_Input);
          wc_tick(&World);
        }
        wc_free(&World);
      }
      ElapsedTime = omp_get_wtime() - StartTime;
    }

    aTPSValues[run] = (double)total_ticks / ElapsedTime;
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
