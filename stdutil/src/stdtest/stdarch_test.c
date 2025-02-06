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

#undef  STD_SAFE_CHECKS
#define STD_SAFE_CHECKS

#include <stdio.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#include <stdutil/stddefines.h>
#include <stdutil/stdutil.h>
#include <stdutil/stderror.h>

int main(void) {
  if (sizeof(char) != 1)
    stderr_quit("Your platform failed sizeof(char) == 1! Currently sizeof(char) = %u\n"
		"This is not ANSI compliant and the stdutil library may not work!\n"
		"To try anyway, open %s and comment out the if statement around line %u\n", 
		sizeof(char), __FILE__, __LINE__);

#ifdef stdint16
  if (sizeof(stdint16) != 2)
    stderr_quit("Your platform failed sizeof(stdint16) == 2! Currently sizeof(stdint16) = %u\n"
		"If you used configure, then configure was wrong, delete config.* and re-run configure.\n"
		"If that doesn't work then manually #define the type sizes (SIZEOF_SHORT, etc.) in stdutil/stddefines.h.\n"
		"If you manually #defined type sizes in stdutil/stddefines.h, then you did it wrong.\n", sizeof(stdint16));
#endif

#ifdef stdint32
  if (sizeof(stdint32) != 4)
    stderr_quit("Your platform failed sizeof(stdint32) == 4! Currently sizeof(stdint32) = %u\n"
		"If you used configure, then configure was wrong, delete config.* and re-run configure.\n"
		"If that doesn't work then manually #define the type sizes (SIZEOF_SHORT, etc.) in stdutil/stddefines.h.\n"
		"If you manually #defined type sizes in stdutil/stddefines.h, then you did it wrong.\n", sizeof(stdint32));
#endif

#ifdef stdint64
  if (sizeof(stdint64) != 8)
    stderr_quit("Your platform failed sizeof(stdint64) == 8! Currently sizeof(stdint64) = %u\n"
		"If you used configure, then configure was wrong, delete config.* and re-run configure.\n"
		"If that doesn't work then manually #define the type sizes (SIZEOF_SHORT, etc.) in stdutil/stddefines.h.\n"
		"If you manually #defined type sizes in stdutil/stddefines.h, then you did it wrong.\n", sizeof(stdint64));
#endif

#ifdef stdhsize_t
  if (sizeof(size_t) != sizeof(stdhsize_t) * 2)
    stderr_quit("Your platform failed sizeof(size_t) == 2 * sizeof(stdhsize_t)!\n"
		"Currently sizeof(size_t) = %u, and sizeof(stdhsize_t) = %u\n"
		"If you used configure, then configure was wrong, delete config.* and re-run configure.\n"
		"If that doesn't work then manually #define the size_t type (SIZEOF_SIZE_T) in stdutil/stddefines.h.\n"
		"If you manually #defined type sizes in stdutil/stddefines.h, then you did it wrong.\n", 
		sizeof(size_t), sizeof(stdhsize_t));
#endif


#if !defined(STDRAND_EXISTS)
  printf("The stdrand fcns are disabled!\n");
  printf("The stdhash data structure is disabled!\n");
#endif

#if !defined(STDRAND32_EXISTS)
  printf("The stdrand32 fcns are disabled!\n");
#endif

#if !defined(STDRAND64_EXISTS)
  printf("The stdrand64 fcns are disabled!\n");
#endif

  printf("\nThe stdutil library will function on your platform.\n\n");

  return 0;
}
