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
#include <stdutil/stdcarr.h>

#ifdef STD_CONSTRUCT_CHECKS
# define IS_CARR_INITED(carr) ((carr)->init_val == STDCARR_INITED)
# define INIT_CARR(carr)      ((carr)->init_val = STDCARR_INITED)
# define UNINIT_CARR(carr)    ((carr)->init_val = ~STDCARR_INITED)
# define IS_IT_INITED(it)     ((it)->it_init_val == STDCARR_IT_INITED)
# define INIT_IT(it)          ((it)->it_init_val = STDCARR_IT_INITED)
#else
# define IS_CARR_INITED(carr)
# define INIT_CARR(carr)
# define UNINIT_CARR(carr)
# define IS_IT_INITED(it)
# define INIT_IT(it)
#endif

/* does the iterator point at a legal position in the carray? */
/* note that endbase is not a legal place to point */
#define IS_LEGAL_IT(it) \
((it)->carr->begin <= (it)->carr->end ? \
 (it)->val  >= (it)->carr->begin && (it)->val <= (it)->carr->end : \
 ((it)->val >= (it)->carr->begin && (it)->val <  (it)->carr->endbase) || \
 ((it)->val >= (it)->carr->base  && (it)->val <= (it)->carr->end))

/* does the iterator point at a valid (not end), legal position? */
#define IS_VALID_IT(it) \
((it)->carr->begin <= (it)->carr->end ? \
 (it)->val  >= (it)->carr->begin && (it)->val < (it)->carr->end : \
 ((it)->val >= (it)->carr->begin && (it)->val < (it)->carr->endbase) || \
 ((it)->val >= (it)->carr->base  && (it)->val < (it)->carr->end))

/* copy a portion of a carray to a buffer, returns ptr to right after copied data in dst */
/* begin and end point at positions inside of carr */
inline static char *copy_to_buf(char *dst, const stdcarr *carr, char *begin, char *end) {
  stdssize_t diff = end - begin;

  if (diff >= 0)
    memcpy(dst, begin, (size_t) diff), dst += diff;
  else {
    memcpy(dst, begin, (size_t) (diff = (carr->endbase - begin))), dst += diff;
    memcpy(dst, carr->base, (size_t) (diff = (end - carr->base))), dst += diff;
  }
  return dst;
}

/* The following functions are called when the carray's size
   changes. They decide whether the capacity thresholds have been
   exceeded or not. If the thresholds have been exceeded, then they
   call the stdauto_allocate fcn that determines new capacity
   thresholds and allocates the necessary memory.

   grow_mem   - when there is an increase in size
   shrink_mem - when there is a decrease in size 

   ***Note: I require new_size < *high_cap. This implies there is
   always at least one empty element in a non-empty carray.

   Notice that we require new_size > *low_cap. This allows for
   reallocation even when low_cap is zero, which allows the carray to
   go to zero alloc'ed memory usage. 
*/
inline static int grow_mem(char **mem, size_t new_size, size_t *high_cap, 
			   size_t *low_cap, size_t vsize) {
  return (new_size < *high_cap ? STD_NO_MEM_CHANGE :
	  stdauto_allocate(mem, new_size, high_cap, low_cap, vsize));
}

inline static int shrink_mem(char **mem, size_t new_size, size_t *high_cap,
			     size_t *low_cap, size_t vsize) {
  return (new_size > *low_cap ? STD_NO_MEM_CHANGE :
	  stdauto_allocate(mem, new_size, high_cap, low_cap, vsize));
}

/* functions that can move ptrs around in a carray wo/ bounds checking. */
inline static char *forward(const stdcarr *carr, char *p, size_t bytesf) {
  return ((p += bytesf) < carr->endbase) ? p : carr->base + (p - carr->endbase);
}

inline static char *backward(const stdcarr *carr, char *p, size_t bytesb) {
  return ((p -= bytesb) >= carr->base) ? p : carr->endbase - (carr->base - p);
}

/* This function shifts all the values from 'it' and on, to the right by delta bytes. 
   It also updates end and size. This fcn does no bounds checking. */
