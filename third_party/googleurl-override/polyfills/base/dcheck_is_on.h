// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLYFILLS_BASE_DCHECK_IS_ON_H_
#define POLYFILLS_BASE_DCHECK_IS_ON_H_

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define DCHECK_IS_ON() false
#else
#define DCHECK_IS_ON() true
#endif

#define EXPENSIVE_DCHECKS_ARE_ON() false

#define GURL_DCHECK_IS_ON() DCHECK_IS_ON()

#endif  // POLYFILLS_BASE_DCHECK_IS_ON_H_
