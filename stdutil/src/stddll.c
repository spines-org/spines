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
#include <stdutil/stddll.h>

#ifdef STD_CONSTRUCT_CHECKS
# define IS_DLL_INITED(list) ((list)->init_val == STDDLL_INITED)
# define INIT_DLL(list)      ((list)->init_val = STDDLL_INITED)
# define UNINIT_DLL(list)    ((list)->init_val = ~STDDLL_INITED)
# define IS_IT_INITED(it)    ((it)->it_init_val == STDDLL_IT_INITED)
# define INIT_IT(it)         ((it)->it_init_val = STDDLL_IT_INITED)
#else
# define IS_DLL_INITED(list)
# define INIT_DLL(list)
# define UNINIT_DLL(list)
# define IS_IT_INITED(it)
# define INIT_IT(it)
#endif

/* pointer to value appended to a list node */
#define NVAL(node_ptr)   ((node_ptr)->val)

/* pointer to the beginning node of a list */
#define LBEGIN(list_ptr) ((list_ptr)->end_node.next)

/* pointer to the end node of a list */
#define LEND(list_ptr)   (&(list_ptr)->end_node)

/* This fcn allocates a sequence of linked nodes of _non-zero_ length
   len. It returns pointers to the first and last nodes. The sequence
   is null terminated on both ends. On success it returns STD_SUCCESS.
*/
inline static int alloc_seq(stddll *l, size_t request_len,
			    stddll_node **first, stddll_node **last) {
  stddll_node *prev, *curr;

  STD_SAFE_CHECK(request_len != 0);

  /* append values on to end of node in memory */
  if (!(curr = (stddll_node*) malloc(sizeof(stddll_node))))
    return STD_MEM_FAILURE;

  if (!(curr->val = malloc(l->vsize))) {
    free(curr);
    return STD_MEM_FAILURE;
  }

  *first = curr;
  curr->prev = 0;

  while (--request_len) {
    prev = curr;
    if (!(curr = (stddll_node*) malloc(sizeof(stddll_node))))
      goto alloc_seq_fail;

    if (!(curr->val = malloc(l->vsize))) {
      free(curr);
      goto alloc_seq_fail;
    }
    prev->next = curr;
    curr->prev = prev;
  }

  curr->next = 0;
  *last = curr;

  return STD_SUCCESS;

 alloc_seq_fail:
  while (prev) {
    curr = prev->prev;
    free(prev->val);
    free(prev);
    prev = curr;
  }
  return STD_MEM_FAILURE;
}

/* This fcn inserts a _non-empty_ sequence of linked nodes into a list before next. */
inline static void linsert(stddll *l, stddll_node *next, size_t num_insert,
			   stddll_node *first, stddll_node *last) {
  stddll_node *prev = next->prev;

  first->prev = prev;
  last->next  = next;
  prev->next  = first;
  next->prev  = last;

  l->size += num_insert;
}

/* This fcn allocates and inserts a sequence of linked nodes into a
   list before next. It returns a pointer to the first node of the
   inserted sequence, next if the sequence is empty, and 0 on failure.  
*/
inline static stddll_node *insert_space(stddll *l, stddll_node *next, size_t num_insert) {
  stddll_node *first, *last;

  if (!num_insert)
    return next;

  if (alloc_seq(l, num_insert, &first, &last) != STD_SUCCESS)
    return 0;           /* failure */

  linsert(l, next, num_insert, first, last);

  return first;
}

/* Erase a subsequence of the list starting at erase_begin of length
   num_erase. It returns a pointer to the node that is shifted into 
   the position of erase_begin after the removal.
*/
inline static stddll_node *erase(stddll *l, stddll_node *erase_begin, size_t num_erase) {
  stddll_node *prev, *curr = erase_begin;

  erase_begin = erase_begin->prev;  /* get last node before erase region */
  l->size -= num_erase;
  while (num_erase--) {
    STD_BOUNDS_CHECK(curr != LEND(l));  /* don't ever free end */
    prev = curr;
    curr = curr->next;
    free(prev->val);
    free(prev);
  }
  erase_begin->next = curr;
  curr->prev        = erase_begin;

  return curr;
}

