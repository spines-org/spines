/* Copyright (c) 2000, The Johns Hopkins University
 * All rights reserved.
 *
 * The contents of this file are subject to a license (the ``License'')
 * that is the exact equivalent of the BSD license as of July 23, 1999. 
 * You may not use this file except in compliance with the License. The
 * specific language governing the rights and limitations of the License
 * can be found in the file ``STDUTIL_LICENSE'' found in this 
 * distribution.
 *
 * Software distributed under the License is distributed on an AS IS 
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. 
 *
 * The Original Software is:
 *     The Stdutil Library
 * 
 * Contributors:
 *     Creator - John Lane Schultz (jschultz@cnds.jhu.edu)
 *     The Center for Networking and Distributed Systems
 *         (CNDS - http://www.cnds.jhu.edu)
 */ 

#include <stdio.h>
#include <stdlib.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <stdutil/stdutil.h>
#include <stdutil/stderror.h>

/* only does indention properly for single threaded programs right now */
int std_stkfprintf(FILE *stream, int entering, const char *fmt, ...) {
#define MAX_TAB_IN 4096
#define INDENT 2
  static int tab_in = 0;
  static char tab[MAX_TAB_IN] = { 0 };
  va_list ap;
  int ret;

  if (entering < 0) {
    if ((tab_in -= INDENT) < 0)
      stderr_abort("popped off of top of empty trace print stack!\n");
    memset(tab + tab_in, 0, INDENT);
    fprintf(stream, "%sST Leave: ", tab);
  } else if (entering > 0) {
    fprintf(stream, "%sST Enter: ", tab);
    if (tab_in + INDENT >= MAX_TAB_IN)
      stderr_abort("execution stack depth exceded MAX_TAB_IN: %d\n", MAX_TAB_IN);
    memset(tab + tab_in, ' ', INDENT);
    tab_in += INDENT;
  } else
    fprintf(stream, "%s", tab);

  va_start(ap, fmt);
  ret = vfprintf(stream, fmt, ap);
  va_end(ap);

  return ret;
}

inline void stdflip_endian16(void *dst) {
  char *d = (char*) dst, t;

  STDSWAP(d[0], d[1], t);
}

inline void stdflip_endian32(void *dst) {
  char *d = (char*) dst, t;
  
  STDSWAP(d[0], d[3], t); STDSWAP(d[1], d[2], t);
}

inline void stdflip_endian64(void *dst) {
  char *d = (char*) dst, t;

  STDSWAP(d[0], d[7], t); STDSWAP(d[1], d[6], t);
  STDSWAP(d[2], d[5], t); STDSWAP(d[3], d[4], t);
}

inline void stdflip_endian_n(void *dst, size_t n) {
  char *d = (char*) dst, t;
  size_t nc = 0;

  if (!n)
    return;

  for (--n; nc < n; --n, ++nc)
    STDSWAP(d[nc], d[n], t);
}

#if defined (stduint16) && defined(stduint32)
/*
 * Copyright (c) 1993 Martin Birgmeier
 * All rights reserved.
 *
 * You may redistribute unmodified or modified versions of this source
 * code provided that the above copyright notice and this and the
 * following conditions are retained.
 *
 * This software is provided ``as is'', and comes with no warranties
 * of any kind. I shall in no event be liable for anything that happens
 * to anyone/anything when using this software.
 */

#define RAND48_SEED_0   (0x330e)
#define RAND48_SEED_1   (0xabcd)
#define RAND48_SEED_2   (0x1234)
#define RAND48_MULT_0   (0xe66d)
#define RAND48_MULT_1   (0xdeec)
#define RAND48_MULT_2   (0x0005)
#define RAND48_ADD      (0x000b)

