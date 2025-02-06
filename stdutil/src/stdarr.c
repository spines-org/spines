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

#include <stdlib.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#include <string.h>
#include <stdutil/stdutil.h>
#include <stdutil/stderror.h>
#include <stdutil/stdarr.h>

#ifdef STD_CONSTRUCT_CHECKS
# define IS_ARR_INITED(arr) ((arr)->init_val == STDARR_INITED)
# define INIT_ARR(arr)      ((arr)->init_val = STDARR_INITED)
# define UNINIT_ARR(arr)    ((arr)->init_val = ~STDARR_INITED)
# define IS_IT_INITED(it)   ((it)->it_init_val == STDARR_IT_INITED)
# define INIT_IT(it)        ((it)->it_init_val = STDARR_IT_INITED)
#else
# define IS_ARR_INITED(arr)
# define INIT_ARR(arr)
# define UNINIT_ARR(arr)
# define IS_IT_INITED(it)
# define INIT_IT(it)
#endif

/* does the iterator point at a legal position in the array? */
#define IS_LEGAL_IT(it) \
((it)->val >= (it)->arr->begin && (it)->val <= (it)->arr->end)

/* does the iterator point at a valid (not end), legal position in the array? */
#define IS_VALID_IT(it) \
((it)->val >= (it)->arr->begin && (it)->val < (it)->arr->end)

/* this fcn inserts num_insert element positions before the position pointed */
/* at by it the fcn updates size and end and if necessary grows the array */
inline static int insert_space(stdarr_it *it, size_t num_insert) {
  char *mem;
  stdarr *arr;
  size_t new_size, delta, prior, after;

  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));

  arr      = it->arr;
  new_size = arr->size + num_insert;
  delta    = num_insert * arr->vsize;
  after    = (size_t) (arr->end - it->val);

  /* grow the array if necessary: update high_cap and low_cap and alloc into mem */
  /* see stdutil.c for stdgrow_mem */
  switch (arr->auto_alloc ? 
	  stdgrow_mem(&mem, new_size, &arr->high_cap, &arr->low_cap, arr->vsize) : 
	  STD_NO_MEM_CHANGE) {
  case STD_NO_MEM_CHANGE:                        /* array didn't grow */
    STD_BOUNDS_CHECK(new_size <= arr->high_cap); /* if !auto_alloc check if legal */
    memmove(it->val + delta, it->val, after);    /* shift array */
    arr->end += delta;
    arr->size = new_size;
    return STD_SUCCESS;

  case STD_MEM_CHANGE:                           /* array grew and alloc'ed into mem */
    prior = (size_t) (it->val - arr->begin);
    if (arr->begin) {                            /* if array is !null then free after copying */
      memcpy(mem, arr->begin, prior);
      memcpy(mem + prior + delta, it->val, after);
      free(arr->begin);
    }
    it->val    = mem + prior;                    /* point it to same element position as before */
    arr->begin = mem;
    arr->end   = mem + new_size * arr->vsize;
    arr->size  = new_size;
    return STD_SUCCESS;

  case STD_MEM_FAILURE:
    return STD_ERROR(STD_MEM_FAILURE);

  default:
    return STD_EXCEPTION(unknown value returned from stdgrow_mem);
  }
}

/* get the size in bytes of the type of values the array this iterator points at contains */
inline static size_t sizeof_val(const stdarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  return it->arr->vsize;
}

/**************************** stdarr_it interface ************************/

inline void *stdarr_it_val(const stdarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_VALID_IT(it));
  return it->val;
}

inline stdbool stdarr_it_equals(const stdarr_it *it1, const stdarr_it *it2) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it1) && IS_IT_INITED(it2));
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(it1->arr) && IS_ARR_INITED(it2->arr));
  STD_BOUNDS_CHECK(it1->arr == it2->arr && IS_LEGAL_IT(it1) && IS_LEGAL_IT(it2));
  return it1->val == it2->val;
}

