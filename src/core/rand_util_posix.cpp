// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/rand_util.hpp"

#ifndef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <base/posix/eintr_wrapper.h>

#include "core/logging.hpp"

#if defined(__linux__) || defined(__ANDROID__)
#include "linux_syscall_support.h"
#elif BUILDFLAG(IS_MAC)
// TODO(crbug.com/995996): Waiting for this header to appear in the iOS SDK.
// (See below.)
#include <sys/random.h>
#endif

#if __APPLE__
#include <AvailabilityMacros.h>
#endif

// Inlcude MSAN_UNPOISON fixes
#include "core/compiler_specific.hpp"

// TODO: move to file_utils.cpp
static bool ReadFromFD(int fd, char* buffer, size_t bytes);

bool ReadFromFD(int fd, char* buffer, size_t bytes) {
  size_t total_read = 0;
  while (total_read < bytes) {
    ssize_t bytes_read = HANDLE_EINTR(read(fd, buffer + total_read, bytes - total_read));
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
  }
  return total_read == bytes;
}

namespace {

static constexpr int kOpenFlags = O_RDONLY | O_CLOEXEC;

// We keep the file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive), and since we may not even be able to reopen
// it if we are later put in a sandbox. This class wraps the file descriptor so
// we can use a static-local variable to handle opening it on the first access.
class URandomFd {
 public:
  URandomFd() : fd_(HANDLE_EINTR(open("/dev/urandom", kOpenFlags))) { CHECK(fd_ >= 0) << "Cannot open /dev/urandom"; }

  ~URandomFd() { IGNORE_EINTR(::close(fd_)); }

  int fd() const { return fd_; }

 private:
  const int fd_;
};

}  // anonymous namespace

// NOTE: In an ideal future, all implementations of this function will just
// wrap BoringSSL's `RAND_bytes`. TODO(crbug.com/995996): Figure out the
// build/test/performance issues with dcheng's CL
// (https://chromium-review.googlesource.com/c/chromium/src/+/1545096) and land
// it or some form of it.
void RandBytes(void* output, size_t output_length) {
#ifdef __linux__
  // We have to call `getrandom` via Linux Syscall Support, rather than through
  // the libc wrapper, because we might not have an up-to-date libc (e.g. on
  // some bots).
  const ssize_t r = HANDLE_EINTR(sys_getrandom(output, output_length, 0));

  // Return success only on total success. In case errno == ENOSYS (or any other
  // error), we'll fall through to reading from urandom below.
  if (output_length == static_cast<size_t>(r)) {
    MSAN_UNPOISON(output, output_length);
    return;
  }
#elif BUILDFLAG(IS_MAC)
  // TODO(crbug.com/995996): Enable this on iOS too, when sys/random.h arrives
  // in its SDK.
  if (getentropy(output, output_length) == 0) {
    return;
  }
#endif

  // If the OS-specific mechanisms didn't work, fall through to reading from
  // urandom.
  //
  // TODO(crbug.com/995996): When we no longer need to support old Linux
  // kernels, we can get rid of this /dev/urandom branch altogether.
  const int urandom_fd = GetUrandomFD();
  const bool success = ReadFromFD(urandom_fd, static_cast<char*>(output), output_length);
  CHECK(success) << "urandom read failure";
}

int GetUrandomFD() {
  static URandomFd urandom_fd;
  return urandom_fd.fd();
}

#endif  // _WIN32
