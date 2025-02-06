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

#ifndef stdcarr_h_2000_01_30_23_42_05_jschultz_at_cnds_jhu_edu
#define stdcarr_h_2000_01_30_23_42_05_jschultz_at_cnds_jhu_edu

/* stdcarr static initializer with no safety checks */
#define STDCARR_STATIC_CONSTRUCT(sizeof_val)

#include <stdutil/stddefines.h>
#include <stdutil/stdcarr_p.h>

#ifdef __cplusplus
extern "C" {
#endif

/* stdcarr_it: must first be initialized by a stdcarr iterator fcn (see below) */
inline void       *stdcarr_it_val(const stdcarr_it *it);
inline stdbool    stdcarr_it_equals(const stdcarr_it *it1, const stdcarr_it *it2);
inline stdssize_t stdcarr_it_compare(const stdcarr_it *it1, const stdcarr_it *it2);
inline stdbool    stdcarr_it_is_begin(const stdcarr_it *it);
inline stdbool    stdcarr_it_is_end(const stdcarr_it *it);    

inline stdcarr_it *stdcarr_it_seek_begin(stdcarr_it *it);
inline stdcarr_it *stdcarr_it_seek_end(stdcarr_it *it);
inline stdcarr_it *stdcarr_it_next(stdcarr_it *it);
inline stdcarr_it *stdcarr_it_advance(stdcarr_it *it, size_t num_advance);
inline stdcarr_it *stdcarr_it_prev(stdcarr_it *it);
inline stdcarr_it *stdcarr_it_retreat(stdcarr_it *it, size_t num_retreat);
inline stdcarr_it *stdcarr_it_offset(stdcarr_it *it, stdssize_t offset);

/* stdcarr */
/* Constructors, Destructor */
inline int  stdcarr_construct(stdcarr *carr, size_t sizeof_val);
inline int  stdcarr_copy_construct(stdcarr *dst, const stdcarr *src);
inline void stdcarr_destruct(stdcarr *carr);

/* Iterator Interface */
inline stdcarr_it *stdcarr_begin(const stdcarr *carr, stdcarr_it *it);
inline stdcarr_it *stdcarr_last(const stdcarr *carr, stdcarr_it *it);
inline stdcarr_it *stdcarr_end(const stdcarr *carr, stdcarr_it *it);
inline stdcarr_it *stdcarr_get(const stdcarr *carr, stdcarr_it *it, size_t elem_num);

/* Size and Capacity Information */
inline size_t  stdcarr_size(const stdcarr *carr);
inline stdbool stdcarr_empty(const stdcarr *carr);
inline size_t  stdcarr_high_capacity(const stdcarr *carr);
inline size_t  stdcarr_low_capacity(const stdcarr *carr);

inline size_t stdcarr_max_size(const stdcarr *carr);
inline size_t stdcarr_val_size(const stdcarr *carr);

/* Size and Capacity Operations */
inline int stdcarr_resize(stdcarr *carr, size_t num_elems);
inline int stdcarr_clear(stdcarr *carr);

inline int stdcarr_set_capacity(stdcarr *carr, size_t num_elems);
inline int stdcarr_reserve(stdcarr *carr, size_t num_elems);
inline int stdcarr_shrink_fit(stdcarr *carr);

/* Stack Operations: amoritized O(1) operations, worst case O(n) */
inline int stdcarr_push_front(stdcarr *carr, const void *val);
inline int stdcarr_pop_front(stdcarr *carr);
inline int stdcarr_push_back(stdcarr *carr, const void *val);
inline int stdcarr_pop_back(stdcarr *carr);

inline int stdcarr_multi_push_front(stdcarr *carr, const void *vals, size_t num_push);
inline int stdcarr_multi_pop_front(stdcarr *carr, size_t num_pop);
inline int stdcarr_multi_push_back(stdcarr *carr, const void *vals, size_t num_push);
inline int stdcarr_multi_pop_back(stdcarr *carr, size_t num_pop);

/* List Operations: O(n) operations */
inline stdcarr_it *stdcarr_insert(stdcarr_it *it, const void *val);
inline stdcarr_it *stdcarr_erase(stdcarr_it *it);

inline stdcarr_it *stdcarr_repeat_insert(stdcarr_it *it, const void *val, size_t num_times);
inline stdcarr_it *stdcarr_multi_insert(stdcarr_it *it, const void *vals, size_t num_insert);
inline stdcarr_it *stdcarr_multi_erase(stdcarr_it *it, size_t num_erase);

/* Data Structure Options */
inline stdbool stdcarr_get_auto_alloc(const stdcarr *carr);
inline void    stdcarr_set_auto_alloc(stdcarr *carr, stdbool use_auto_alloc);

#ifdef __cplusplus
}
#endif

#endif
