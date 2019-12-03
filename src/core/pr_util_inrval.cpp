//
// pr_util_inrval.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "core/pr_util.hpp"

#define HAVE_LONG_LONG
#if defined(HAVE_LONG_LONG)

/* Keep this in sync with prtypes.h. */
#if PR_BYTES_PER_LONG == 8 && !defined(PR_ALTERNATE_INT64_TYPEDEF)
#define LL_MAXINT   9223372036854775807L
#define LL_MININT   (-LL_MAXINT - 1L)
#define LL_ZERO     0L
#define LL_MAXUINT  18446744073709551615UL
#define LL_INIT(hi, lo)  ((hi ## L << 32) + lo ## L)
#elif defined(WIN32) && !defined(__GNUC__)
#define LL_MAXINT   9223372036854775807i64
#define LL_MININT   (-LL_MAXINT - 1i64)
#define LL_ZERO     0i64
#define LL_MAXUINT  18446744073709551615ui64
#define LL_INIT(hi, lo)  ((hi ## i64 << 32) + lo ## i64)
#else
#define LL_MAXINT   9223372036854775807LL
#define LL_MININT   (-LL_MAXINT - 1LL)
#define LL_ZERO     0LL
#define LL_MAXUINT  18446744073709551615ULL
#define LL_INIT(hi, lo)  ((hi ## LL << 32) + lo ## LL)
#endif

/***********************************************************************
** MACROS:      LL_*
** DESCRIPTION:
**      The following macros define portable access to the 64 bit
**      math facilities.
**
***********************************************************************/

/***********************************************************************
** MACROS:      LL_<relational operators>
**
**  LL_IS_ZERO        Test for zero
**  LL_EQ             Test for equality
**  LL_NE             Test for inequality
**  LL_GE_ZERO        Test for zero or positive
**  LL_CMP            Compare two values
***********************************************************************/
#define LL_IS_ZERO(a)       ((a) == 0)
#define LL_EQ(a, b)         ((a) == (b))
#define LL_NE(a, b)         ((a) != (b))
#define LL_GE_ZERO(a)       ((a) >= 0)
#define LL_CMP(a, op, b)    ((int64_t)(a) op (int64_t)(b))
#define LL_UCMP(a, op, b)   ((uint64_t)(a) op (uint64_t)(b))

/***********************************************************************
** MACROS:      LL_<logical operators>
**
**  LL_AND            Logical and
**  LL_OR             Logical or
**  LL_XOR            Logical exclusion
**  LL_OR2            A disgusting deviation
**  LL_NOT            Negation (one's complement)
***********************************************************************/
#define LL_AND(r, a, b)        ((r) = (a) & (b))
#define LL_OR(r, a, b)        ((r) = (a) | (b))
#define LL_XOR(r, a, b)        ((r) = (a) ^ (b))
#define LL_OR2(r, a)        ((r) = (r) | (a))
#define LL_NOT(r, a)        ((r) = ~(a))

/***********************************************************************
** MACROS:      LL_<mathematical operators>
**
**  LL_NEG            Negation (two's complement)
**  LL_ADD            Summation (two's complement)
**  LL_SUB            Difference (two's complement)
***********************************************************************/
#define LL_NEG(r, a)        ((r) = -(a))
#define LL_ADD(r, a, b)     ((r) = (a) + (b))
#define LL_SUB(r, a, b)     ((r) = (a) - (b))

/***********************************************************************
** MACROS:      LL_<mathematical operators>
**
**  LL_MUL            Product (two's complement)
**  LL_DIV            Quotient (two's complement)
**  LL_MOD            Modulus (two's complement)
***********************************************************************/
#define LL_MUL(r, a, b)        ((r) = (a) * (b))
#define LL_DIV(r, a, b)        ((r) = (a) / (b))
#define LL_MOD(r, a, b)        ((r) = (a) % (b))

