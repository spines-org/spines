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

#ifndef stdutil_h_2000_01_17_16_00_08_jschultz_at_cnds_jhu_edu
#define stdutil_h_2000_01_17_16_00_08_jschultz_at_cnds_jhu_edu

#include <stdio.h>
#include <stdutil/stddefines.h>

#ifdef __cplusplus
extern "C" {
#endif

/* debug printf fcn */
int std_stkfprintf(FILE *stream, int entering, const char *fmt, ...);

/* in-place endian flippers */
inline void stdflip_endian16(void *dst);
inline void stdflip_endian32(void *dst);
inline void stdflip_endian64(void *dst);
inline void stdflip_endian_n(void *dst, size_t n);

/* uniform random number generators */
#if defined(stduint16) && defined(stduint32)
# define STDRAND32_EXISTS
inline stduint32 stdrand32(stduint16 x[3]);
inline void      stdrand32_dseed(stduint16 x[3], stduint32 seed);
inline void      stdrand32_seed(stduint16 x[3], stduint32 seed);

# if defined(stduint64)
#  define STDRAND64_EXISTS
inline stduint64 stdrand64(stduint32 x[3]);
inline void      stdrand64_dseed(stduint32 x[3], stduint64 seed);
inline void      stdrand64_seed(stduint32 x[3], stduint64 seed);
# endif

# if defined(stdhsize_t) && (SIZEOF_SIZE_T == 4 || (SIZEOF_SIZE_T == 8 && defined(stduint64)))
#  define STDRAND_EXISTS
inline size_t stdrand(stdhsize_t x[3]);
inline void   stdrand_dseed(stdhsize_t x[3], size_t seed);
inline void   stdrand_seed(stdhsize_t x[3], size_t seed);
# endif
#endif

/* array allocation routines */
inline int stdget_mem(char **mem, size_t new_size, size_t *high_cap, 
		      size_t *low_cap, size_t vsize);
inline int stdgrow_mem(char **mem, size_t new_size, size_t *high_cap, 
		       size_t *low_cap, size_t vsize);
inline int stdshrink_mem(char **mem, size_t new_size, size_t *high_cap,
			 size_t *low_cap, size_t vsize);

inline int stdauto_allocate(char **mem, size_t new_size, size_t *high_cap,
			    size_t *low_cap, size_t vsize);
inline int stdset_allocate(char **mem, size_t new_cap, size_t *high_cap,
			   size_t *low_cap, size_t vsize);

/* powers of 2 utilities */
inline size_t stdround_down_pow2(size_t roundme);
inline size_t stdround_up_pow2(size_t roundme);
inline size_t stdlg_round_down_pow2(size_t roundme);
inline size_t stdlg_round_up_pow2(size_t roundme);
inline size_t stdgood_pow2_cap(size_t request_size);

#ifdef __cplusplus
}
#endif

#endif
