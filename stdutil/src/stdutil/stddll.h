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

#ifndef stddll_h_2000_02_14_16_22_38_jschultz_at_cnds_jhu_edu
#define stddll_h_2000_02_14_16_22_38_jschultz_at_cnds_jhu_edu

#include <stdutil/stddefines.h>
#include <stdutil/stddll_p.h>

#ifdef __cplusplus
extern "C" {
#endif

/* stddll_it: must first be initialized by a stddll iterator fcn (see below) */
inline void    *stddll_it_val(const stddll_it *it);
inline stdbool stddll_it_equals(const stddll_it *it1, const stddll_it *it2);
inline stdbool stddll_it_is_begin(const stddll_it *it);
inline stdbool stddll_it_is_end(const stddll_it *it);  

inline stddll_it *stddll_it_seek_begin(stddll_it *it);
inline stddll_it *stddll_it_seek_end(stddll_it *it);
inline stddll_it *stddll_it_next(stddll_it *it);
inline stddll_it *stddll_it_advance(stddll_it *it, size_t num_advance);
inline stddll_it *stddll_it_prev(stddll_it *it);
inline stddll_it *stddll_it_retreat(stddll_it *it, size_t num_retreat);

/* stddll */
/* Constructors, Destructor */
inline int  stddll_construct(stddll *l, size_t sizeof_val);
inline int  stddll_copy_construct(stddll *dst, const stddll *src);
inline void stddll_destruct(stddll *l);

/* Iterator Interface */
inline stddll_it *stddll_begin(const stddll *l, stddll_it *it);
inline stddll_it *stddll_last(const stddll *l, stddll_it *it);
inline stddll_it *stddll_end(const stddll *l, stddll_it *it);
inline stddll_it *stddll_get(const stddll *l, stddll_it *it, size_t index);

/* Size Information */
inline size_t  stddll_size(const stddll *l);
inline stdbool stddll_empty(const stddll *l);

inline size_t stddll_max_size(const stddll *l);
inline size_t stddll_val_size(const stddll *l);

/* Size Operations */
inline int stddll_resize(stddll *l, size_t num_elems);
inline int stddll_clear(stddll *l);

/* Stack Operations: O(1) operations */
inline int stddll_push_front(stddll *l, const void *val);
inline int stddll_pop_front(stddll *l);
inline int stddll_push_back(stddll *l, const void *val);
inline int stddll_pop_back(stddll *l);

inline int stddll_multi_push_front(stddll *l, const void *vals, size_t num_push);
inline int stddll_multi_pop_front(stddll *l, size_t num_pop);
inline int stddll_multi_push_back(stddll *l, const void *vals, size_t num_push);
inline int stddll_multi_pop_back(stddll *l, size_t num_pop);

/* List Operations: O(1) operations */
inline stddll_it *stddll_insert(stddll_it *it, const void *val);
inline stddll_it *stddll_erase(stddll_it *it);

inline stddll_it *stddll_repeat_insert(stddll_it *it, const void *val, size_t num_times);
inline stddll_it *stddll_multi_insert(stddll_it *it, const void *vals, size_t num_insert);
inline stddll_it *stddll_multi_erase(stddll_it *it, size_t num_erase);

#ifdef __cplusplus
}
#endif

#endif