inline static void insert_shift_right(stdcarr *carr, char *it,
				      size_t delta, size_t new_size) {
  stdssize_t diff, diff2;

  if (carr->begin <= carr->end) {
    if ((diff = carr->end + delta - carr->endbase) <= 0) /* no data wraps around */
      memmove(it + delta, it, (size_t) (carr->end - it));
    else {
      if ((diff2 = carr->end - it) <= diff) /* shifted data can fit between base and new end */
	memcpy(carr->base + diff - diff2, it, (size_t) diff2);
      else {
	memcpy(carr->base, carr->end - diff, (size_t) diff);
	memmove(it + delta, it, (size_t) (diff2 - diff));
      }
    }
  } else { /* space exists between end and begin for insertion */
    if ((diff = carr->end - it) >= 0)                    /* it is below end */
      memmove(it + delta, it, (size_t) diff);
    else {
      memmove(carr->base + delta, carr->base, (size_t) (carr->end - carr->base));
      if ((size_t) (diff = carr->endbase - it) <= delta) /* fits into newly opened area */
	memcpy(carr->base + delta - diff, it, (size_t) diff);
      else {
	memcpy(carr->base, carr->endbase - delta, delta);
	memmove(it + delta, it, (size_t) (diff - delta));
      }
    }
  }
  carr->size = new_size;
  carr->end  = forward(carr, carr->end, delta);
}

/* This function shifts all values before 'it' to the left by delta bytes. 
   It also updates begin and size. This fcn does no bounds checking. */
inline static void insert_shift_left(stdcarr *carr, char *it, 
				     size_t delta, size_t new_size) {
  stdssize_t diff, diff2;

  if (carr->begin <= carr->end) {
    if ((diff = carr->base - (carr->begin - delta)) <= 0) /* new begin doesn't wrap around */
      memmove(carr->begin - delta, carr->begin, (size_t) (it - carr->begin));
    else {
      if ((diff2 = it - carr->begin) <= diff) /* shifted data fits between new begin and endbase */
	memcpy(carr->endbase - diff, carr->begin, (size_t) diff2);
      else {
	memcpy(carr->endbase - diff, carr->begin, (size_t) diff);
	memmove(carr->base, carr->begin + diff, (size_t) (diff2 - diff));
      }
    }
  } else { /* space exists between end and begin for insertion */
    if (it >= carr->begin)                                /* it is above begin */
      memmove(carr->begin - delta, carr->begin, (size_t) (it - carr->begin));
    else {
      memmove(carr->begin - delta, carr->begin, (size_t) (carr->endbase - carr->begin));
      if ((size_t) (diff = it - carr->base) <= delta)     /* fits into newly opened area */
	memcpy(carr->endbase - delta, carr->base, (size_t) diff);
      else {
	memcpy(carr->endbase - delta, carr->base, delta);
	memmove(carr->base, carr->base + delta, (size_t) (diff - delta));
      }
    }
  }
  carr->size  = new_size;
  carr->begin = backward(carr, carr->begin, delta);
}

/* This function first determines if an insertion will require a
   reallocation. If reallocation isn't needed, it calls the specified
   array shift function. If reallocation is called for, it does it and
   copies the values from carr to the new carray while creating the
   open space requested. The members of carr are updated. This
   function returns a pointer to the beginning (leftmost value) of the
   insertion region or 0 on memory failure. Inserting into an empty
   array makes the return value be begin. Note that zero indicates
   failure, except in the case when the capacity before calling is
   zero and the insert size is zero, then it indicates success!  
*/
inline static int insert_shift(stdcarr *carr, char **itp, size_t delta, 
			       size_t new_size, stdbool shift_right) {
  char *mem;

  switch (carr->auto_alloc ? 
	  grow_mem(&mem, new_size, &carr->high_cap, &carr->low_cap, carr->vsize) :
	  STD_NO_MEM_CHANGE) {
  case STD_NO_MEM_CHANGE:
    STD_BOUNDS_CHECK(new_size <= stdcarr_high_capacity(carr));
    if (shift_right)
      insert_shift_right(carr, *itp, delta, new_size);
    else {
      insert_shift_left(carr, *itp, delta, new_size);
      *itp = backward(carr, *itp, delta);
    }
    return STD_SUCCESS;

  case STD_MEM_CHANGE:
    if (carr->base) {
      char *insert_begin = *itp;

      *itp = copy_to_buf(mem, carr, carr->begin, insert_begin);
      copy_to_buf(*itp + delta, carr, insert_begin, carr->end);
      free(carr->base);
    } else
      *itp = mem;

    carr->base    = mem;
    carr->endbase = mem + carr->high_cap * carr->vsize;
    carr->begin   = mem;
    carr->end     = mem + new_size * carr->vsize;
    carr->size    = new_size;
    return STD_SUCCESS;

  case STD_MEM_FAILURE:
    return STD_MEM_FAILURE;

  default:
    return STD_EXCEPTION(impossible value returned from grow_mem);
  }
}

