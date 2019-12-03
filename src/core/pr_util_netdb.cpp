//
// pr_util_netdb.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "pr_util.hpp"
#include <cstring>

#define _PR_IN6_IS_ADDR_UNSPECIFIED(a)				\
				(((a)->p_s6_addr32[0] == 0) &&	\
				((a)->p_s6_addr32[1] == 0) &&		\
				((a)->p_s6_addr32[2] == 0) &&		\
				((a)->p_s6_addr32[3] == 0))

#define _PR_IN6_IS_ADDR_LOOPBACK(a)					\
               (((a)->p_s6_addr32[0] == 0)	&&	\
               ((a)->p_s6_addr32[1] == 0)		&&	\
               ((a)->p_s6_addr32[2] == 0)		&&	\
               ((a)->p_s6_addr[12] == 0)		&&	\
               ((a)->p_s6_addr[13] == 0)		&&	\
               ((a)->p_s6_addr[14] == 0)		&&	\
               ((a)->p_s6_addr[15] == 0x1U))

const PIPv6Addr _pr_in6addr_any =	{{{ 0, 0, 0, 0,
										0, 0, 0, 0,
										0, 0, 0, 0,
										0, 0, 0, 0 }}};

const PIPv6Addr _pr_in6addr_loopback = {{{ 0, 0, 0, 0,
											0, 0, 0, 0,
											0, 0, 0, 0,
											0, 0, 0, 0x1U }}};
/*
 * The values at bytes 10 and 11 are compared using pointers to
 * 8-bit fields, and not 32-bit fields, to make the comparison work on
 * both big-endian and little-endian systems
 */

#define _PR_IN6_IS_ADDR_V4MAPPED(a)			\
		(((a)->p_s6_addr32[0] == 0) 	&&	\
		((a)->p_s6_addr32[1] == 0)	&&	\
		((a)->p_s6_addr[8] == 0)		&&	\
		((a)->p_s6_addr[9] == 0)		&&	\
		((a)->p_s6_addr[10] == 0xff)	&&	\
		((a)->p_s6_addr[11] == 0xff))

#define _PR_IN6_IS_ADDR_V4COMPAT(a)			\
		(((a)->p_s6_addr32[0] == 0) &&	\
		((a)->p_s6_addr32[1] == 0) &&		\
		((a)->p_s6_addr32[2] == 0))

#define _PR_IN6_V4MAPPED_TO_IPADDR(a) ((a)->p_s6_addr32[3])

bool IsValidNetAddr(const PNetAddr *addr)
{
    if ((addr != NULL)
	    && (addr->raw.family != P_AF_LOCAL)
	    && (addr->raw.family != P_AF_INET6)
	    && (addr->raw.family != P_AF_INET)) {
        return false;
    }
    return true;
}

PRStatus PR_InitializeNetAddr(
    PNetAddrValue val, uint16_t port, PNetAddr *addr)
{
    PRStatus rv = PR_SUCCESS;
    if (!_pr_initialized) _PR_ImplicitInitialization();

	if (val != P_IpAddrNull) memset(addr, 0, sizeof(addr->inet));
	addr->inet.family = AF_INET;
	addr->inet.port = htons(port);
	switch (val)
	{
	case P_IpAddrNull:
		break;  /* don't overwrite the address */
	case P_IpAddrAny:
		addr->inet.ip = htonl(INADDR_ANY);
		break;
	case P_IpAddrLoopback:
		addr->inet.ip = htonl(INADDR_LOOPBACK);
		break;
	default:
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
		rv = PR_FAILURE;
	}
    return rv;
}  /* PR_InitializeNetAddr */

PRStatus PR_SetNetAddr(
    PNetAddrValue val, uint16_t af, uint16_t port, PNetAddr *addr)
{
    PRStatus rv = PR_SUCCESS;
    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (af == P_AF_INET6)
    {
        if (val != P_IpAddrNull) memset(addr, 0, sizeof(addr->ipv6));
        addr->ipv6.family = af;
        addr->ipv6.port = htons(port);
        addr->ipv6.flowinfo = 0;
        addr->ipv6.scope_id = 0;
        switch (val)
        {
        case P_IpAddrNull:
            break;  /* don't overwrite the address */
        case P_IpAddrAny:
            addr->ipv6.ip = _pr_in6addr_any;
            break;
        case P_IpAddrLoopback:
            addr->ipv6.ip = _pr_in6addr_loopback;
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = PR_FAILURE;
        }
    }
    else
    {
        if (val != P_IpAddrNull) memset(addr, 0, sizeof(addr->inet));
        addr->inet.family = af;
        addr->inet.port = htons(port);
        switch (val)
        {
        case P_IpAddrNull:
            break;  /* don't overwrite the address */
        case P_IpAddrAny:
            addr->inet.ip = htonl(INADDR_ANY);
            break;
        case P_IpAddrLoopback:
            addr->inet.ip = htonl(INADDR_LOOPBACK);
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = PR_FAILURE;
        }
    }
    return rv;
}  /* PR_SetNetAddr */
