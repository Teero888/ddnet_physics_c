#include "../src/gamecore.h"
#include "collision.h"
#include "data.h"
#include "utils.h"
#include <getopt.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>

#define ITERATIONS 200
#define TICKS_PER_ITERATION s_TestRun.m_Ticks
#define TOTAL_TICKS ITERATIONS *TICKS_PER_ITERATION
#define NUM_RUNS 30
#define BAR_WIDTH 50

// Real random numbers i rolled with my 2^32-1D dice
// int aRandom[256] = {
//     -1356700578, -1534797692, 992389211,   328527201,   1853856961,  308814430,   -862386912,  765185020,
//     -1703905945, -1632957813, 1015855183,  322618648,   -1194060609, 1184981114,  1781786099,  -209750241,
//     -2109315868, 304999046,   -71218228,   -1332677000, -1517659267, -381400079,  -925296344,  87095766,
//     339283821,   -1672585687, 1040251111,  -543791265,  -1353683871, 1575102271,  -433383680,  -1446246743,
//     -875041619,  859778983,   229116938,   384253642,   1415632882,  -1665132082, 71170118,    -976862025,
//     -1053281945, 1670812247,  1147000509,  -633203630,  84948221,    -431419485,  863337596,   1495072446,
//     -555035586,  -485309128,  560250587,   737571676,   382465086,   579379155,   1495362994,  -351076385,
//     1243525741,  1424770233,  524813739,   646914866,   2065268661,  1447066678,  -99013920,   -2101605456,
//     -195454754,  -275532543,  -564556462,  -174471827,  -1127714148, 1880294664,  1957664021,  -1358982860,
//     -779602382,  1799885751,  -953196595,  -183299271,  2094344853,  -1251489434, 1686214669,  -2032584968,
//     -2120903010, 1536895632,  -650299970,  1473844338,  1073022992,  -2057111859, -728586803,  -645368921,
//     -12173985,   -1683749707, -394165743,  -1814765138, 912976125,   89070460,    -51948360,   -225290749,
//     -1825314876, 589743795,   673249031,   542180148,   31770008,    830217676,   -1451652938, 1145678923,
//     -695007247,  1799446731,  695286940,   1102569694,  1170424801,  -1270551578, 681799875,   -2116053054,
//     -744687266,  -886265048,  951843215,   838236092,   1498585291,  -515554042,  1373460312,  962989362,
//     2117537655,  -687590094,  -1289250961, -287530148,  -1349080740, -2057311602, 730799782,   -750470184,
//     -1096392145, -1492846891, 459243781,   -824305796,  1806943790,  1918528686,  1844951599,  -788776831,
//     258068923,   347309741,   -1838012185, 1929152069,  919000165,   880635984,   -503351438,  1325934961,
//     1864057976,  1772148784,  -1006163451, -577293149,  511360433,   816381901,   528180588,   101745465,
//     48889424,    1054873501,  -1942448563, -1926243617, -1267642284, -1048975261, -1648205794, 400865610,
//     -229572093,  1545162595,  -1983262419, -391679113,  -2000523613, 1122410205,  -269159888,  -1014962186,
//     -1996504140, 94118698,    -468427872,  218608699,   -208636405,  409159151,   -1463068927, -1423717893,
//     1267367086,  1126281778,  1437928596,  -2112809026, 551303520,   1507640002,  -106312650,  -613705551,
//     -1773434099, -1616692368, 580026799,   -197668875,  1679075954,  -1204552429, -1937649312, -1995967652,
//     1166423140,  877419818,   -37763499,   -1900362622, -1627370547, -1117545099, -346584376,  -124655767,
//     -1751900528, 1705159263,  -75404798,   -2093262873, -70727056,   470413085,   -486434488,  655492731,
//     660204781,   1559964712,  -1050095040, 649017882,   2032448283,  -351690846,  -1250970843, -2022766254,
//     -995403470,  -2042045251, 1319820968,  1468541913,  -1808251610, -1970740237, 1742713535,  -1224359351,
//     1904976217,  373757187,   -221218111,  -1621607003, -122347975,  -1149088878, 1544811826,  959157313,
//     1373693402,  1743323457,  -1911314465, 1028501829,  -1711922731, 35302227,    -399775541,  -755013875,
//     340789294,   -1023375812, 2041400040,  -1919381023, 293496965,   -1644581470, -1069030006, 1546311270,
//     -311684506,  -584321019,  311180177,   -945314925,  933326812,   -648073597,  -584794547,  -483801347};

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
        // SPlayerInput Input;
        for (int t = 0; t < TICKS_PER_ITERATION; ++t) {
          // Input.m_Direction = World.m_GameTick % 3 - 1;
          // Input.m_Jump = World.m_GameTick % 2;
          // Input.m_Hook = World.m_GameTick % 2;
          // Input.m_TargetX = aRandom[i % 256];
          // Input.m_TargetY = aRandom[255 - i % 256];
          // cc_on_input(&World.m_pCharacters[0], &Input);
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
