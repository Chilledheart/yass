/* More subroutines needed by GCC output code on some machines.  */
/* Compile this one with gcc.  */
/* Copyright (C) 1989-2022 Free Software Foundation, Inc.
This file is part of GCC.
*/

/* predefined definition */
#define L_udivdi3
#define L_divdi3
#define L_umoddi3
#define L_moddi3
#define L_udivmoddi4
#define TARGET_HAS_NO_HW_DIVIDE

/* below are from libgcc2.h */

#include <stdlib.h>

#if __SIZEOF_POINTER__ != 4
#error This is for 32-bit targets only
#endif

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error This is for little endian targets only
#endif

/* In the first part of this file, we are interfacing to calls generated
   by the compiler itself.  These calls pass values into these routines
   which have very specific modes (rather than very specific types), and
   these compiler-generated calls also expect any return values to have
   very specific modes (rather than very specific types).  Thus, we need
   to avoid using regular C language type names in this part of the file
   because the sizes for those types can be configured to be anything.
   Instead we use the following special type names.  */

/* These typedefs are usually forbidden on dsp's with UNITS_PER_WORD 1.  */
typedef 	 int SItype	__attribute__ ((mode (SI)));
typedef unsigned int USItype	__attribute__ ((mode (SI)));
#if __SIZEOF_LONG_LONG__ > 4
/* These typedefs are usually forbidden on archs with UNITS_PER_WORD 2.  */
typedef		 int DItype	__attribute__ ((mode (DI)));
typedef unsigned int UDItype	__attribute__ ((mode (DI)));
#endif

#define W_TYPE_SIZE (4 * __CHAR_BIT__)
#define Wtype	SItype
#define UWtype	USItype
#define HWtype	SItype
#define UHWtype	USItype
#define DWtype	DItype
#define UDWtype	UDItype

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
  struct DWstruct {Wtype high, low;};
#else
  struct DWstruct {Wtype low, high;};
#endif

/* We need this union to unpack/pack DImode values, since we don't have
   any arithmetic yet.  Incoming DImode parameters are stored into the
   `ll' field, and the unpacked result is read from the struct `s'.  */

typedef union
{
  struct DWstruct s;
  DWtype ll;
} DWunion;

/* below are from libgcc2.c */

#ifdef L_udivmoddi4
#ifdef TARGET_HAS_NO_HW_DIVIDE

#if (defined (L_udivdi3) || defined (L_divdi3) || \
     defined (L_umoddi3) || defined (L_moddi3) || \
     defined (L_divmoddi4))
static inline __attribute__ ((__always_inline__))
#endif
UDWtype
__udivmoddi4 (UDWtype n, UDWtype d, UDWtype *rp)
{
  UDWtype q = 0, r = n, y = d;
  UWtype lz1, lz2, i, k;

  /* Implements align divisor shift dividend method. This algorithm
     aligns the divisor under the dividend and then perform number of
     test-subtract iterations which shift the dividend left. Number of
     iterations is k + 1 where k is the number of bit positions the
     divisor must be shifted left to align it under the dividend.
     quotient bits can be saved in the rightmost positions of the dividend
     as it shifts left on each test-subtract iteration. */

  if (y <= r)
    {
      lz1 = __builtin_clzll (d);
      lz2 = __builtin_clzll (n);

      k = lz1 - lz2;
      y = (y << k);

      /* Dividend can exceed 2 ^ (width - 1) - 1 but still be less than the
	 aligned divisor. Normal iteration can drops the high order bit
	 of the dividend. Therefore, first test-subtract iteration is a
	 special case, saving its quotient bit in a separate location and
	 not shifting the dividend. */
      if (r >= y)
	{
	  r = r - y;
	  q =  (1ULL << k);
	}

      if (k > 0)
	{
	  y = y >> 1;

	  /* k additional iterations where k regular test subtract shift
	    dividend iterations are done.  */
	  i = k;
	  do
	    {
	      if (r >= y)
		r = ((r - y) << 1) + 1;
	      else
		r =  (r << 1);
	      i = i - 1;
	    } while (i != 0);

	  /* First quotient bit is combined with the quotient bits resulting
	     from the k regular iterations.  */
	  q = q + r;
	  r = r >> k;
	  q = q - (r << k);
	}
    }

  if (rp)
    *rp = r;
  return q;
}
#else

