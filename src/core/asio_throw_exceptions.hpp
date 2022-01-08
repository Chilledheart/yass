// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef H_CORE_ASIO_THROW_EXCEPTIONS
#define H_CORE_ASIO_THROW_EXCEPTIONS

#include "core/logging.hpp"

namespace asio {
namespace detail {

// provide a definition of this function if ASIO_NO_EXCEPTIONS is defined
#if defined(ASIO_NO_EXCEPTIONS)
template <typename Exception>
void throw_exception(const Exception& e) {
  LOG(FATAL) << "ASIO Error Msg: " << e.what();
}
#endif  // ASIO_NO_EXCEPTIONS

}  // namespace detail
}  // namespace asio

#endif  // H_CORE_ASIO_THROW_EXCEPTIONS
