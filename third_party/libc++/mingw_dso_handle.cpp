#include "cxxabi.h"

extern "C" {
// TO work around http://code.google.coom/p/android/issues/detail?id=23203
_LIBCXXABI_WEAK void *__dso_handle;
}
