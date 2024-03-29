// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifndef _H_HARMONY_TUN2PROXY_HPP
#define _H_HARMONY_TUN2PROXY_HPP

extern "C"
void* tun2proxy_init(const char* proxy_url,
                     int tun_fd,
                     int tun_mtu,
                     int log_level,
                     int dns_over_tcp);

extern "C"
int tun2proxy_run(void* ptr);

extern "C"
int tun2proxy_destroy(void* ptr);

#endif //  _H_HARMONY_TUN2PROXY_HPP