/* This function shifts values from the right to the left into erased values.
   erase_shift_left(__****----1***___) =>  __****1***_______ (delta == 4 values)
   Erase_end points to one past the last value to be erased.
   Legend: _ = empty slot, * = occupied slot, 1 = erase_end, - = to be deleted. 
*/
inline static void erase_shift_left(stdcarr *carr, char *erase_end,
				    size_t delta, size_t new_size) {
  stdssize_t diff, diff2;
  char *erase_begin = erase_end - delta;  /* may point outside valid memory range */

  if ((diff = carr->end - erase_end) >= 0) {     /* end is above erase_end */
    /* diff: number of bytes from erase_end to end of array */
    if ((diff2 = carr->base - erase_begin) <= 0) /* erase region doesn't wrap around */
      memmove(erase_begin, erase_end, (size_t) diff);
    else {
      /* diff2: number of bytes that are erased off endbase portion of array */
      erase_begin = carr->endbase - diff2;       /* now erase_begin points in valid range */
      if (diff <= diff2)       /* data to shift fits between erase_begin and endbase */
	memcpy(erase_begin, erase_end, (size_t) diff);
      else {
	memcpy(erase_begin, erase_end, (size_t) diff2);
	memmove(carr->base, erase_end + diff2, (size_t) (diff - diff2));
      }
    }
  } else {
    diff = carr->endbase - erase_end;
    memmove(erase_begin, erase_end, (size_t) diff);
    erase_begin += diff;
    if ((diff = carr->end - carr->base) <= delta)
      memcpy(erase_begin, carr->base, (size_t) diff);
    else {
      memcpy(erase_begin, carr->base, delta);
      memmove(carr->base, carr->base + delta, (size_t) (diff - delta));
    }
  }
  carr->size = new_size;
  carr->end  = backward(carr, carr->end, delta);
}

/* This function shifts values from the left to the right into erased values.
   erase_shift_right(__**1---*****___) => ______*******___ (delta == 4 values)
   Legend: _ = empty slot, * = occupied slot, 1 = it, - = to be deleted. (it deleted) */
inline static void erase_shift_right(stdcarr *carr, char *erase_begin,
				     size_t delta, size_t new_size) {
  stdssize_t diff, diff2, diff3;
  char *erase_end = erase_begin + delta;   /* may point outside valid range */

  if ((diff = erase_begin - carr->begin) >= 0) {  /* erase_begin is above begin */
    if ((diff2 = erase_end - carr->endbase) <= 0) /* erase region doesn't wrap around */
      memmove(carr->begin + delta, carr->begin, (size_t) diff); 
    else {
      if ((diff3 = diff2 - diff) >= 0) /* data to shift fits between base and erase_end */
	memcpy(carr->base + diff3, carr->begin, (size_t) diff);
      else {
	memcpy(carr->base, erase_begin - diff2, (size_t) diff2);
	memmove(carr->begin + delta, carr->begin, (size_t) (diff - diff2));
      }
    }
  } else {
    diff = erase_begin - carr->base;
    erase_end -= diff;
    memmove(erase_end, carr->base, (size_t) diff);
    if ((size_t) (diff = carr->endbase - carr->begin) <= delta) /* fits into newly opened area */
      memcpy(erase_end - diff, carr->begin, (size_t) diff);
    else {
      memcpy(carr->base, carr->endbase - delta, delta);
      memmove(carr->begin + delta, carr->begin, (size_t) (diff - delta));
    }
  }
  carr->size  = new_size;
  carr->begin = forward(carr, carr->begin, delta);
}