#if (defined (L_udivdi3) || defined (L_divdi3) || \
     defined (L_umoddi3) || defined (L_moddi3) || \
     defined (L_divmoddi4))
static inline __attribute__ ((__always_inline__))
#endif
UDWtype
__udivmoddi4 (UDWtype n, UDWtype d, UDWtype *rp)
{
  const DWunion nn = {.ll = n};
  const DWunion dd = {.ll = d};
  DWunion rr;
  UWtype d0, d1, n0, n1, n2;
  UWtype q0, q1;
  UWtype b, bm;

  d0 = dd.s.low;
  d1 = dd.s.high;
  n0 = nn.s.low;
  n1 = nn.s.high;

#if !UDIV_NEEDS_NORMALIZATION
  if (d1 == 0)
    {
      if (d0 > n1)
	{
	  /* 0q = nn / 0D */

	  udiv_qrnnd (q0, n0, n1, n0, d0);
	  q1 = 0;

	  /* Remainder in n0.  */
	}
      else
	{
	  /* qq = NN / 0d */

	  if (d0 == 0)
	    d0 = 1 / d0;	/* Divide intentionally by zero.  */

	  udiv_qrnnd (q1, n1, 0, n1, d0);
	  udiv_qrnnd (q0, n0, n1, n0, d0);

	  /* Remainder in n0.  */
	}

      if (rp != 0)
	{
	  rr.s.low = n0;
	  rr.s.high = 0;
	  *rp = rr.ll;
	}
    }

#else /* UDIV_NEEDS_NORMALIZATION */

  if (d1 == 0)
    {
      if (d0 > n1)
	{
	  /* 0q = nn / 0D */

	  count_leading_zeros (bm, d0);

	  if (bm != 0)
	    {
	      /* Normalize, i.e. make the most significant bit of the
		 denominator set.  */

	      d0 = d0 << bm;
	      n1 = (n1 << bm) | (n0 >> (W_TYPE_SIZE - bm));
	      n0 = n0 << bm;
	    }

	  udiv_qrnnd (q0, n0, n1, n0, d0);
	  q1 = 0;

	  /* Remainder in n0 >> bm.  */
	}
      else
	{
	  /* qq = NN / 0d */

	  if (d0 == 0)
	    d0 = 1 / d0;	/* Divide intentionally by zero.  */

	  count_leading_zeros (bm, d0);

	  if (bm == 0)
	    {
	      /* From (n1 >= d0) /\ (the most significant bit of d0 is set),
		 conclude (the most significant bit of n1 is set) /\ (the
		 leading quotient digit q1 = 1).
		 This special case is necessary, not an optimization.
		 (Shifts counts of W_TYPE_SIZE are undefined.)  */

	      n1 -= d0;
	      q1 = 1;
	    }
	  else
	    {
	      /* Normalize.  */

	      b = W_TYPE_SIZE - bm;

	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      udiv_qrnnd (q1, n1, n2, n1, d0);
	    }

	  /* n1 != d0...  */

	  udiv_qrnnd (q0, n0, n1, n0, d0);

	  /* Remainder in n0 >> bm.  */
	}

      if (rp != 0)
	{
	  rr.s.low = n0 >> bm;
	  rr.s.high = 0;
	  *rp = rr.ll;
	}
    }
#endif /* UDIV_NEEDS_NORMALIZATION */

  else
    {
      if (d1 > n1)
	{
	  /* 00 = nn / DD */

	  q0 = 0;
	  q1 = 0;

	  /* Remainder in n1n0.  */
	  if (rp != 0)
	    {
	      rr.s.low = n0;
	      rr.s.high = n1;
	      *rp = rr.ll;
	    }
	}
      else
	{
	  /* 0q = NN / dd */

	  count_leading_zeros (bm, d1);
	  if (bm == 0)
	    {
	      /* From (n1 >= d1) /\ (the most significant bit of d1 is set),
		 conclude (the most significant bit of n1 is set) /\ (the
		 quotient digit q0 = 0 or 1).
		 This special case is necessary, not an optimization.  */

	      /* The condition on the next line takes advantage of that
		 n1 >= d1 (true due to program flow).  */
	      if (n1 > d1 || n0 >= d0)
		{
		  q0 = 1;
		  sub_ddmmss (n1, n0, n1, n0, d1, d0);
		}
	      else
		q0 = 0;

	      q1 = 0;

	      if (rp != 0)
		{
		  rr.s.low = n0;
		  rr.s.high = n1;
		  *rp = rr.ll;
		}
	    }
	  else
	    {
	      UWtype m1, m0;
	      /* Normalize.  */

	      b = W_TYPE_SIZE - bm;

	      d1 = (d1 << bm) | (d0 >> b);
	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      udiv_qrnnd (q0, n1, n2, n1, d1);
	      umul_ppmm (m1, m0, q0, d0);

	      if (m1 > n1 || (m1 == n1 && m0 > n0))
		{
		  q0--;
		  sub_ddmmss (m1, m0, m1, m0, d1, d0);
		}

	      q1 = 0;

	      /* Remainder in (n1n0 - m1m0) >> bm.  */
	      if (rp != 0)
		{
		  sub_ddmmss (n1, n0, n1, n0, m1, m0);
		  rr.s.low = (n1 << b) | (n0 >> bm);
		  rr.s.high = n1 >> bm;
		  *rp = rr.ll;
		}
	    }
	}
    }

  const DWunion ww = {{.low = q0, .high = q1}};
  return ww.ll;
}
#endif
#endif

