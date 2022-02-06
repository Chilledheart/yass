// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_BUG_TRACKER_IMPL_H_
#define QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_BUG_TRACKER_IMPL_H_

#include "quic/platform/api/quic_logging.h"

#define QUICHE_BUG_IMPL(bug_id) QUIC_LOG(DFATAL)
#define QUICHE_BUG_IF_IMPL(bug_id, condition) QUIC_LOG_IF(DFATAL, condition)
#define QUICHE_PEER_BUG_IMPL(bug_id) QUIC_LOG(ERROR)
#define QUICHE_PEER_BUG_IF_IMPL(bug_id, condition) QUIC_LOG_IF(ERROR, condition)

#define QUICHE_BUG_V2_IMPL(bug_id) QUIC_LOG(DFATAL)
#define QUICHE_BUG_IF_V2_IMPL(bug_id, condition) QUIC_LOG_IF(DFATAL, condition)
#define QUICHE_PEER_BUG_V2_IMPL(bug_id) QUIC_LOG(ERROR)
#define QUICHE_PEER_BUG_IF_V2_IMPL(bug_id, condition) \
  QUIC_LOG_IF(ERROR, condition)

#endif  // QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_BUG_TRACKER_IMPL_H_