/* This function determines if an erasure will require a reallocation
   or not. If reallocation isn't needed, it calls the specified array
   shift function. If reallocation is called for, it does it and
   copies the values to the new carray while deleting the proper
   elements. The boolean parameter erase_begin indicates whether 'it'
   points to the beginning of the erase sequence or to one past the
   end of the erase sequence. This parameter also determines whether
   we erase_shift_right or erase_shift_left. The members of carr are
   updated appropriately. Returns ptr to what shifted into 'it's place
   in the sequence. Note that zero indicates failure, except in the
   case when the capacity goes to zero, then it indicates success!  
*/
inline static int erase_shift(stdcarr *carr, char **itp, size_t delta, 
			      size_t new_size, stdbool shift_right) {
  char *mem;

  switch (carr->auto_alloc ? 
	  shrink_mem(&mem, new_size, &carr->high_cap, &carr->low_cap, carr->vsize) :
	  STD_NO_MEM_CHANGE) {
  case STD_NO_MEM_CHANGE:
    if (shift_right) { 
      erase_shift_right(carr, *itp, delta, new_size);
      *itp = forward(carr, *itp, delta);
    } else
      /* erase_shift_left is implemented to use a pointer to the end of the erase
	 region, so we pass forward(carr, *itp, delta) here... 
	 This causes some minor inefficiency (most notably for back_pop operations) 
	 for shift_left operations but to keep fcn interfaces understandable (e.g. - 
	 what shift_right and what itp represent) we do a forward(carr, 
	 backward(carr, end_ptr, delta), delta) for those pop ops... :( 
      */
      erase_shift_left(carr, forward(carr, *itp, delta), delta, new_size);

    return STD_SUCCESS;

  case STD_MEM_CHANGE: {
    char *erase_end = forward(carr, *itp, delta);

    *itp = copy_to_buf(mem, carr, carr->begin, *itp);
    copy_to_buf(*itp, carr, erase_end, carr->end);
    free(carr->base);               /* must have had values to re-allocate on a shrink_mem() */
    carr->base    = mem;
    carr->endbase = mem + carr->high_cap * carr->vsize;
    carr->begin   = mem;
    carr->end     = mem + new_size * carr->vsize;
    carr->size    = new_size;
    return STD_SUCCESS;
  }
  case STD_MEM_FAILURE:
    return STD_MEM_FAILURE;

  default:
    return STD_EXCEPTION(impossible value returned from shrink_mem);
  }
}

inline static size_t sizeof_val(const stdcarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  return it->carr->vsize;
}

/************************** stdcarr_it interface *******************************/

inline void *stdcarr_it_val(const stdcarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  STD_BOUNDS_CHECK(IS_VALID_IT(it));
  return it->val;
}

inline stdbool stdcarr_it_equals(const stdcarr_it *it1, const stdcarr_it *it2) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it1) && IS_IT_INITED(it2));
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(it1->carr) && IS_CARR_INITED(it2->carr));
  STD_BOUNDS_CHECK(it1->carr == it2->carr && IS_LEGAL_IT(it1) && IS_LEGAL_IT(it2));
  return it1->val == it2->val;
}

inline stdssize_t stdcarr_it_compare(const stdcarr_it *it1, const stdcarr_it *it2) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it1) && IS_IT_INITED(it2));
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(it1->carr) && IS_CARR_INITED(it2->carr));
  STD_BOUNDS_CHECK(it1->carr == it2->carr && IS_LEGAL_IT(it1) && IS_LEGAL_IT(it2));

  if (it1->val >= it1->carr->begin && it2->val < it1->carr->begin)
    /* return a negative answer */
    return ((it1->val - it1->carr->endbase) + 
	    (it1->carr->base - it2->val)) / it1->carr->vsize;
  else if (it1->val < it1->carr->begin && it2->val >= it1->carr->begin)
    /* return a positive answer */
    return ((it2->carr->endbase - it2->val) + 
	    (it1->val - it2->carr->base)) / it1->carr->vsize;
  
  /* either both are above begin or both are below begin */ 
  return (it1->val - it2->val) / it1->carr->vsize;
}

