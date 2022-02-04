// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_canon.h"

namespace url {

template class CanonOutputT<char>;
template class CanonOutputT<char16_t>;

}  // namespace url
