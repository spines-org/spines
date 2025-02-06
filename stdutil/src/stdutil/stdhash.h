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

#ifndef stdhash_h_2000_02_14_16_22_38_jschultz_at_cnds_jhu_edu
#define stdhash_h_2000_02_14_16_22_38_jschultz_at_cnds_jhu_edu

#include <stdutil/stdutil.h>

/* defined in stdutil.h: only if all the necessary types exist can you use stdhash */
#ifdef STDRAND_EXISTS 

/* static initializers with no safety checks */
# define STDHASH_STATIC_CONSTRUCT(sizeof_key, sizeof_val, stdequals_fcn, stdhcode_fcn)
# define STDHASH_STATIC_CONSTRUCT2(sizeof_key, sizeof_val, stdequals_fcn, stdhcode_fcn, dseed)

# include <stdutil/stddefines.h>
# include <stdutil/stdkvp.h>
# include <stdutil/stdhash_p.h>

# ifdef __cplusplus
extern "C" {
# endif

/* stdhash_it: must first be initialized by a stdhash iterator fcn (see below) */
inline void    *stdhash_it_key(const stdhash_it *it);
inline void    *stdhash_it_val(const stdhash_it *it);
inline stdkvp  *stdhash_it_kvp(const stdhash_it *it);
inline stdbool stdhash_it_equals(const stdhash_it *it1, const stdhash_it *it2);
inline stdbool stdhash_it_is_begin(const stdhash_it *it);
inline stdbool stdhash_it_is_end(const stdhash_it *it);

inline stdhash_it *stdhash_it_seek_begin(stdhash_it *it);
inline stdhash_it *stdhash_it_seek_end(stdhash_it *it);
inline stdhash_it *stdhash_it_next(stdhash_it *it);
inline stdhash_it *stdhash_it_advance(stdhash_it *it, size_t num_advance);
inline stdhash_it *stdhash_it_prev(stdhash_it *it);
inline stdhash_it *stdhash_it_retreat(stdhash_it *it, size_t num_retreat);

inline stdhash_it *stdhash_it_keyed_next(stdhash_it *it);

/* stdhash */
/* Constructors, Destructor */
inline int  stdhash_construct(stdhash *h, size_t sizeof_key, size_t sizeof_val, 
			      stdequals_fcn key_eq, stdhcode_fcn key_hcode);
inline int  stdhash_construct2(stdhash *h, size_t sizeof_key, size_t sizeof_val, 
			       stdequals_fcn key_eq, stdhcode_fcn key_hcode, size_t dseed);
inline int  stdhash_copy_construct(stdhash *dst, const stdhash *src);
inline void stdhash_destruct(stdhash *h);

/* Iterator Interface */
inline stdhash_it *stdhash_begin(const stdhash *h, stdhash_it *it);
inline stdhash_it *stdhash_last(const stdhash *h, stdhash_it *it);
inline stdhash_it *stdhash_end(const stdhash *h, stdhash_it *it);
inline stdhash_it *stdhash_get(const stdhash *h, stdhash_it *it, size_t elem_num);

/* Size and Capacity Information */
inline size_t  stdhash_size(const stdhash *h);
inline stdbool stdhash_empty(const stdhash *h);

inline size_t stdhash_max_size(const stdhash *h);
inline size_t stdhash_key_size(const stdhash *h);
inline size_t stdhash_val_size(const stdhash *h);
inline size_t stdhash_kvp_size(const stdhash *h);

/* Size and Capacity Operations */
inline int stdhash_clear(stdhash *h);
inline int stdhash_reserve(stdhash *h, size_t num_elems);
inline int stdhash_rehash(stdhash *h);

/* Hash Operations: O(1) expected, O(n) worst case */
inline stdhash_it *stdhash_find(const stdhash *h, stdhash_it *it, const void *key);
inline stdhash_it *stdhash_insert(stdhash *h, stdhash_it *it, const void *key, const void *val);
inline stdhash_it *stdhash_erase(stdhash_it *erase);
inline int        stdhash_erase_key(stdhash *h, const void *key);

/* Equals and hashcode function pairs for commonly used key types */
/* key type is int or uint */
stdbool stdhash_int_equals(const void *int1, const void *int2);
size_t  stdhash_int_hashcode(const void *kint);

/* key type is a C string (key type is pointer to a null terminated array of characters) */
stdbool stdhash_str_equals(const void *str_ptr1, const void *str_ptr2);
size_t  stdhash_str_hashcode(const void *str_ptr);

# ifdef __cplusplus
}
# endif

#endif /* ifdef STDRAND_EXISTS */
#endif