/***********************************************************************
** MACROS:      LL_<shifting operators>
**
**  LL_SHL            Shift left [0..64] bits
**  LL_SHR            Shift right [0..64] bits with sign extension
**  LL_USHR           Unsigned shift right [0..64] bits
**  LL_ISHL           Signed shift left [0..64] bits
***********************************************************************/
#define LL_SHL(r, a, b)     ((r) = (int64_t)(a) << (b))
#define LL_SHR(r, a, b)     ((r) = (int64_t)(a) >> (b))
#define LL_USHR(r, a, b)    ((r) = (uint64_t)(a) >> (b))
#define LL_ISHL(r, a, b)    ((r) = (int64_t)(a) << (b))

/***********************************************************************
** MACROS:      LL_<conversion operators>
**
**  LL_L2I            Convert to signed 32 bit
**  LL_L2UI           Convert to unsigned 32 bit
**  LL_L2F            Convert to floating point
**  LL_L2D            Convert to floating point
**  LL_I2L            Convert signed to 64 bit
**  LL_UI2L           Convert unsigned to 64 bit
**  LL_F2L            Convert float to 64 bit
**  LL_D2L            Convert float to 64 bit
***********************************************************************/
#define LL_L2I(i, l)        ((i) = (int32_t)(l))
#define LL_L2UI(ui, l)        ((ui) = (uint32_t)(l))
#define LL_L2F(f, l)        ((f) = (double)(l))
#define LL_L2D(d, l)        ((d) = (double)(l))

#define LL_I2L(l, i)        ((l) = (int64_t)(i))
#define LL_UI2L(l, ui)        ((l) = (int64_t)(ui))
#define LL_F2L(l, f)        ((l) = (int64_t)(f))
#define LL_D2L(l, d)        ((l) = (int64_t)(d))

/***********************************************************************
** MACROS:      LL_UDIVMOD
** DESCRIPTION:
**  Produce both a quotient and a remainder given an unsigned
** INPUTS:      uint64_t a: The dividend of the operation
**              uint64_t b: The quotient of the operation
** OUTPUTS:     uint64_t *qp: pointer to quotient
**              uint64_t *rp: pointer to remainder
***********************************************************************/
#define LL_UDIVMOD(qp, rp, a, b) \
    (*(qp) = ((uint64_t)(a) / (b)), \
     *(rp) = ((uint64_t)(a) % (b)))

#else  /* !HAVE_LONG_LONG */

#define LL_MAXINT   LL_MaxInt()
#define LL_MININT   LL_MinInt()
#define LL_ZERO     LL_Zero()
#define LL_MAXUINT  LL_MaxUint()

#ifdef IS_LITTLE_ENDIAN
#define LL_INIT(hi, lo) {PR_UINT32(lo), PR_UINT32(hi)}
#else
#define LL_INIT(hi, lo) {PR_UINT32(hi), PR_UINT32(lo)}
#endif

#define LL_IS_ZERO(a)        (((a).hi == 0) && ((a).lo == 0))
#define LL_EQ(a, b)        (((a).hi == (b).hi) && ((a).lo == (b).lo))
#define LL_NE(a, b)        (((a).hi != (b).hi) || ((a).lo != (b).lo))
#define LL_GE_ZERO(a)        (((a).hi >> 31) == 0)

#define LL_CMP(a, op, b)    (((a).hi == (b).hi) ? ((a).lo op (b).lo) : \
                 ((int32_t)(a).hi op (int32_t)(b).hi))
#define LL_UCMP(a, op, b)    (((a).hi == (b).hi) ? ((a).lo op (b).lo) : \
                 ((a).hi op (b).hi))

#define LL_AND(r, a, b)        ((r).lo = (a).lo & (b).lo, \
                 (r).hi = (a).hi & (b).hi)
#define LL_OR(r, a, b)        ((r).lo = (a).lo | (b).lo, \
                 (r).hi = (a).hi | (b).hi)
#define LL_XOR(r, a, b)        ((r).lo = (a).lo ^ (b).lo, \
                 (r).hi = (a).hi ^ (b).hi)
#define LL_OR2(r, a)        ((r).lo = (r).lo | (a).lo, \
                 (r).hi = (r).hi | (a).hi)
#define LL_NOT(r, a)        ((r).lo = ~(a).lo, \
                 (r).hi = ~(a).hi)

#define LL_NEG(r, a)        ((r).lo = -(int32_t)(a).lo, \
                 (r).hi = -(int32_t)(a).hi - ((r).lo != 0))
