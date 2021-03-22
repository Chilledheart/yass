// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#include "gui/utils.hpp"

#include "core/logging.hpp"
#ifdef __linux__
#include <time.h>
uint64_t Utils::GetCurrentTime() {
  static struct timespec start_ts;
  static bool started;
  struct timespec ts;
  int ret;
  if (!started) {
    ret = clock_gettime(CLOCK_MONOTONIC, &start_ts);
    if (ret < 0) {
      LOG(WARNING) << "clock_gettime failed";
      return 0;
    }
    started = true;
  }
  // Activity to be timed

  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret < 0) {
    LOG(WARNING) << "clock_gettime failed";
    return 0;
  }
  return (double)(ts.tv_sec - start_ts.tv_sec) * NS_PER_SECOND +
    ts.tv_nsec - start_ts.tv_nsec;

}
#endif
