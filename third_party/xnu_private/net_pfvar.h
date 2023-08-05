/*	$apfw: git commit b6bf13f8321283cd7ee82b1795e86506084b1b95 $ */
/*	$OpenBSD: pfvar.h,v 1.259 2007/12/02 12:08:04 pascoe Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * NAT64 - Copyright (c) 2010 Viagenie Inc. (http://www.viagenie.ca)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _NET_PFVAR_H_
#define _NET_PFVAR_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <net/if_var.h>

enum    { PF_INOUT, PF_IN, PF_OUT };

struct pf_addr {
	union {
		struct in_addr          _v4addr;
		struct in6_addr         _v6addr;
		u_int8_t                _addr8[16];
		u_int16_t               _addr16[8];
		u_int32_t               _addr32[4];
	} pfa;              /* 128-bit address */
#define v4addr  pfa._v4addr
#define v6addr  pfa._v6addr
#define addr8   pfa._addr8
#define addr16  pfa._addr16
#define addr32  pfa._addr32
};

union pf_state_xport {
	u_int16_t       port;
	u_int16_t       call_id;
	u_int32_t       spi;
};

struct pfioc_natlook {
	struct pf_addr   saddr;
	struct pf_addr   daddr;
	struct pf_addr   rsaddr;
	struct pf_addr   rdaddr;
	union pf_state_xport    sxport;
	union pf_state_xport    dxport;
	union pf_state_xport    rsxport;
	union pf_state_xport    rdxport;
	sa_family_t      af;
	u_int8_t         proto;
	u_int8_t         proto_variant;
	u_int8_t         direction;
};

/*
 * ioctl operations
 */
#define DIOCNATLOOK     _IOWR('D', 23, struct pfioc_natlook)

#ifdef  __cplusplus
}
#endif
#endif /* _NET_PFVAR_H_ */