#define LL_ADD(r, a, b) { \
    int64_t _a, _b; \
    _a = a; _b = b; \
    (r).lo = _a.lo + _b.lo; \
    (r).hi = _a.hi + _b.hi + ((r).lo < _b.lo); \
}

#define LL_SUB(r, a, b) { \
    int64_t _a, _b; \
    _a = a; _b = b; \
    (r).lo = _a.lo - _b.lo; \
    (r).hi = _a.hi - _b.hi - (_a.lo < _b.lo); \
}

#define LL_MUL(r, a, b) { \
    int64_t _a, _b; \
    _a = a; _b = b; \
    LL_MUL32(r, _a.lo, _b.lo); \
    (r).hi += _a.hi * _b.lo + _a.lo * _b.hi; \
}

#define _lo16(a)        ((a) & PR_BITMASK(16))
#define _hi16(a)        ((a) >> 16)

#define LL_MUL32(r, a, b) { \
     uint32_t _a1, _a0, _b1, _b0, _y0, _y1, _y2, _y3; \
     _a1 = _hi16(a), _a0 = _lo16(a); \
     _b1 = _hi16(b), _b0 = _lo16(b); \
     _y0 = _a0 * _b0; \
     _y1 = _a0 * _b1; \
     _y2 = _a1 * _b0; \
     _y3 = _a1 * _b1; \
     _y1 += _hi16(_y0);                         /* can't carry */ \
     _y1 += _y2;                                /* might carry */ \
     if (_y1 < _y2)    \
        _y3 += (uint32_t)(PR_BIT(16));  /* propagate */ \
     (r).lo = (_lo16(_y1) << 16) + _lo16(_y0); \
     (r).hi = _y3 + _hi16(_y1); \
}

#define LL_UDIVMOD(qp, rp, a, b)    ll_udivmod(qp, rp, a, b)

NSPR_API(void) ll_udivmod(uint64_t *qp, uint64_t *rp, uint64_t a, uint64_t b);

#define LL_DIV(r, a, b) { \
    int64_t _a, _b; \
    uint32_t _negative = (int32_t)(a).hi < 0; \
    if (_negative) { \
    LL_NEG(_a, a); \
    } else { \
    _a = a; \
    } \
    if ((int32_t)(b).hi < 0) { \
    _negative ^= 1; \
    LL_NEG(_b, b); \
    } else { \
    _b = b; \
    } \
    LL_UDIVMOD(&(r), 0, _a, _b); \
    if (_negative) \
    LL_NEG(r, r); \
}

#define LL_MOD(r, a, b) { \
    int64_t _a, _b; \
    uint32_t _negative = (int32_t)(a).hi < 0; \
    if (_negative) { \
    LL_NEG(_a, a); \
    } else { \
    _a = a; \
    } \
    if ((int32_t)(b).hi < 0) { \
    LL_NEG(_b, b); \
    } else { \
    _b = b; \
    } \
    LL_UDIVMOD(0, &(r), _a, _b); \
    if (_negative) \
    LL_NEG(r, r); \
}

#define LL_SHL(r, a, b) { \
    if (b) { \
    int64_t _a; \
        _a = a; \
        if ((b) < 32) { \
        (r).lo = _a.lo << ((b) & 31); \
        (r).hi = (_a.hi << ((b) & 31)) | (_a.lo >> (32 - (b))); \
    } else { \
        (r).lo = 0; \
        (r).hi = _a.lo << ((b) & 31); \
    } \
    } else { \
    (r) = (a); \
    } \
}

/* a is an int32_t, b is int32_t, r is int64_t */
#define LL_ISHL(r, a, b) { \
    if (b) { \
    int64_t _a; \
    _a.lo = (a); \
    _a.hi = 0; \
        if ((b) < 32) { \
        (r).lo = (a) << ((b) & 31); \
        (r).hi = ((a) >> (32 - (b))); \
    } else { \
        (r).lo = 0; \
        (r).hi = (a) << ((b) & 31); \
    } \
    } else { \
    (r).lo = (a); \
    (r).hi = 0; \
    } \
}

