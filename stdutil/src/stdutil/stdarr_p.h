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

/* this file should only be included from stdarr.h */

#ifdef stdarr_h_2000_01_26_11_38_04_jschultz_at_cnds_jhu_edu
# ifndef stdarr_p_h_2000_05_13_19_35_29_jschultz_at_cnds_jhu_edu
# define stdarr_p_h_2000_05_13_19_35_29_jschultz_at_cnds_jhu_edu

# define STDARR_INITED    ((int) 0xF1E2D3C4L)
# define STDARR_IT_INITED ((int) 0xAC920327L)

# undef STDARR_STATIC_CONSTRUCT
# define STDARR_STATIC_CONSTRUCT(vsize) { 0, 0, (vsize), 0, 0, 0, stdfalse, STD_ARR_INITED }

/* stdarr: An array-based sequence: a growable array or vector. 
           Stores elements by value, contiguously in memory starting at the base address.
  
   begin      - address of the lowest byte of the array's alloc'ed memory, null(0) if none   
   end        - address of the first byte past the last value stored in the array, same as begin if none
   vsize      - size, in bytes, of the type of values this array is storing   
   size       - number of values this array is storing   
   high_cap   - number of values that can legally fit in alloc'ed memory
   low_cap    - number of values below which the array should be shrunk in alloc'ed memory   
   auto_alloc - user set parameter controlling whether the array is automatically 
                reallocated when size exceeds high_cap and when size gets down to low_cap   
   init_val   - magic number that indicates if this array is initialized 
*/

typedef struct {
  char    *begin, *end;
  size_t  vsize, size, high_cap, low_cap;
  stdbool auto_alloc;
  int     init_val;
} stdarr;

/* stdarr_it: An iterator for a stdarr.

   arr         - address of the stdarr this iterator is referencing into
   val         - address of the value in arr this iterator is referencing
   it_init_val - magic number that indicates if this iterator is initialized
*/

typedef struct {
  stdarr *arr;
  char   *val;
  int    it_init_val;
} stdarr_it;

# endif
#else
COMPILE-ERROR: __FILE__, line __LINE__: stdarr_p.h should only be included from within stdarr.h!
#endif