inline stdssize_t stdarr_it_compare(const stdarr_it *it1, const stdarr_it *it2) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it1) && IS_IT_INITED(it2));
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(it1->arr) && IS_ARR_INITED(it2->arr));
  STD_BOUNDS_CHECK(it1->arr == it2->arr && IS_LEGAL_IT(it1) && IS_LEGAL_IT(it2));
  return (it1->val - it2->val) / it1->arr->vsize;
}

inline stdbool stdarr_it_is_begin(const stdarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  return it->val == it->arr->begin;
}

inline stdbool stdarr_it_is_end(const stdarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  return it->val == it->arr->end;
}

inline stdarr_it *stdarr_it_seek_begin(stdarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  it->val = it->arr->begin;
  return it;
}

inline stdarr_it *stdarr_it_seek_end(stdarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  it->val = it->arr->end;
  return it;
}

inline stdarr_it *stdarr_it_next(stdarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_VALID_IT(it)); /* no next on end */
  it->val += it->arr->vsize;
  return it;
}

inline stdarr_it *stdarr_it_advance(stdarr_it *it, size_t num_advance) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  it->val += num_advance * it->arr->vsize;
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  return it;
}

inline stdarr_it *stdarr_it_prev(stdarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it) && it->val != it->arr->begin);
  it->val -= it->arr->vsize;
  return it;
}

inline stdarr_it *stdarr_it_retreat(stdarr_it *it, size_t num_retreat) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  it->val -= num_retreat * it->arr->vsize;
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  return it;
}

inline stdarr_it *stdarr_it_offset(stdarr_it *it, stdssize_t offset) {
  return (offset >= 0 ? stdarr_it_advance(it, (size_t) offset) :
	  stdarr_it_retreat(it, (size_t) -offset));
}

/************************** stdarr interface ******************************/
/* Constructors, Destructor */
inline int stdarr_construct(stdarr *arr, size_t sizeof_val) { 
  STD_CONSTRUCT_CHECK(!IS_ARR_INITED(arr));

  if (!(arr->vsize = sizeof_val))
    return STD_ERROR(STD_ILLEGAL_PARAM);

  arr->begin      = 0;
  arr->end        = 0;
  arr->size       = 0;
  arr->high_cap   = 0;
  arr->low_cap    = 0;
  arr->auto_alloc = stdtrue;

  INIT_ARR(arr);

  return STD_SUCCESS;
}

inline int stdarr_copy_construct(stdarr *dst, const stdarr *src) { 
  size_t byte_size;
  int ret;

  STD_CONSTRUCT_CHECK(IS_ARR_INITED(src));

  if ((ret = stdarr_construct(dst, src->vsize)) != STD_SUCCESS)
    return STD_ERROR(ret);

  if ((dst->auto_alloc = src->auto_alloc)) {
    if (stdgrow_mem(&dst->begin, src->size, &dst->high_cap, 
		    &dst->low_cap, src->vsize) == STD_MEM_FAILURE)
      return STD_ERROR(STD_MEM_FAILURE);
  } else {
    if (stdset_allocate(&dst->begin, src->high_cap, &dst->high_cap, 
			&dst->low_cap, src->vsize) == STD_MEM_FAILURE)
      return STD_ERROR(STD_MEM_FAILURE);
  }
  byte_size = (size_t) (src->end - src->begin);
  memcpy(dst->begin, src->begin, byte_size);

  dst->end  = dst->begin + byte_size;
  dst->size = src->size;

  return STD_SUCCESS;
}

inline void stdarr_destruct(stdarr *arr) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));

  if (arr->begin) 
    free(arr->begin); 

  UNINIT_ARR(arr);
}

/* Iterator Access Functions */
inline stdarr_it *stdarr_begin(const stdarr *arr, stdarr_it *it) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  it->arr = (stdarr*) arr;
  it->val = (char*) arr->begin;
  INIT_IT(it);
  return it;
}

