#ifndef LIB_TESTS_UTIL_H
#define LIB_TESTS_UTIL_H

#include <stdio.h>
#include <string.h>
#include <time.h>

clock_t timer_start() { return clock(); }

double timer_end(clock_t start_time) {
  clock_t end_time = clock();
  return (double)(end_time - start_time) / CLOCKS_PER_SEC;
}

void format_int(long long num, char *result) {
  char buffer[50];
  sprintf(buffer, "%lld", num);

  int len = strlen(buffer);
  int newLen = len + (len - 1) / 3;
  int j = newLen - 1;
  result[newLen] = '\0';

  for (int i = len - 1, k = 0; i >= 0; i--, k++) {
    result[j--] = buffer[i];
    if (k % 3 == 2 && i != 0) {
      result[j--] = ',';
    }
  }
}
#endif // LIB_TESTS_UTIL_H
