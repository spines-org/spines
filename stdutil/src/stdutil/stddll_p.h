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

/* this file should only be included from stddll.h */

#ifdef stddll_h_2000_02_14_16_22_38_jschultz_at_cnds_jhu_edu 
# ifndef stddll_p_h_2000_05_16_18_07_15_jschultz_at_cnds_jhu_edu
# define stddll_p_h_2000_05_16_18_07_15_jschultz_at_cnds_jhu_edu

# define STDDLL_INITED    ((int) 0xAB12DC43L)
# define STDDLL_IT_INITED ((int) 0x8725AD8FL)

/* stddll_node: A type that points to a value and to the next and previous nodes in the list.

   prev - pointer to previous stddll_node on list
   next - pointer to next stddl_node on list
   val  - pointer to the value this node is containing
*/
typedef struct stddll_node {
  struct stddll_node *prev, *next;
  void *val;
} stddll_node;

/* stddll: A circular linked list of values contained by value.

   end_node - end_node represents the end of the list
     end_node.next - points to first node of list, or end_node if no values
     end_node.prev - points to last node of list, or end_node if no values
   vsize    - size, in bytes, of the type of values this list is storing 
   size     - number of values this list is storing
   init_val - magic number that indicates if this list is initialized 
*/
typedef struct {
  stddll_node end_node;
  size_t      vsize, size;
  int         init_val;
} stddll;

/* stddll_it: An iterator for a stddll.

   list        - address of the stddll this iterator is referencing into
   node        - address of the node pointing at the value this iterator is referencing
   it_init_val - magic number that indicates if this iterator is initialized 
*/
typedef struct {
  stddll      *list;
  stddll_node *node;
  int         it_init_val;
} stddll_it;

# endif
#else
COMPILE-ERROR: __FILE__, line __LINE__: stddll_p.h should only be included from within stddll.h!
#endif
