// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_LOGGING_IMPL_H_
#define QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_LOGGING_IMPL_H_

#include <vector>

#include "core/check_op.hpp"
#include "core/compiler_specific.hpp"
#include "core/logging.hpp"

#define QUICHE_LOG_IMPL(severity) LOG_##severity
#define QUICHE_VLOG_IMPL(verbose_level) VLOG(verbose_level)
#define QUICHE_LOG_EVERY_N_SEC_IMPL(severity, seconds) QUICHE_LOG_IMPL(severity)
#define QUICHE_LOG_FIRST_N_IMPL(severity, n) QUICHE_LOG_IMPL(severity)
#define QUICHE_DLOG_IMPL(severity) DLOG_##severity
#define QUICHE_DLOG_IF_IMPL(severity, condition) \
  DLOG_IF_##severity(condition)
#define QUICHE_LOG_IF_IMPL(severity, condition) \
  LOG_IF_##severity(condition)

#define LOG_INFO VLOG(1)
#define LOG_WARNING DLOG(WARNING)
#define LOG_ERROR DLOG(ERROR)
#define LOG_FATAL LOG(FATAL)
#define LOG_DFATAL LOG(DFATAL)

#define DLOG_INFO DVLOG(1)
#define DLOG_WARNING DLOG(WARNING)
#define DLOG_ERROR DLOG(ERROR)
#define DLOG_FATAL DLOG(FATAL)
#define DLOG_DFATAL DLOG(DFATAL)

#define LOG_IF_INFO(condition) VLOG_IF(1, condition)
#define LOG_IF_WARNING(condition) DLOG_IF(WARNING, condition)
#define LOG_IF_ERROR(condition) DLOG_IF(ERROR, condition)
#define LOG_IF_FATAL(condition) LOG_IF(FATAL, condition)
#define LOG_IF_DFATAL(condition) LOG_IF(DFATAL, condition)

#define DLOG_IF_INFO(condition) DVLOG_IF(1, condition)
#define DLOG_IF_WARNING(condition) DLOG_IF(WARNING, condition)
#define DLOG_IF_ERROR(condition) DLOG_IF(ERROR, condition)
#define DLOG_IF_FATAL(condition) DLOG_IF(FATAL, condition)
#define DLOG_IF_DFATAL(condition) DLOG_IF(DFATAL, condition)

#define QUICHE_DVLOG_IMPL(verbose_level) DVLOG(verbose_level)
#define QUICHE_DVLOG_IF_IMPL(verbose_level, condition) \
  DVLOG_IF(verbose_level, condition)

#define QUICHE_LOG_INFO_IS_ON_IMPL() 0
#ifdef NDEBUG
#define QUICHE_LOG_WARNING_IS_ON_IMPL() 0
#define QUICHE_LOG_ERROR_IS_ON_IMPL() 0
#else
#define QUICHE_LOG_WARNING_IS_ON_IMPL() 1
#define QUICHE_LOG_ERROR_IS_ON_IMPL() 1
#endif
#define QUICHE_DLOG_INFO_IS_ON_IMPL() 0

#ifdef OS_WIN
// wingdi.h defines ERROR to be 0. When we call QUICHE_DLOG(ERROR), it gets
// substituted with 0, and it expands to DLOG_0. To allow us to
// keep using this syntax, we define this macro to do the same thing as
// DLOG_ERROR.
#define LOG_0 LOG_ERROR
#define DLOG_0 DLOG_ERROR
#define LOG_IF_0 LOG_IF_ERROR
#define DLOG_IF_0 DLOG_IF_ERROR
#endif

#define QUICHE_PREDICT_FALSE_IMPL(x) x
#define QUICHE_PREDICT_TRUE_IMPL(x) x

#define QUICHE_NOTREACHED_IMPL() NOTREACHED()

#define QUICHE_PLOG_IMPL(severity) DVLOG(1)

#define QUICHE_CHECK_IMPL(condition) CHECK(condition)
#define QUICHE_CHECK_EQ_IMPL(val1, val2) CHECK_EQ(val1, val2)
#define QUICHE_CHECK_NE_IMPL(val1, val2) CHECK_NE(val1, val2)
#define QUICHE_CHECK_LE_IMPL(val1, val2) CHECK_LE(val1, val2)
#define QUICHE_CHECK_LT_IMPL(val1, val2) CHECK_LT(val1, val2)
#define QUICHE_CHECK_GE_IMPL(val1, val2) CHECK_GE(val1, val2)
#define QUICHE_CHECK_GT_IMPL(val1, val2) CHECK_GT(val1, val2)

#define QUICHE_DCHECK_IMPL(condition) DCHECK(condition)
#define QUICHE_DCHECK_EQ_IMPL(val1, val2) DCHECK_EQ(val1, val2)
#define QUICHE_DCHECK_NE_IMPL(val1, val2) DCHECK_NE(val1, val2)
#define QUICHE_DCHECK_LE_IMPL(val1, val2) DCHECK_LE(val1, val2)
#define QUICHE_DCHECK_LT_IMPL(val1, val2) DCHECK_LT(val1, val2)
#define QUICHE_DCHECK_GE_IMPL(val1, val2) DCHECK_GE(val1, val2)
#define QUICHE_DCHECK_GT_IMPL(val1, val2) DCHECK_GT(val1, val2)

namespace quic {
template <typename T>
inline std::ostream& operator<<(std::ostream& out,
                                const std::vector<T>& v) {
  out << "[";
  const char* sep = "";
  for (size_t i = 0; i < v.size(); ++i) {
    out << sep << v[i];
    sep = ", ";
  }
  return out << "]";
}
}  // namespace quic

#endif  // QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_LOGGING_IMPL_H_
