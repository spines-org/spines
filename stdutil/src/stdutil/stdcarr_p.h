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

/* this file should only be included from stdcarr.h */

#ifdef stdcarr_h_2000_01_30_23_42_05_jschultz_at_cnds_jhu_edu
# ifndef stdcarr_p_h_2000_05_15_18_35_08_jschultz_at_cnds_jhu_edu
# define stdcarr_p_h_2000_05_15_18_35_08_jschultz_at_cnds_jhu_edu

# define STDCARR_INITED    ((int) 0x1234FEDCL)
# define STDCARR_IT_INITED ((int) 0xAF429771L)

# undef STDCARR_STATIC_CONSTRUCT
# define STDCARR_STATIC_CONSTRUCT(vsize) { 0, 0, 0, 0, (vsize), 0, 0, 0, stdtrue, STDCARR_INITED }

/* stdcarr: A circular array-based sequence: a growable circular array or vector. 
            Stores element by value, contiguously in memory modulo the size of the array.
	    Always has at least one empty element in the array.

   base       - address of the lowest byte of the array's alloc'ed memory, null(0) if none
   endbase    - address of the first byte past the end of the array's alloc'ed memory, null(0) if none
   begin      - address of the first byte of the first (begin) value stored in the array, arbitrary if none
   end        - address of the first byte past the last value stored in the array (end), begin if none 
   vsize      - size, in bytes, of the type of values this array is storing   
   size       - number of values this array is storing   
   high_cap   - number of values that could legally fit in alloc'ed memory: is one more than the #
                of elements that will be allowed in the array before reallocating to a larger array 
   low_cap    - number of values below which the array should be shrunk in alloc'ed memory   
   auto_alloc - user set parameter controlling whether the array is automatically 
                reallocated when size exceeds high_cap and when size gets down to low_cap   
   init_val   - magic number that indicates if this array is initialized 
*/

typedef struct {
  char    *base, *endbase;
  char    *begin, *end;
  size_t  vsize, size, high_cap, low_cap;
  stdbool auto_alloc;
  int     init_val;
} stdcarr;

/* stdcarr_it: Iterator for stdcarrs.

   carr        - address of the stdcarr this iterator is referencing into
   val         - address of the value in carr this iterator is referencing
   it_init_val - magic number that indicates if this iterator is initialized
*/

typedef struct {
  stdcarr *carr;
  char    *val;
  int     it_init_val;
} stdcarr_it;

# endif
#else
COMPILE-ERROR: __FILE__, line __LINE__: stdcarr_p.h should only be included from within stdcarr.h!
#endif