/* Same as above, except the erase region ends just before erase_end
   and is of length num_erase.
*/
inline static stddll_node *rerase(stddll *l, stddll_node *erase_end, size_t num_erase) {
  stddll_node *prev, *curr = erase_end->prev;

  l->size -= num_erase;
  while (num_erase--) {
    STD_BOUNDS_CHECK(curr != LEND(l)); /* don't allow a wrap around to delete end */
    prev = curr;
    curr = curr->prev;
    free(prev->val);
    free(prev);
  }
  curr->next      = erase_end;
  erase_end->prev = curr;

  return erase_end;
}

inline static size_t sizeof_val(const stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  return it->list->vsize;
}

/**************************** stddll_it interface *********************************/

inline void *stddll_it_val(const stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  return NVAL(it->node);
}

inline stdbool stddll_it_equals(const stddll_it *it1, const stddll_it *it2) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it1) && IS_IT_INITED(it2));
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(it1->list) && IS_DLL_INITED(it2->list));
  STD_BOUNDS_CHECK(it1->list == it2->list);
  return it1->node == it2->node;
}

inline stdbool stddll_it_is_begin(const stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  return it->node == LBEGIN(it->list);
}

inline stdbool stddll_it_is_end(const stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  return it->node == LEND(it->list);
}

inline stddll_it *stddll_it_seek_begin(stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  it->node = LBEGIN(it->list);
  return it;
}

inline stddll_it *stddll_it_seek_end(stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  it->node = LEND(it->list);
  return it;
}

inline stddll_it *stddll_it_next(stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  STD_BOUNDS_CHECK(it->node != LEND(it->list));
  it->node = it->node->next;
  return it;
}

inline stddll_it *stddll_it_advance(stddll_it *it, size_t num_advance) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  while (num_advance--) {
    STD_BOUNDS_CHECK(it->node != LEND(it->list));
    it->node = it->node->next;
  }
  return it;
}

inline stddll_it *stddll_it_prev(stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  STD_BOUNDS_CHECK(it->node != LBEGIN(it->list));
  it->node = it->node->prev;
  return it;
}

inline stddll_it *stddll_it_retreat(stddll_it *it, size_t num_retreat) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  while (num_retreat--) {
    STD_BOUNDS_CHECK(it->node != LBEGIN(it->list));
    it->node = it->node->prev;
  }
  return it;
}

/***************************** stddll iterface ************************************/
/* Constructors, Destructor */
inline int stddll_construct(stddll *l, size_t sizeof_val) {
  STD_CONSTRUCT_CHECK(!IS_DLL_INITED(l));

  if (!(l->vsize = sizeof_val))
    return STD_ERROR(STD_ILLEGAL_PARAM);

  l->size          = 0;
  l->end_node.prev = &l->end_node;
  l->end_node.next = &l->end_node;
  l->end_node.val  = 0;

  INIT_DLL(l);

  return STD_SUCCESS;
}

inline int stddll_copy_construct(stddll *dst, const stddll *src) {
  stddll_node *node1, *node2;
  int ret;

  STD_CONSTRUCT_CHECK(IS_DLL_INITED(src));

  if ((ret = stddll_construct(dst, src->vsize)) != STD_SUCCESS)
    return STD_ERROR(ret);

  if (!(node1 = insert_space(dst, LEND(dst), src->size)))
    return STD_ERROR(STD_MEM_FAILURE);

  node2 = LBEGIN(src);
  for (; node1 != LEND(dst); node1 = node1->next, node2 = node2->next)
    memcpy(NVAL(node1), NVAL(node2), dst->vsize);

   return STD_SUCCESS;
}

inline void stddll_destruct(stddll *l) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  stddll_clear(l);
  UNINIT_DLL(l);
}

/* Iterator Interface */
inline stddll_it *stddll_begin(const stddll *l, stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  it->list = (stddll*) l;
  it->node = (stddll_node*) LBEGIN(l);
  INIT_IT(it);
  return it;
}

inline stddll_it *stddll_last(const stddll *l, stddll_it *it) {
  return stddll_it_prev(stddll_end(l, it));
}

inline stddll_it *stddll_end(const stddll *l, stddll_it *it) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  it->list = (stddll*) l;
  it->node = (stddll_node*) LEND(l);
  INIT_IT(it);
  return it;
}