inline stdbool stdcarr_it_is_begin(const stdcarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  return it->val == it->carr->begin;
}

inline stdbool stdcarr_it_is_end(const stdcarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));
  return it->val == it->carr->end;
}

inline stdcarr_it *stdcarr_it_seek_begin(stdcarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  it->val = it->carr->begin;
  return it;
}

inline stdcarr_it *stdcarr_it_seek_end(stdcarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  it->val = it->carr->end;
  return it;
}

inline stdcarr_it *stdcarr_it_next(stdcarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  STD_BOUNDS_CHECK(IS_VALID_IT(it));
  it->val = forward(it->carr, it->val, it->carr->vsize);
  return it;
}

inline stdcarr_it *stdcarr_it_advance(stdcarr_it *it, size_t num_advance) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
#ifdef STD_BOUNDS_CHECKS
  {
    stdcarr_it end;

    stdcarr_end(it->carr, &end);
    STD_BOUNDS_CHECK((size_t) stdcarr_it_compare(&end, it) >= num_advance);
  }
#endif
  it->val = forward(it->carr, it->val, num_advance * it->carr->vsize);
  return it;
}

inline stdcarr_it *stdcarr_it_prev(stdcarr_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it) && it->val != it->carr->begin);
  it->val = backward(it->carr, it->val, it->carr->vsize);
  return it;
}

inline stdcarr_it *stdcarr_it_retreat(stdcarr_it *it, size_t num_retreat) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
#ifdef STD_BOUNDS_CHECKS
  {
    stdcarr_it begin;

    stdcarr_begin(it->carr, &begin);
    STD_BOUNDS_CHECK((size_t) stdcarr_it_compare(it, &begin) >= num_retreat);
  }
#endif
  it->val = backward(it->carr, it->val, num_retreat * it->carr->vsize);
  return it;
}

inline stdcarr_it *stdcarr_it_offset(stdcarr_it *it, stdssize_t offset) {
  return (offset >= 0 ? stdcarr_it_advance(it, (size_t) offset) :
	  stdcarr_it_retreat(it, (size_t) -offset));
}

/****************************** stdcarr interface *************************************/
/* Constructors, Destructor */
inline int stdcarr_construct(stdcarr *carr, size_t sizeof_val) {
  STD_CONSTRUCT_CHECK(!IS_CARR_INITED(carr));

  if (!(carr->vsize = sizeof_val))
    return STD_ERROR(STD_ILLEGAL_PARAM);

  carr->base       = 0;
  carr->endbase    = 0;
  carr->begin      = 0;
  carr->end        = 0;
  carr->size       = 0;
  carr->high_cap   = 0;
  carr->low_cap    = 0;
  carr->auto_alloc = stdtrue;

  INIT_CARR(carr);

  return STD_SUCCESS;
}

inline int stdcarr_copy_construct(stdcarr *dst, const stdcarr *src) {
  int ret;

  STD_CONSTRUCT_CHECK(IS_CARR_INITED(src));

  if ((ret = stdcarr_construct(dst, src->vsize)) != STD_SUCCESS)
    return STD_ERROR(ret);

  if ((dst->auto_alloc = src->auto_alloc)) {
    if (grow_mem(&dst->base, src->size, &dst->high_cap,
		 &dst->low_cap, src->vsize) == STD_MEM_FAILURE)
      return STD_ERROR(STD_MEM_FAILURE);
  } else {
    if (stdset_allocate(&dst->base, src->high_cap, &dst->high_cap,
			&dst->low_cap, src->vsize) == STD_MEM_FAILURE)
      return STD_ERROR(STD_MEM_FAILURE);
  }
  
  dst->endbase = dst->base + dst->high_cap * dst->vsize;
  dst->begin   = dst->base;
  dst->end     = copy_to_buf(dst->base, src, src->begin, src->end);
  dst->size    = src->size;

  return STD_SUCCESS;
}