#define LL_SHR(r, a, b) { \
    if (b) { \
    int64_t _a; \
        _a = a; \
    if ((b) < 32) { \
        (r).lo = (_a.hi << (32 - (b))) | (_a.lo >> ((b) & 31)); \
        (r).hi = (int32_t)_a.hi >> ((b) & 31); \
    } else { \
        (r).lo = (int32_t)_a.hi >> ((b) & 31); \
        (r).hi = (int32_t)_a.hi >> 31; \
    } \
    } else { \
    (r) = (a); \
    } \
}

#define LL_USHR(r, a, b) { \
    if (b) { \
    int64_t _a; \
        _a = a; \
    if ((b) < 32) { \
        (r).lo = (_a.hi << (32 - (b))) | (_a.lo >> ((b) & 31)); \
        (r).hi = _a.hi >> ((b) & 31); \
    } else { \
        (r).lo = _a.hi >> ((b) & 31); \
        (r).hi = 0; \
    } \
    } else { \
    (r) = (a); \
    } \
}

#define LL_L2I(i, l)        ((i) = (l).lo)
#define LL_L2UI(ui, l)        ((ui) = (l).lo)
#define LL_L2F(f, l)        { double _d; LL_L2D(_d, l); (f) = (double)_d; }

#define LL_L2D(d, l) { \
    int _negative; \
    int64_t _absval; \
 \
    _negative = (l).hi >> 31; \
    if (_negative) { \
    LL_NEG(_absval, l); \
    } else { \
    _absval = l; \
    } \
    (d) = (double)_absval.hi * 4.294967296e9 + _absval.lo; \
    if (_negative) \
    (d) = -(d); \
}

#define LL_I2L(l, i)        { int32_t _i = ((int32_t)(i)) >> 31; (l).lo = (i); (l).hi = _i; }
#define LL_UI2L(l, ui)      ((l).lo = (ui), (l).hi = 0)
#define LL_F2L(l, f)        { double _d = (double)f; LL_D2L(l, _d); }

#define LL_D2L(l, d) { \
    int _negative; \
    double _absval, _d_hi; \
    int64_t _lo_d; \
 \
    _negative = ((d) < 0); \
    _absval = _negative ? -(d) : (d); \
 \
    (l).hi = _absval / 4.294967296e9; \
    (l).lo = 0; \
    LL_L2D(_d_hi, l); \
    _absval -= _d_hi; \
    _lo_d.hi = 0; \
    if (_absval < 0) { \
    _lo_d.lo = -_absval; \
    LL_SUB(l, l, _lo_d); \
    } else { \
    _lo_d.lo = _absval; \
    LL_ADD(l, l, _lo_d); \
    } \
 \
    if (_negative) \
    LL_NEG(l, l); \
}

#endif /* !HAVE_LONG_LONG */


#ifdef _WIN32
/* Mozilla's build system defines this globally. */
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <MMSystem.h>
#define _PR_MD_INTERVAL_INIT()

PRIntervalTime
_PR_MD_GET_INTERVAL()
{
    /* milliseconds since system start */
    return timeGetTime();
}

PRIntervalTime
_PR_MD_INTERVAL_PER_SEC()
{
    return 1000;
}
#else
#include <sys/time.h>
#define _PR_MD_INTERVAL_INIT()
#define _PR_MD_GET_INTERVAL		_PR_UNIX_GetInterval
#define _PR_MD_INTERVAL_PER_SEC		_PR_UNIX_TicksPerSecond

/*
 * This version of interval times is based on the time of day
 * capability offered by the system. This isn't valid for two reasons:
 * 1) The time of day is neither linear nor montonically increasing
 * 2) The units here are milliseconds. That's not appropriate for our use.
 */
PRIntervalTime _PR_UNIX_GetInterval()
{
    struct timeval time;
    PRIntervalTime ticks;

    (void)gettimeofday(&time, NULL);  /* fallicy of course */
    ticks = (uint32_t)time.tv_sec * PR_MSEC_PER_SEC;  /* that's in milliseconds */
    ticks += (uint32_t)time.tv_usec / PR_USEC_PER_MSEC;  /* so's that */
    return ticks;
}  /* _PR_UNIX_GetInterval */

