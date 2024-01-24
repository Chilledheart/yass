// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifndef _H_HARMONY_YASS_HPP
#define _H_HARMONY_YASS_HPP

#ifdef __OHOS__

// synchronous call
int setProtectFd(int fd);

__attribute__((visibility ("default")))
extern "C" void RegisterEntryModule(void);

#endif // __OHOS__

#endif //  _H_HARMONY_YASS_HPP