inline stddll_it *stddll_get(const stddll *l, stddll_it *it, size_t elem_num) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  STD_BOUNDS_CHECK(elem_num <= stddll_size(l));
  if (elem_num < stddll_size(l) >> 1)
    return stddll_it_advance(stddll_begin(l, it), elem_num);
  else
    return stddll_it_retreat(stddll_end(l, it), stddll_size(l) - elem_num);
}

/* Size Information */
inline size_t stddll_size(const stddll *l) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  return l->size;
}

inline stdbool stddll_empty(const stddll *l) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  return l->size == 0;
}

inline size_t stddll_max_size(const stddll *l) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  return STD_SIZE_T_MAX;
}

inline size_t stddll_val_size(const stddll *l) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  return l->vsize;
}

/* Size Operations */
inline int stddll_resize(stddll *l, size_t num_elems) {
  stdssize_t diff;

  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));

  if ((diff = num_elems - l->size) > 0) {
    if (!insert_space(l, LEND(l), (size_t) diff))
      return STD_ERROR(STD_MEM_FAILURE);
  } else if (diff < 0)
    rerase(l, LEND(l), (size_t) -diff);
 
  return STD_SUCCESS;
}

inline int stddll_clear(stddll *l) {
  return stddll_resize(l, 0);
}

/* Stack Operations: O(1) operations */
inline int stddll_push_front(stddll *l, const void *val) {
  return stddll_multi_push_front(l, val, 1);
}

inline int stddll_pop_front(stddll *l) {
  return stddll_multi_pop_front(l, 1);
}

inline int stddll_push_back(stddll *l, const void *val) {
  return stddll_multi_push_back(l, val, 1);
}

inline int stddll_pop_back(stddll *l) {
  return stddll_multi_pop_back(l, 1);
}

inline int stddll_multi_push_front(stddll *l, const void *vals, size_t num_push) {
  stddll_it it;

  if (stddll_multi_insert(stddll_begin(l, &it), vals, num_push))
    return STD_SUCCESS;
  else
    return STD_ERROR(STD_MEM_FAILURE);
}

inline int stddll_multi_pop_front(stddll *l, size_t num_pop) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  STD_BOUNDS_CHECK(num_pop <= stddll_size(l));
  erase(l, LBEGIN(l), num_pop);
  return STD_SUCCESS;
}

inline int stddll_multi_push_back(stddll *l, const void *vals, size_t num_push) {
  stddll_it it;

  if (stddll_multi_insert(stddll_end(l, &it), vals, num_push))
    return STD_SUCCESS;
  else
    return STD_ERROR(STD_MEM_FAILURE);
}

inline int stddll_multi_pop_back(stddll *l, size_t num_pop) {
  STD_CONSTRUCT_CHECK(IS_DLL_INITED(l));
  STD_BOUNDS_CHECK(num_pop <= stddll_size(l));
  rerase(l, LEND(l), num_pop);
  return STD_SUCCESS;
}

/* List Operations: O(1) operations */
inline stddll_it *stddll_insert(stddll_it *it, const void *val) {
  return stddll_multi_insert(it, val, 1);
}

inline stddll_it *stddll_erase(stddll_it *it) {
  return stddll_multi_erase(it, 1);
}

inline stddll_it *stddll_repeat_insert(stddll_it *it, const void *val, size_t num_times) {
  stddll_node *first;

  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));

  if (!(first = insert_space(it->list, it->node, num_times)))
    return (stddll_it*) STD_ERROR(STD_MEM_FAILURE2);

  it->node = first;
  for (; num_times--; first = first->next)
    memcpy(NVAL(first), val, it->list->vsize);

  return it;
}

inline stddll_it *stddll_multi_insert(stddll_it *it, const void *vals, size_t num_insert) {
  stddll_node *first;
  char *vals_ptr;

  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));

  if (!(first = insert_space(it->list, it->node, num_insert)))
    return (stddll_it*) STD_ERROR(STD_MEM_FAILURE2);

  it->node = first;
  for (vals_ptr = (char*) vals; num_insert--; first = first->next, vals_ptr += it->list->vsize)
    memcpy(NVAL(first), vals_ptr, it->list->vsize);

  return it;
}

inline stddll_it *stddll_multi_erase(stddll_it *it, size_t num_erase) {
  STD_CONSTRUCT_CHECK(IS_IT_INITED(it) && IS_DLL_INITED(it->list));
  it->node = erase(it->list, it->node, num_erase);
  return it;
}