inline void stdcarr_destruct(stdcarr *carr) {
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));

  if (carr->base) 
    free(carr->base); 

  UNINIT_CARR(carr);
}

/* Iterator Interface */
inline stdcarr_it *stdcarr_begin(const stdcarr *carr, stdcarr_it *it) { 
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  it->carr = (stdcarr*) carr;
  it->val  = (char*) carr->begin;
  INIT_IT(it);
  return it;
}

inline stdcarr_it *stdcarr_last(const stdcarr *carr, stdcarr_it *it) {
  return stdcarr_it_prev(stdcarr_end(carr, it));
}

inline stdcarr_it *stdcarr_end(const stdcarr *carr, stdcarr_it *it) { 
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  it->carr = (stdcarr*) carr;
  it->val  = (char*) carr->end;
  INIT_IT(it);
  return it;
}

inline stdcarr_it *stdcarr_get(const stdcarr *carr, stdcarr_it *it, size_t elem_num) { 
  return stdcarr_it_advance(stdcarr_begin(carr, it), elem_num);
}

/* Size and Capacity Information */
inline size_t stdcarr_size(const stdcarr *carr) { 
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  return carr->size; 
}

inline stdbool stdcarr_empty(const stdcarr *carr) { 
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  return carr->size == 0; 
}

/* carray can only fit carr->high_cap - 1 values before reallocation */
inline size_t stdcarr_high_capacity(const stdcarr *carr) {
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  return carr->high_cap ? carr->high_cap - 1 : 0;
}

/* we reallocate when size equals low_cap (see comments from mem fcns) */
inline size_t stdcarr_low_capacity(const stdcarr *carr) {
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  return carr->high_cap ? carr->low_cap + 1 : 0;
}

inline size_t stdcarr_max_size(const stdcarr *carr) { 
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  return STD_SIZE_T_MAX / carr->vsize; 
}

inline size_t stdcarr_val_size(const stdcarr *carr) { 
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  return carr->vsize; 
}

/* Size and Capacity Operations */
inline int stdcarr_resize(stdcarr *carr, size_t num_elems) {
  stdssize_t delta_size;
  char *endptr;

  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));

  if ((delta_size = (num_elems - carr->size) * carr->vsize) <= 0) {
    delta_size = -delta_size;
    endptr = backward(carr, carr->end, delta_size);

    if (erase_shift(carr, &endptr, (size_t) delta_size, num_elems, stdfalse))
      return STD_ERROR(STD_MEM_FAILURE);
  } else { 
    endptr = carr->end;

    if (insert_shift(carr, &endptr, (size_t) delta_size, num_elems, stdtrue))
      return STD_ERROR(STD_MEM_FAILURE);
  }
  return STD_SUCCESS;
}

inline int stdcarr_clear(stdcarr *carr) {
  return stdcarr_resize(carr, 0);
}

inline int stdcarr_set_capacity(stdcarr *carr, size_t num_elems) {
  char *mem;
  stdssize_t diff;
  size_t new_cap = num_elems ? num_elems + 1 : 0;
  /* carray can only fit carr->high_cap - 1 values before reallocation */

  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));

  switch (stdset_allocate(&mem, new_cap, &carr->high_cap, &carr->low_cap, carr->vsize)) {
  case STD_MEM_CHANGE:
    if ((diff = num_elems - carr->size) < 0) {
      carr->end  = backward(carr, carr->end, (size_t) -diff * carr->vsize);
      carr->size = num_elems;
    }
    if (carr->base) {
      copy_to_buf(mem, carr, carr->begin, carr->end);
      free(carr->base);
    }
    carr->base    = mem;
    carr->endbase = mem + carr->high_cap * carr->vsize;
    carr->begin   = mem;
    carr->end     = mem + carr->size * carr->vsize;
    return STD_SUCCESS;

  case STD_NO_MEM_CHANGE:
    return STD_SUCCESS;

  case STD_MEM_FAILURE:
    return STD_ERROR(STD_MEM_FAILURE);
    
  default:
    return STD_EXCEPTION(impossible value from stdset_allocate);
  }
}