inline stdarr_it *stdarr_last(const stdarr *arr, stdarr_it *it) {
  return stdarr_it_prev(stdarr_end(arr, it));
}

inline stdarr_it *stdarr_end(const stdarr *arr, stdarr_it *it) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  it->arr = (stdarr*) arr;
  it->val = (char*) arr->end;
  INIT_IT(it);
  return it;
}

inline stdarr_it *stdarr_get(const stdarr *arr, stdarr_it *it, size_t elem_num) { 
  return stdarr_it_advance(stdarr_begin(arr, it), elem_num);
}

/* Size and Capacity Information */
inline size_t stdarr_size(const stdarr *arr) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  return arr->size; 
}

inline stdbool stdarr_empty(const stdarr *arr) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  return arr->size == 0; 
}

inline size_t stdarr_high_capacity(const stdarr *arr) {
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  return arr->high_cap;
}

/* we reallocate when size equals low_cap (see stdshrink_mem) */
inline size_t stdarr_low_capacity(const stdarr *arr) {
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  return arr->high_cap != 0 ? arr->low_cap + 1 : 0;
}

inline size_t stdarr_max_size(const stdarr *arr) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  return STD_SIZE_T_MAX / arr->vsize; 
}

inline size_t stdarr_val_size(const stdarr *arr) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  return arr->vsize; 
}

/* Size and Capacity Operations */
inline int stdarr_resize(stdarr *arr, size_t num_elems) { 
  char *mem;

  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));

  switch(arr->auto_alloc ? 
	 stdget_mem(&mem, num_elems, &arr->high_cap, &arr->low_cap, arr->vsize) :
	 STD_NO_MEM_CHANGE) {
  case STD_NO_MEM_CHANGE:
    STD_BOUNDS_CHECK(num_elems <= stdarr_high_capacity(arr));
    arr->end  = arr->begin + num_elems * arr->vsize;
    arr->size = num_elems;
    return STD_SUCCESS;

  case STD_MEM_CHANGE:
    if (arr->begin) {
      memcpy(mem, arr->begin, STDMIN(num_elems, arr->size) * arr->vsize);
      free(arr->begin);
    }
    arr->begin = mem;
    arr->end   = mem + num_elems * arr->vsize;
    arr->size  = num_elems;
    return STD_SUCCESS;
    
  case STD_MEM_FAILURE:
    return STD_ERROR(STD_MEM_FAILURE);

  default:
    return STD_EXCEPTION(impossible value from stdget_mem);
  }
}

inline int stdarr_clear(stdarr *arr) { 
  return stdarr_resize(arr, 0);
}

inline int stdarr_set_capacity(stdarr *arr, size_t num_elems) {
  char *mem;
  size_t byte_size;

  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));

  switch(stdset_allocate(&mem, num_elems, &arr->high_cap, &arr->low_cap, arr->vsize)) {
  case STD_MEM_CHANGE:
    if (num_elems < arr->size) 
      arr->size = num_elems;

    byte_size = arr->size * arr->vsize;
    if (arr->begin) { 
      memcpy(mem, arr->begin, byte_size);
      free(arr->begin);
    }
    arr->begin = mem;
    arr->end   = mem + byte_size;
    return STD_SUCCESS;

  case STD_NO_MEM_CHANGE:
    return STD_SUCCESS;

  case STD_MEM_FAILURE:
    return STD_ERROR(STD_MEM_FAILURE);
    
  default: 
    return STD_EXCEPTION(impossible value from alloc_mem);
  }
}

inline int stdarr_reserve(stdarr *arr, size_t num_elems) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  return num_elems <= stdarr_high_capacity(arr) ? STD_SUCCESS :
    stdarr_set_capacity(arr, arr->auto_alloc ? num_elems << 1 : num_elems);
}

inline int stdarr_shrink_fit(stdarr *arr) {
  return stdarr_set_capacity(arr, stdarr_size(arr));
}