inline stduint32 stdrand32(stduint16 x[3]) {
  stduint16 temp[2];
  stduint32 acc;

  acc = (stduint32) RAND48_MULT_0 * x[0] + RAND48_ADD;
  temp[0] = (stduint16) acc;

  acc >>= 16;
  acc += (stduint32) RAND48_MULT_0 * x[1] + (stduint32) RAND48_MULT_1 * x[0];
  temp[1] = (stduint16) acc;

  acc >>= 16;
  acc += (stduint32) RAND48_MULT_0 * x[2] + (stduint32) RAND48_MULT_1 * x[1] + 
         (stduint32) RAND48_MULT_2 * x[0];

  x[0] = temp[0];
  x[1] = temp[1];
  x[2] = (stduint16) acc;

  return ((stduint32) x[2] << 16) + x[1];
}

inline void stdrand32_dseed(stduint16 x[3], stduint32 seed) {
  x[2] = (stduint16) ((seed >> 16) ^ RAND48_SEED_0);
  x[1] = (stduint16) (seed ^ RAND48_SEED_1);
  x[0] = x[2] ^ RAND48_SEED_2;
}

inline void stdrand32_seed(stduint16 x[3], stduint32 seed) {
  stdrand32_dseed(x, ((stduint32) time(0) ^ seed) * (seed | 0x1));
}

# if defined(stduint64)
inline stduint64 stdrand64(stduint32 x[3]) {
  return ((stduint64) stdrand32((stduint16*) x) << 32) + stdrand32((stduint16*) x + 3);
}

inline void stdrand64_dseed(stduint32 x[3], stduint64 seed) {
  stdrand32_dseed((stduint16*) x, (stduint32) (seed >> 32));
  stdrand32_dseed((stduint16*) x + 3, (stduint32) seed);
}

inline void stdrand64_seed(stduint32 x[3], stduint64 seed) {
  stdrand64_dseed(x, ((stduint64) time(0) ^ seed) * (seed | 0x1));
}
# endif /* defined(stduint64) */

# if defined(stdhsize_t)
#  if SIZEOF_SIZE_T == 4
inline size_t stdrand(stdhsize_t x[3]) {
  return stdrand32(x);
}

inline void stdrand_dseed(stdhsize_t x[3], size_t seed) {
  stdrand32_dseed(x, seed);
}

inline void stdrand_seed(stdhsize_t x[3], size_t seed) {
  stdrand32_seed(x, seed);
}
#  elif SIZOF_SIZE_T == 8 && defined(stduint64)
inline size_t stdrand(stdhsize_t x[3]) {
  return stdrand64(x);
}

inline void stdrand_dseed(stdhsize_t x[3], size_t seed) {
  stdrand64_dseed(x, seed);
}

inline void stdrand_seed(stdhsize_t x[3], size_t seed) {
  stdrand64_seed(x, seed);
}
#  endif /* if SIZEOF_SIZE_T == 4 else if SIZEOF_SIZE_T == 8 && defined(stduint64) */
# endif  /* defined(stdhsize_t) */
#endif   /* defined(stduint16) && defined(stduint32) */

/* The following three functions are called when an array's size
   changes. They decide whether the capacity thresholds have been
   exceeded or not. If the thresholds have been exceeded, then they
   call the auto_allocate fcn that determines new capacity thresholds
   and allocates the necessary memory.

   get_mem    - when there is an arbitrary change in size (e.g. - resize)
   grow_mem   - when there is an increase in size
   shrink_mem - when there is a decrease in size

   Notice that we require new_size > *low_cap, this allows for
   reallocation even when low_cap is zero, which allows the array to
   go to zero memory usage.

   These fcns return zero on success, non-zero on failure.
*/
inline int stdget_mem(char **mem, size_t new_size, size_t *high_cap, 
		      size_t *low_cap, size_t vsize) {
  return (new_size <= *high_cap && new_size > *low_cap ? STD_NO_MEM_CHANGE :
	  stdauto_allocate(mem, new_size, high_cap, low_cap, vsize));
}

inline int stdgrow_mem(char **mem, size_t new_size, size_t *high_cap, 
		       size_t *low_cap, size_t vsize) {
  return (new_size <= *high_cap ? STD_NO_MEM_CHANGE :
	  stdauto_allocate(mem, new_size, high_cap, low_cap, vsize));
}