inline int stdcarr_reserve(stdcarr *carr, size_t num_elems) {
  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  return (num_elems <= stdcarr_high_capacity(carr) ? STD_SUCCESS : 
	  stdcarr_set_capacity(carr, num_elems << 1));
}

inline int stdcarr_shrink_fit(stdcarr *carr) { 
  return stdcarr_set_capacity(carr, stdcarr_size(carr));
}

/* Stack Operations: amoritized O(1) operations */
inline int stdcarr_push_front(stdcarr *carr, const void *val) {
  return stdcarr_multi_push_front(carr, val, 1);
}

inline int stdcarr_pop_front(stdcarr *carr) {
  return stdcarr_multi_pop_front(carr, 1);
}

inline int stdcarr_push_back(stdcarr *carr, const void *val) {
  return stdcarr_multi_push_back(carr, val, 1);
}

inline int stdcarr_pop_back(stdcarr *carr) {
  return stdcarr_multi_pop_back(carr, 1);
}

inline int stdcarr_multi_push_front(stdcarr *carr, const void *vals, size_t num_push) {
  size_t delta = num_push * carr->vsize, new_size = carr->size + num_push, diff;
  char *begin_ptr = carr->begin;

  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  if (insert_shift(carr, &begin_ptr, delta, new_size, stdfalse))
    return STD_ERROR(STD_MEM_FAILURE);

  if ((diff = (size_t) (carr->endbase - carr->begin)) >= delta)
    memcpy(carr->begin, vals, delta);
  else {
    memcpy(carr->begin, vals, diff);
    memcpy(carr->base, (char*) vals + diff, (size_t) (delta - diff));
  }
  return STD_SUCCESS;
}

inline int stdcarr_multi_pop_front(stdcarr *carr, size_t num_pop) {
  size_t delta = num_pop * carr->vsize, new_size = carr->size - num_pop;
  char *begin_ptr = carr->begin;

  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  STD_BOUNDS_CHECK(num_pop <= stdcarr_size(carr));
  if (erase_shift(carr, &begin_ptr, delta, new_size, stdtrue))
    return STD_ERROR(STD_MEM_FAILURE);

  return STD_SUCCESS;
}

inline int stdcarr_multi_push_back(stdcarr *carr, const void *vals, size_t num_push) {
  size_t delta = num_push * carr->vsize, new_size = carr->size + num_push, diff;
  char *it = carr->end;

  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  if (insert_shift(carr, &it, delta, new_size, stdtrue))
    return STD_ERROR(STD_MEM_FAILURE);

  if ((diff = (size_t) (carr->endbase - it)) >= delta)
    memcpy(it, vals, delta);
  else {
    memcpy(it, vals, diff);
    memcpy(carr->base, (char*) vals + diff, (size_t) (delta - diff));
  }
  return STD_SUCCESS;
}

inline int stdcarr_multi_pop_back(stdcarr *carr, size_t num_pop) {
  size_t delta = num_pop * carr->vsize, new_size = carr->size - num_pop;
  char *end_ptr;

  STD_CONSTRUCT_CHECK(IS_CARR_INITED(carr));
  STD_BOUNDS_CHECK(num_pop <= stdcarr_size(carr));

  end_ptr = backward(carr, carr->end, delta);
  if (erase_shift(carr, &end_ptr, delta, new_size, stdfalse))
    return STD_ERROR(STD_MEM_FAILURE);

  return STD_SUCCESS;
}

/* List Operations: O(n) operations */
inline stdcarr_it *stdcarr_insert(stdcarr_it *it, const void *val) {
  return stdcarr_multi_insert(it, val, 1);
}

inline stdcarr_it *stdcarr_erase(stdcarr_it *it) {
  return stdcarr_multi_erase(it, 1);
}

