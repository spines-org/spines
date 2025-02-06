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

#ifndef stdthread_h_2000_03_14_12_28_17_jschultz_at_cnds_jhu_edu
#define stdthread_h_2000_03_14_12_28_17_jschultz_at_cnds_jhu_edu

/* stdmutex static initializer */
#define STDMUTEX_STATIC_CONSTRUCT

#include <stdutil/stddefines.h>
#include <stdutil/stdthread_p.h>

/* stdmutex: work similiar to pthread mutexes, but cannot be copied or moved */
#define STDMUTEX_FAIL    -1
#define STDMUTEX_BUSY    -2

inline int stdmutex_construct(stdmutex *mut);
inline int stdmutex_destruct(stdmutex *mut);

inline int stdmutex_lock(stdmutex *mut);
inline int stdmutex_trylock(stdmutex *mut);
inline int stdmutex_unlock(stdmutex *mut);

/* stdcond: work similiar to pthread conditions, but no static initializer */
#define STDCOND_TIMEOUT   1
#define STDCOND_FAIL    -10
#define STDCOND_BUSY    -11

inline int stdcond_construct(stdcond *cond);
inline int stdcond_destruct(stdcond *cond);

inline int stdcond_wait(stdcond *cond, stdmutex *mut);
inline int stdcond_timedwait(stdcond *cond, stdmutex *mut, long ns);

inline int stdcond_signal(stdcond *cond);
inline int stdcond_broadcast(stdcond *cond);

#endif