PRIntervalTime _PR_UNIX_TicksPerSecond()
{
    return 1000;  /* this needs some work :) */
}
#endif

/*
 *-----------------------------------------------------------------------
 *
 * _PR_InitClock --
 *
 *
 *-----------------------------------------------------------------------
 */

void _PR_InitClock(void)
{
    _PR_MD_INTERVAL_INIT();
#ifdef DEBUG
    {
        PRIntervalTime ticksPerSec = PR_TicksPerSecond();

        PR_ASSERT(ticksPerSec >= PR_INTERVAL_MIN);
        PR_ASSERT(ticksPerSec <= PR_INTERVAL_MAX);
    }
#endif /* DEBUG */
}

PRIntervalTime PR_IntervalNow(void)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();
    return _PR_MD_GET_INTERVAL();
}  /* PR_IntervalNow */

uint32_t PR_TicksPerSecond(void)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();
    return _PR_MD_INTERVAL_PER_SEC();
}  /* PR_TicksPerSecond */

PRIntervalTime PR_SecondsToInterval(uint32_t seconds)
{
    return seconds * PR_TicksPerSecond();
}  /* PR_SecondsToInterval */

PRIntervalTime PR_MillisecondsToInterval(uint32_t milli)
{
    PRIntervalTime ticks;
    uint64_t tock, tps, msecPerSec, rounding;
    LL_UI2L(tock, milli);
    LL_I2L(msecPerSec, PR_MSEC_PER_SEC);
    LL_I2L(rounding, (PR_MSEC_PER_SEC >> 1));
    LL_I2L(tps, PR_TicksPerSecond());
    LL_MUL(tock, tock, tps);
    LL_ADD(tock, tock, rounding);
    LL_DIV(tock, tock, msecPerSec);
    LL_L2UI(ticks, tock);
    return ticks;
}  /* PR_MillisecondsToInterval */

PRIntervalTime PR_MicrosecondsToInterval(uint32_t micro)
{
    PRIntervalTime ticks;
    uint64_t tock, tps, usecPerSec, rounding;
    LL_UI2L(tock, micro);
    LL_I2L(usecPerSec, PR_USEC_PER_SEC);
    LL_I2L(rounding, (PR_USEC_PER_SEC >> 1));
    LL_I2L(tps, PR_TicksPerSecond());
    LL_MUL(tock, tock, tps);
    LL_ADD(tock, tock, rounding);
    LL_DIV(tock, tock, usecPerSec);
    LL_L2UI(ticks, tock);
    return ticks;
}  /* PR_MicrosecondsToInterval */

uint32_t PR_IntervalToSeconds(PRIntervalTime ticks)
{
    return ticks / PR_TicksPerSecond();
}  /* PR_IntervalToSeconds */

uint32_t PR_IntervalToMilliseconds(PRIntervalTime ticks)
{
    uint32_t milli;
    uint64_t tock, tps, msecPerSec, rounding;
    LL_UI2L(tock, ticks);
    LL_I2L(msecPerSec, PR_MSEC_PER_SEC);
    LL_I2L(tps, PR_TicksPerSecond());
    LL_USHR(rounding, tps, 1);
    LL_MUL(tock, tock, msecPerSec);
    LL_ADD(tock, tock, rounding);
    LL_DIV(tock, tock, tps);
    LL_L2UI(milli, tock);
    return milli;
}  /* PR_IntervalToMilliseconds */

uint32_t PR_IntervalToMicroseconds(PRIntervalTime ticks)
{
    uint32_t micro;
    uint64_t tock, tps, usecPerSec, rounding;
    LL_UI2L(tock, ticks);
    LL_I2L(usecPerSec, PR_USEC_PER_SEC);
    LL_I2L(tps, PR_TicksPerSecond());
    LL_USHR(rounding, tps, 1);
    LL_MUL(tock, tock, usecPerSec);
    LL_ADD(tock, tock, rounding);
    LL_DIV(tock, tock, tps);
    LL_L2UI(micro, tock);
    return micro;
}  /* PR_IntervalToMicroseconds */

/* prinrval.c */