inline stdcarr_it *stdcarr_repeat_insert(stdcarr_it *it, const void *val, size_t num_times) {
  size_t delta, new_size;
  char *tmp = it->val;     /* tmp is it->val */
  stdbool shift_right;
  stdssize_t diff;

  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));

  delta    = num_times * it->carr->vsize;
  new_size = it->carr->size + num_times;

  /* determine whether it is cheaper to shift_right or to shift_left */
  if ((diff = tmp - it->carr->begin) >= 0) /* it is above carr->begin */
    shift_right = (diff / it->carr->vsize > (stdcarr_size(it->carr) >> 1));
  else 
    shift_right = ((it->carr->end - tmp) / it->carr->vsize <= (stdcarr_size(it->carr) >> 1));

  if (insert_shift(it->carr, &tmp, delta, new_size, (stdbool) shift_right))
    return (stdcarr_it*) STD_ERROR(STD_MEM_FAILURE2);
  
  it->val = tmp;
  for (; num_times--; tmp = forward(it->carr, tmp, it->carr->vsize))
    memcpy(tmp, val, it->carr->vsize);
  
  return it;
}

inline stdcarr_it *stdcarr_multi_insert(stdcarr_it *it, const void *vals, size_t num_insert) {
  size_t delta, new_size;
  char *tmp = it->val;
  int shift_right;
  stdssize_t diff;

  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
  STD_BOUNDS_CHECK(IS_LEGAL_IT(it));

  delta    = num_insert * it->carr->vsize;
  new_size = it->carr->size + num_insert;

  /* determine whether it is cheaper to shift_right or to shift_left */
  if ((diff = tmp - it->carr->begin) >= 0) /* it is above carr->begin */
    shift_right = (diff / it->carr->vsize > stdcarr_size(it->carr) >> 1);
  else 
    shift_right = ((it->carr->end - tmp) / it->carr->vsize <= stdcarr_size(it->carr) >> 1);

  if (insert_shift(it->carr, &tmp, delta, new_size, (stdbool) shift_right))
    return (stdcarr_it*) STD_ERROR(STD_MEM_FAILURE2);

  it->val = tmp;
  if ((diff = it->carr->endbase - tmp) >= delta)
    memcpy(tmp, vals, delta);
  else {
    memcpy(tmp, vals, (size_t) diff);
    memcpy(it->carr->base, (char*) vals + diff, (size_t) (delta - diff));
  }
  return it;
}

inline stdcarr_it *stdcarr_multi_erase(stdcarr_it *it, size_t num_erase) {
  size_t delta, new_size;
  char *tmp = it->val;
  stdbool shift_right;
  stdssize_t diff;

  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_CARR_INITED(it->carr));
#ifdef STD_BOUNDS_CHECKS
  {
    stdcarr_it end;
    
    stdcarr_end(it->carr, &end);
    STD_BOUNDS_CHECK((size_t) stdcarr_it_compare(&end, it) >= num_erase);
  }
#endif

  delta    = num_erase * it->carr->vsize;
  new_size = it->carr->size - num_erase;

  /* determine whether it is cheaper to shift_right or to !shift_right */  
  if ((diff = tmp - it->carr->begin) >= 0) {
    shift_right = (diff / it->carr->vsize < ((stdcarr_size(it->carr) - num_erase) >> 1));
  } else
    /* although this looks wrong, it's right -- make sure you think really hard and work
       it out on paper before changing this */
    shift_right = ((it->carr->end - tmp) / it->carr->vsize >= 
		   ((stdcarr_size(it->carr) + num_erase) >> 1));

  if (erase_shift(it->carr, &tmp, delta, new_size, (stdbool) shift_right))
    return (stdcarr_it*) STD_ERROR(STD_MEM_FAILURE2);

  it->val = tmp;	     
  return it;
}

/* Data Structure Options */
inline stdbool stdcarr_get_auto_alloc(const stdcarr *arr) {
  return arr->auto_alloc;
}

inline void stdcarr_set_auto_alloc(stdcarr *arr, stdbool use_auto_alloc) { 
  arr->auto_alloc = use_auto_alloc;
}