inline int stdshrink_mem(char **mem, size_t new_size, size_t *high_cap,
			 size_t *low_cap, size_t vsize) {
  return (new_size > *low_cap ? STD_NO_MEM_CHANGE :
	  stdauto_allocate(mem, new_size, high_cap, low_cap, vsize));
}

/* The brains of the auto allocation mechanism. Given a size it determines what
   the capacity thresholds should be and calls the allocation function. */
inline int stdauto_allocate(char **mem, size_t new_size, size_t *high_cap,
			    size_t *low_cap, size_t vsize) {
  size_t new_cap = new_size << 1;
  
  if (new_cap - new_size == new_size) /* check for overflow */
    return stdset_allocate(mem, new_cap, high_cap, low_cap, vsize);
  else 
    return STD_MEM_FAILURE;
}

/* If new_cap is not equal to *high_cap, then this function allocates
   an array of new_cap * vsize bytes into *mem and updates *high_cap
   and *low_cap to be the same as new_cap and new_cap/4, respectively,
   and returns STD_MEM_CHANGE. If in the case above, new_cap * vsize
   is zero then *mem is set to zero (null). If new_cap * vsize
   overflows the size_t type or malloc fails in allocating that memory
   then the function returns STD_MEM_FAILURE, but *high_cap and
   *low_cap are uneffected.

   If new_cap equals *high_cap, then the function returns
   STD_NO_MEM_CHANGE and no side effects occur.  
*/
inline int stdset_allocate(char **mem, size_t new_cap, size_t *high_cap,
			   size_t *low_cap, size_t vsize) {
  size_t new_cap_bytes;

  if (new_cap == *high_cap)
    return STD_NO_MEM_CHANGE;

  if (new_cap == 0) {
    *mem      = 0;
    *high_cap = 0;
    *low_cap  = 0;
  } else if ((new_cap_bytes = new_cap * vsize) / vsize == new_cap && /* check for overflow */
	     (*mem = (char*) malloc(new_cap_bytes)) != 0) {          /* check for success */
    *high_cap = new_cap;
    *low_cap  = new_cap >> 2;
  } else
    return STD_MEM_FAILURE;

  return STD_MEM_CHANGE;
}

/* "rounds" roundme to the closest power of 2 <= roundme, 
   returns zero if roundme is zero */
inline size_t stdround_down_pow2(size_t roundme) {
  if (roundme == 0)
    return 0;

  return (size_t) 0x1 << stdlg_round_down_pow2(roundme);
}

/* "rounds" roundme to the closest power of 2 >= roundme,
   returns zero if that number can't be fit in a size_t */
inline size_t stdround_up_pow2(size_t roundme) {
  return (size_t) 0x1 << stdlg_round_up_pow2(roundme);
}

/* returns the lg of round_down_pow2, returns STD_SIZE_T_MAX if
   roundme is zero */
inline size_t stdlg_round_down_pow2(size_t roundme) {
  size_t shift = 0;

  if (roundme == 0)  /* should be negative infinity */
    return STD_SIZE_T_MAX;

  for (; roundme != 1; roundme >>= 1, ++shift);

  return shift;
}

/* returns the lg of round_up_pow2, except that it returns
   a correct value when round_up_pow2 returns zero */
inline size_t stdlg_round_up_pow2(size_t roundme) {
  if (roundme == 0 || roundme == 1)
    return 0;

  return stdlg_round_down_pow2(roundme - 1) + 1;
}

/* this fcn returns a power of 2 that is between 4/3 * request_size
   and 8/3 * request_size; often used when resizing a table that
   has to be a power of 2 and a random request for resize comes in
*/
inline size_t stdgood_pow2_cap(size_t request_size) {
  size_t shift, base_pow2, low_base_pow2;

  if (!request_size)
    return 0;
  else if (request_size == 1)
    return 2;

  shift         = stdlg_round_down_pow2(request_size);
  base_pow2     = (size_t) 0x1 << shift;
  low_base_pow2 = (size_t) 0x1 << (shift - 1);

  if (request_size < base_pow2 + low_base_pow2)
    return base_pow2 << 1;
  else
    return base_pow2 << 2;
}
