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

#ifndef stdarr_h_2000_01_26_11_38_04_jschultz_at_cnds_jhu_edu
#define stdarr_h_2000_01_26_11_38_04_jschultz_at_cnds_jhu_edu

/* stdarr static initializer with no safety checks: sizeof_val should be a size_t */
#define STDARR_STATIC_CONSTRUCT(sizeof_val)

#include <stdutil/stddefines.h>
#include <stdutil/stdarr_p.h>

#ifdef __cplusplus
extern "C" {
#endif

/* stdarr_it: must first be initialized by a stdarr iterator fcn (see below) */
inline void       *stdarr_it_val(const stdarr_it *it);
inline stdbool    stdarr_it_equals(const stdarr_it *it1, const stdarr_it *it2);
inline stdssize_t stdarr_it_compare(const stdarr_it *it1, const stdarr_it *it2);
inline stdbool    stdarr_it_is_begin(const stdarr_it *it);
inline stdbool    stdarr_it_is_end(const stdarr_it *it);

inline stdarr_it *stdarr_it_seek_begin(stdarr_it *it);
inline stdarr_it *stdarr_it_seek_end(stdarr_it *it);
inline stdarr_it *stdarr_it_next(stdarr_it *it);
inline stdarr_it *stdarr_it_advance(stdarr_it *it, size_t num_advance);
inline stdarr_it *stdarr_it_prev(stdarr_it *it);
inline stdarr_it *stdarr_it_retreat(stdarr_it *it, size_t num_retreat);
inline stdarr_it *stdarr_it_offset(stdarr_it *it, stdssize_t offset);

/* stdarr */
/* Constructors, Destructor */
inline int  stdarr_construct(stdarr *arr, size_t sizeof_val);
inline int  stdarr_copy_construct(stdarr *dst, const stdarr *src);
inline void stdarr_destruct(stdarr *arr);

/* Iterator Initializers */
inline stdarr_it *stdarr_begin(const stdarr *arr, stdarr_it *it);
inline stdarr_it *stdarr_last(const stdarr *arr, stdarr_it *it);
inline stdarr_it *stdarr_end(const stdarr *arr, stdarr_it *it);
inline stdarr_it *stdarr_get(const stdarr *arr, stdarr_it *it, size_t elem_num);

/* Size and Capacity Information */
inline size_t  stdarr_size(const stdarr *arr);
inline stdbool stdarr_empty(const stdarr *arr);
inline size_t  stdarr_high_capacity(const stdarr *arr);
inline size_t  stdarr_low_capacity(const stdarr *arr);

inline size_t stdarr_max_size(const stdarr *arr);
inline size_t stdarr_val_size(const stdarr *arr);

/* Size and Capacity Operations */
inline int stdarr_resize(stdarr *arr, size_t num_elems);
inline int stdarr_clear(stdarr *arr);

inline int stdarr_set_capacity(stdarr *arr, size_t num_elems);
inline int stdarr_reserve(stdarr *arr, size_t num_elems);
inline int stdarr_shrink_fit(stdarr *arr);

/* Stack Operations: amoritized O(1) operations, worst case O(n) */
inline int stdarr_push_back(stdarr *arr, const void *val);
inline int stdarr_pop_back(stdarr *arr);

inline int stdarr_multi_push_back(stdarr *arr, const void *vals, size_t num_push);
inline int stdarr_multi_pop_back(stdarr *arr, size_t num_pop);

/* List Operations: O(n) operations */
inline stdarr_it *stdarr_insert(stdarr_it *it, const void *val);
inline stdarr_it *stdarr_erase(stdarr_it *it);

inline stdarr_it *stdarr_repeat_insert(stdarr_it *it, const void *val, size_t num_times);
inline stdarr_it *stdarr_multi_insert(stdarr_it *it, const void *vals, size_t num_insert);
inline stdarr_it *stdarr_multi_erase(stdarr_it *it, size_t num_erase);

/* Data Structure Options */
inline stdbool stdarr_get_auto_alloc(const stdarr *arr);
inline void    stdarr_set_auto_alloc(stdarr *arr, stdbool use_auto_alloc);

#ifdef __cplusplus
}
#endif

#endif
