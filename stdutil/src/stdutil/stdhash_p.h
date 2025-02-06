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

/* this file should only be included from within stdhash.h */

#ifdef stdhash_h_2000_02_14_16_22_38_jschultz_at_cnds_jhu_edu
# ifndef stdhash_p_h_2000_05_17_18_02_31_jschultz_at_cnds_jhu_edu
# define stdhash_p_h_2000_05_17_18_02_31_jschultz_at_cnds_jhu_edu

# include <stdutil/stddefines.h>
# include <stdutil/stdkvp.h>

# define STDHASH_INITED    ((int) 0x84C0772DL)
# define STDHASH_IT_INITED ((int) 0x0227A8C7L)

# undef STDHASH_STATIC_CONSTRUCT
# undef STDHASH_STATIC_CONSTRUCT2

/* static initializer with no safety checks */
# define STDHASH_STATIC_CONSTRUCT2(ksize, vsize, eq_fcn, hcode_fcn, seed) \
{ 0, 0, 0, ksize, vsize, 0, (size_t) -1, 0, 0, 0, (seed) * 33, ~(seed) * 127, \
{ (stdhsize_t) ((seed) ^ 0x41AE8120UL), (stdhsize_t) ((seed) ^ 0x087DAE31UL), (stdhsize_t) ((seed) ^ 0x027DAE31UL) }, \
{ (stdhsize_t) ((seed) ^ 0x7231DE10UL), (stdhsize_t) ((seed) ^ 0x57EFF218UL), (stdhsize_t) ((seed) ^ 0x34CB06AFUL) }, \
eq_fcn, hcode_fcn, STDHASH_INITED }

/* this macro simply calls the other construct macro with a particular fixed seed */
# define STDHASH_STATIC_CONSTRUCT(ksize, vsize, eq_fcn, hcode_fcn) \
STDHASH_STATIC_CONSTRUCT2(ksize, vsize, eq_fcn, hcode_fcn, 0x9F63B655UL)

/* stdhash_node: A type that contains the stdkvp and caches the key's expanded hashcode.

   exphcode - a cached copy of the expanded hashcode of the key for this key-val pair
   kv       - pointers to the contained key and value (see stdkvp.h)
*/
typedef struct {
  size_t exphcode;
  stdkvp kv;
} stdhash_node;

/* stdhash: An array based dictionary that maps keys to values.

   table       - pointer to the base address of an alloc'ed array of node*'s, null(0) if none
   table_end   - pointer to one past the address of the alloc'ed array of node*'s, null(0) if none
   begin       - pointer to the first node* in the array that is not null, table_end if none
   ksize       - size, in bytes, of the key type
   vsize       - size, in bytes, of the value type
   size        - number of active key-val pairs the hash currently contains
   cap_min1    - number (power of 2) of node*'s that could fit in table minus 1 (bitmask for modulo)
   high_thresh - if num_nodes exceeds this number then table must be rehashed (possibly realloc'ed too)
   low_thresh  - if size drops to this number then table should be shrunk and rehashed
   num_nodes   - the number of alloc'ed nodes in table (a loading factor on the table)
   a           - random multiplicand used in computing hash values
   b           - random addend used in computing hash values
   ai          - seed data used for generating random a's
   bi          - seed data used for generating random b's
   equals      - user passed fcn for comparing key types through ptrs to key types
   hashcode    - user passed fcn for generating hashcodes from a ptr to a key type
   init_val    - magic number indicating if stdhash is initialized yet or not
*/
typedef struct {
  stdhash_node **table, **table_end, **begin;

  size_t ksize;
  size_t vsize;
  size_t size;
  size_t cap_min1;
  size_t high_thresh;
  size_t low_thresh;
  size_t num_nodes;

  size_t a, b;          
  stdhsize_t ai[3];     
  stdhsize_t bi[3];     

  stdequals_fcn equals;
  stdhcode_fcn  hashcode;

  int init_val;
} stdhash;

/* stdhash_it: An iterator for a stdhash.

   hash        - address of the stdhash this iterator is referencing into
   node_pos    - address of the position in the array of node*'s this iterator is currently referencing
   it_init_val - magic number indicating if stdhash_it is initialized yet or not
*/
typedef struct {
  stdhash      *hash;
  stdhash_node **node_pos;
  int          it_init_val;
} stdhash_it;

# endif
#else
COMPILE-ERROR: __FILE__, line __LINE__: stdhash_p.h should only be included from within stdhash.h!
#endif
