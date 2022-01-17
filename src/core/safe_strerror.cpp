// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/safe_strerror.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace {
const char* StrErrorAdaptor(int errnum, char* buf, size_t buflen) {
#if defined(_WIN32)
  int rc = strerror_s(buf, buflen, errnum);
  buf[buflen - 1] = '\0';  // guarantee NUL termination
  if (rc == 0 && strncmp(buf, "Unknown error", buflen) == 0)
    *buf = '\0';
  return buf;
#else
  // The type of `ret` is platform-specific; both of these branches must compile
  // either way but only one will execute on any given platform:
  auto ret = strerror_r(errnum, buf, buflen);
  if (std::is_same<decltype(ret), int>::value) {
    // XSI `strerror_r`; `ret` is `int`:
    if (ret)
      *buf = '\0';
    return buf;
  } else {
    // GNU `strerror_r`; `ret` is `char *`:
    return reinterpret_cast<const char*>(ret);
  }
#endif
}

int posix_strerror_r(int err, char* buf, size_t len) {
  // Sanity check input parameters
  if (buf == nullptr || len <= 0) {
    errno = EINVAL;
    return -1;
  }

  // Reset buf and errno, and try calling whatever version of strerror_r()
  // is implemented by glibc
  buf[0] = '\000';
  int old_errno = errno;
  errno = 0;
  // The GNU-specific strerror_r() returns a pointer to a string containing
  // the error message.
  const char* rc = StrErrorAdaptor(err, buf, len);

  // Both versions set errno on failure
  if (errno) {
    // Should already be there, but better safe than sorry
    buf[0] = '\000';
    return -1;
  }
  errno = old_errno;

  // POSIX is vague about whether the string will be terminated, although
  // is indirectly implies that typically ERANGE will be returned, instead
  // of truncating the string. This is different from the GNU implementation.
  // We play it safe by always terminating the string explicitly.
  buf[len - 1] = '\000';

  // If the function succeeded, we can use its exit code to determine the
  // semantics implemented by glibc
  if (!rc) {
    return 0;
  } else {
    // GNU semantics detected
    if (rc == buf) {
      return 0;
    } else {
      buf[0] = '\000';
#if defined(__APPLE__)
      if (reinterpret_cast<intptr_t>(rc) < sys_nerr) {
        // This means an error on MacOSX or FreeBSD.
        return -1;
      }
#endif
      strncat(buf, rc, len - 1);
      return 0;
    }
  }
}

}  // namespace

void safe_strerror_r(int err, char* buf, size_t len) {
  if (buf == nullptr || len <= 0) {
    return;
  }
  posix_strerror_r(err, buf, len);
}

std::string safe_strerror(int err) {
  const int buffer_size = 256;
  char buf[buffer_size];
  int rc = posix_strerror_r(err, buf, sizeof(buf));
  if ((rc < 0) || (buf[0] == '\000')) {
    snprintf(buf, sizeof(buf), "Error number %d", err);
  }
  return buf;
}
