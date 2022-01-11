// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#include "core/logging.hpp"

#ifdef __APPLE__

#include <AvailabilityMacros.h>

#include <mach/mach_time.h>
#include <stdint.h>

static uint64_t MachTimeToNanoseconds(uint64_t machTime) {
  uint64_t nanoseconds = 0;
  static mach_timebase_info_data_t sTimebase;
  if (sTimebase.denom == 0) {
    kern_return_t mtiStatus = mach_timebase_info(&sTimebase);
    DCHECK_EQ(mtiStatus, KERN_SUCCESS);
    (void)mtiStatus;
  }

  nanoseconds = ((machTime * sTimebase.numer) / sTimebase.denom);

  return nanoseconds;
}

uint64_t GetMonotonicTime() {
  uint64_t now = mach_absolute_time();
  return MachTimeToNanoseconds(now);
}

#endif  // __APPLE__