#ifdef L_divdi3
DWtype
__divdi3 (DWtype u, DWtype v)
{
  Wtype c = 0;
  DWunion uu = {.ll = u};
  DWunion vv = {.ll = v};
  DWtype w;

  if (uu.s.high < 0)
    c = ~c,
    uu.ll = -uu.ll;
  if (vv.s.high < 0)
    c = ~c,
    vv.ll = -vv.ll;

  w = __udivmoddi4 (uu.ll, vv.ll, (UDWtype *) 0);
  if (c)
    w = -w;

  return w;
}
#endif

#ifdef L_moddi3
DWtype
__moddi3 (DWtype u, DWtype v)
{
  Wtype c = 0;
  DWunion uu = {.ll = u};
  DWunion vv = {.ll = v};
  DWtype w;

  if (uu.s.high < 0)
    c = ~c,
    uu.ll = -uu.ll;
  if (vv.s.high < 0)
    vv.ll = -vv.ll;

  (void) __udivmoddi4 (uu.ll, vv.ll, (UDWtype*)&w);
  if (c)
    w = -w;

  return w;
}
#endif

#ifdef L_divmoddi4
DWtype
__divmoddi4 (DWtype u, DWtype v, DWtype *rp)
{
  Wtype c1 = 0, c2 = 0;
  DWunion uu = {.ll = u};
  DWunion vv = {.ll = v};
  DWtype w;
  DWtype r;

  if (uu.s.high < 0)
    c1 = ~c1, c2 = ~c2,
    uu.ll = -uu.ll;
  if (vv.s.high < 0)
    c1 = ~c1,
    vv.ll = -vv.ll;

  w = __udivmoddi4 (uu.ll, vv.ll, (UDWtype*)&r);
  if (c1)
    w = -w;
  if (c2)
    r = -r;

  *rp = r;
  return w;
}
#endif

#ifdef L_umoddi3
UDWtype
__umoddi3 (UDWtype u, UDWtype v)
{
  UDWtype w;

  (void) __udivmoddi4 (u, v, &w);

  return w;
}
#endif

#ifdef L_udivdi3
UDWtype
__udivdi3 (UDWtype n, UDWtype d)
{
  return __udivmoddi4 (n, d, (UDWtype *) 0);
}
#endif
