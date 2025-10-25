#include "../utils.h"
#include <ddnet_physics/collision.h>
#include <ddnet_physics/vmath.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>

#define ITERATIONS 3000
#define TICKS_PER_ITERATION 3000
#define TOTAL_TICKS (ITERATIONS * TICKS_PER_ITERATION)
#define NUM_RUNS 10
#define BAR_WIDTH 50

typedef struct {
  double mean;
  double stddev;
  double min;
  double max;
} SStats;

// xorshift32
static inline unsigned int fast_rand_u32(unsigned int *state) {
  unsigned int x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static inline float fast_rand_float(unsigned int *state, float min, float max) {
  return min + (fast_rand_u32(state) / (float)UINT32_MAX) * (max - min);
}

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
  printf("Benchmark move_box with single or multi-threaded execution.\n\n");
  printf("Options:\n");
  printf("  --multi    Enable multi-threaded execution with OpenMP (default: single-threaded)\n");
  printf("  --help     Display this help message and exit\n");
}

int main(int argc, char *argv[]) {
  int use_multi_threaded = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--multi") == 0) {
      use_multi_threaded = 1;
    } else if (strcmp(argv[i], "--help") == 0) {
      print_help(argv[0]);
      return 0;
    } else if (argv[i][0] == '-') {
      printf("Unknown option: %s. Use --help for usage.\n", argv[i]);
      return 1;
    }
  }

  map_data_t Map = load_map("maps/Aip-Gores.map");
  SCollision Collision;
  if (!init_collision(&Collision, &Map)) {
    printf("Error: Failed to load collision map.\n");
    return 1;
  }
  // Map is now owned by the collision and not needed here anymore
  (void)Map;

  double aTPSValues[NUM_RUNS];
  unsigned int global_seed = 0; // (unsigned)time(NULL);

  printf("Benchmarking move_box in %s-threaded mode...\n", use_multi_threaded ? "multi" : "single");
  if (use_multi_threaded)
    printf("Using %d threads with OpenMP.\n", omp_get_max_threads());

  float max = fminf(Collision.m_MapData.width * 32.f, Collision.m_MapData.height * 32.f) - 128.f;

  for (int run = 0; run < NUM_RUNS; run++) {
    double StartTime, ElapsedTime;
    unsigned int run_seed = global_seed ^ (run * 0x9E3779B9u);

    if (use_multi_threaded) {
      StartTime = omp_get_wtime();
#pragma omp parallel for
      for (int i = 0; i < ITERATIONS; ++i) {
        unsigned int local_seed = run_seed ^ i;
        for (int t = 0; t < TICKS_PER_ITERATION; ++t) {
          // Generate synthetic random positions and velocities
          mvec2 Pos = vec2_init(fast_rand_float(&local_seed, 128.0f, max), fast_rand_float(&local_seed, 128.0f, max));
          mvec2 Vel = vec2_init(fast_rand_float(&local_seed, -32.0f, 32.0f), fast_rand_float(&local_seed, -32.0f, 32.0f));
          bool Grounded = false;
          mvec2 NewPos, NewVel;
          move_box(&Collision, Pos, Vel, &NewPos, &NewVel, vec2_init(0, 0), &Grounded);
        }
      }
      ElapsedTime = omp_get_wtime() - StartTime;
    } else {
      StartTime = omp_get_wtime();
      for (int i = 0; i < ITERATIONS; ++i) {
        unsigned int local_seed = run_seed ^ i;
        for (int t = 0; t < TICKS_PER_ITERATION; ++t) {
          mvec2 Pos = vec2_init(fast_rand_float(&local_seed, 128.0f, max), fast_rand_float(&local_seed, 128.0f, max));
          mvec2 Vel = vec2_init(fast_rand_float(&local_seed, -32.0f, 32.0f), fast_rand_float(&local_seed, -32.0f, 32.0f));
          bool Grounded = false;
          mvec2 NewPos, NewVel;
          move_box(&Collision, Pos, Vel, &NewPos, &NewVel, vec2_init(0, 0), &Grounded);
        }
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
  printf("move_box calls (mean ± σ):\t%s ± %s calls/s\n", aBuf, aBuff);
  format_int((int)stats.min, aBuf);
  printf("Range (min … max):\t\t%s … ", aBuf);
  format_int((int)stats.max, aBuf);
  printf("%s calls/s\t%d runs\n", aBuf, NUM_RUNS);

  free_collision(&Collision);
  return 0;
}
