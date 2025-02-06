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

/* this file should only be included from stdthread.h */

#ifdef stdthread_h_2000_03_14_12_28_17_jschultz_at_cnds_jhu_edu
# ifndef stdthread_p_h_2000_05_18_12_57_48_jschultz_at_cnds_jhu_edu
# define stdthread_p_h_2000_05_18_12_57_48_jschultz_at_cnds_jhu_edu

# undef STDMUTEX_STATIC_CONSTRUCT
# if !defined(_REENTRANT)

#  define STDMUTEX_STATIC_CONSTRUCT 0
typedef int stdmutex;
typedef int stdcond;

# elif defined(_WIN32)
#  include <winbase.h>
#  define STDMUTEX_STATIC_CONSTRUCT 0

typedef CRITICAL_SECTION stdmutex;

typdef struct {
  HANDLE events[2];
} stdcond;

# else
#  include <pthread.h>
#  include <time.h>
#  include <sys/time.h>
#  define STDMUTEX_STATIC_CONSTRUCT PTHREAD_MUTEX_INITIALIZER

typedef pthread_mutex_t stdmutex;
typedef pthread_cond_t stdcond;

# endif
# endif
#else
COMPILE-ERROR: __FILE__, line __LINE__: stdthread_p.h should only be included from stdthread.h!
#endif