/* Stack Operations: amoritized O(1) operations */
inline int stdarr_push_back(stdarr *arr, const void *val) { 
  return stdarr_multi_push_back(arr, val, 1);
}

inline int stdarr_pop_back(stdarr *arr) { 
  return stdarr_multi_pop_back(arr, 1);
}

inline int stdarr_multi_push_back(stdarr *arr, const void *vals, size_t num_push) { 
  stdarr_it ret;

  if (stdarr_multi_insert(stdarr_end(arr, &ret), vals, num_push))
    return STD_SUCCESS;
  else
    return STD_ERROR(STD_MEM_FAILURE);
}

inline int stdarr_multi_pop_back(stdarr *arr, size_t num_pop) { 
  STD_CONSTRUCT_CHECK(IS_ARR_INITED(arr));
  STD_BOUNDS_CHECK(num_pop <= stdarr_size(arr));
  return stdarr_resize(arr, stdarr_size(arr) - num_pop);
}

/* List Operations: O(n) operations */
inline stdarr_it *stdarr_insert(stdarr_it *insert, const void *val) {
  return stdarr_multi_insert(insert, val, 1);
}

inline stdarr_it *stdarr_erase(stdarr_it *erase) { 
  return stdarr_multi_erase(erase, 1);
}

inline stdarr_it *stdarr_repeat_insert(stdarr_it *it, const void *val, size_t num_times) {
  char *curr;

  if (insert_space(it, num_times) != STD_SUCCESS)
    return (stdarr_it*) STD_ERROR(STD_MEM_FAILURE2);

  for (curr = it->val; num_times--; curr += it->arr->vsize)
    memcpy(curr, val, it->arr->vsize);

  return it;
}

inline stdarr_it *stdarr_multi_insert(stdarr_it *it, const void *vals, size_t num_insert) {
  if (insert_space(it, num_insert) != STD_SUCCESS)
    return (stdarr_it*) STD_ERROR(STD_MEM_FAILURE2);

  memcpy(it->val, vals, num_insert * it->arr->vsize);
  return it;
}

inline stdarr_it *stdarr_multi_erase(stdarr_it *it, size_t num_erase) {
  stdarr *arr;
  char *mem, *erase;
  size_t new_size, delta, prior, after_min;

  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_ARR_INITED(it->arr));
  arr   = it->arr;
  erase = it->val;
  delta = num_erase * arr->vsize;
  STD_BOUNDS_CHECK(erase >= arr->begin && erase + delta <= arr->end);

  new_size  = arr->size - num_erase;
  after_min = (size_t) (arr->end - (erase + delta));

  switch(arr->auto_alloc ? 
	 stdshrink_mem(&mem, new_size, &arr->high_cap, &arr->low_cap, arr->vsize) :
	 STD_NO_MEM_CHANGE) {
  case STD_NO_MEM_CHANGE:
    memmove(erase, erase + delta, after_min);
    arr->end -= delta;
    arr->size = new_size;
    return it;

  case STD_MEM_CHANGE:
    prior   = (size_t) (erase - arr->begin);
    it->val = mem + prior;
    memcpy(mem, arr->begin, prior);
    memcpy(it->val, erase + delta, after_min);
    free(arr->begin);
    arr->begin = mem;
    arr->end   = mem + new_size * arr->vsize;
    arr->size  = new_size;
    return it;

  case STD_MEM_FAILURE:
    return (stdarr_it*) STD_ERROR(STD_MEM_FAILURE2);

  default:
    return (stdarr_it*) STD_EXCEPTION(impossible value from shrink_mem);
  }
}

/* Data Structure Options */
inline stdbool stdarr_get_auto_alloc(const stdarr *arr) { 
  return arr->auto_alloc; 
}

inline void stdarr_set_auto_alloc(stdarr *arr, stdbool use_auto_alloc) { 
  arr->auto_alloc = use_auto_alloc;
}
