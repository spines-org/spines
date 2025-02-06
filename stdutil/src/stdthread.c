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

#include <errno.h>
#include <stdutil/stderror.h>
#include <stdutil/stdthread.h>

#if !defined(_REENTRANT)

inline int stdmutex_construct(stdmutex *mut) { return 0; }
inline int stdmutex_destruct(stdmutex *mut) { return 0; }
inline int stdmutex_lock(stdmutex *mut) { return 0; }
inline int stdmutex_trylock(stdmutex *mut) { return 0; }
inline int stdmutex_unlock(stdmutex *mut) { return 0; }

inline int stdcond_construct(stdcond *cond) { return 0; }
inline int stdcond_destruct(stdcond *cond) { return 0; }
inline int stdcond_wait(stdcond *cond, stdmutex *mut) { return 0; }
inline int stdcond_timedwait(stdcond *cond, stdmutex *mut, long ms) { return 0; }
inline int stdcond_signal(stdcond *cond) { return 0; }
inline int stdcond_broadcast(stdcond *cond) { return 0; }

#elif defined(_WIN32)

inline int stdmutex_construct(stdmutex *mut) { 
  if (!InitializeCriticalSection(mut))
    return STD_ERROR(STDMUTEX_FAIL);

  return 0;
}

inline int stdmutex_destruct(stdmutex *mut) {
  return DeleteCriticalSection(mut), 0;
}

inline int stdmutex_lock(stdmutex *mut) {
  return EnterCriticalSection(mut), 0;
}

inline int stdmutex_trylock(stdmutex *mut) {
  if (!TryEnterCriticalSection(mut))
    return STDMUTEX_BUSY;

  return 0;
}

inline int stdmutex_unlock(stdmutex *mut) {
  return LeaveCriticalSection(mut), 0;
}

inline int stdcond_construct(stdcond *cond) {
  if (!(cond->events[0] = CreateEvent(NULL, FALSE, FALSE, NULL)))
    return STD_ERROR(STDCOND_FAIL);

  if (!(cond->events[1] = CreateEvent(NULL, TRUE, FALSE, NULL)))
    return CloseHandle(cond->events[0]), STD_ERROR(STDCOND_FAIL);

  return 0;
}

inline int stdcond_destruct(stdcond *cond) {
  if (!(CloseHandle(cond->events[0]) & CloseHandle(cond->events[1])))
    return STD_ERROR(STDCOND_FAIL);

  return 0;
}

inline int stdcond_wait(stdcond *cond, stdmutex *mut) {
  DWORD ret;

  if (stdmutex_unlock(mut))
    return STD_ERROR(STDMUTEX_FAIL);
  
  if ((ret = WaitForMultipleObjects(2, cond->events, FALSE, INFINITE)) == WAIT_FAILED)
    return STD_ERROR(STDCOND_FAIL);

  if (stdmutex_lock(mut))
    return STDERROR(STDMUTEX_FAIL);

  return 0;
}

inline int stdcond_timedwait(stdcond *cond, stdmutex *mut, size_t ns) {
  DWORD ret;

  if (stdmutex_unlock(mut))
    return STD_ERROR(STDMUTEX_FAIL);

  if ((ret = WaitForMultipleObjects(2, cond->events, FALSE, ns / 10)) == WAIT_FAILED)
    return STD_ERROR(STDCOND_FAIL);

  if (stdmutex_lock(mut))
    return STDERROR(STDMUTEX_FAIL);

  return ret != WAIT_TIMEOUT ? 0 : STDCOND_TIMEOUT;
}

inline int stdcond_signal(stdcond *cond) {
  if (!PulseEvent(cond->events[0]))
    return STD_ERROR(STDCOND_FAIL);
  
  return 0;
}

inline int stdcond_broadcast(stdcond *cond) {
  if (!PulseEvent(cond->events[1]))
    return STD_ERROR(STDCOND_FAIL);

  return 0;
}

#else

inline int stdmutex_construct(stdmutex *mut) {
  if (pthread_mutex_init(mut, 0))
    return STD_ERROR(STDMUTEX_FAIL);

  return 0;
}

inline int stdmutex_destruct(stdmutex *mut) {
  if (pthread_mutex_destroy(mut))
    return STD_ERROR(STDMUTEX_FAIL);

  return 0;
}

inline int stdmutex_lock(stdmutex *mut) {
  if (pthread_mutex_lock(mut))
    return STD_ERROR(STDMUTEX_FAIL);
  
  return 0;
}

inline int stdmutex_trylock(stdmutex *mut) {
  int err;

  if ((err = pthread_mutex_trylock(mut))) {
    if (err == EBUSY) 
      return STDMUTEX_BUSY;
    else
      return STD_ERROR(STDMUTEX_FAIL);
  }   
  return 0;
}

inline int stdmutex_unlock(stdmutex *mut) {
  if (pthread_mutex_unlock(mut))
    return STD_ERROR(STDMUTEX_FAIL);

  return 0;
}

inline int stdcond_construct(stdcond *cond) {
  if (pthread_cond_init(cond, 0))
    return STD_ERROR(STDCOND_FAIL);

  return 0;
}

inline int stdcond_destruct(stdcond *cond) {
  if (pthread_cond_destroy(cond))
    return STD_ERROR(STDCOND_FAIL);

  return 0;
}

inline int stdcond_wait(stdcond *cond, stdmutex *mut) {
  if (pthread_cond_wait(cond, mut))
    return STD_ERROR(STDCOND_FAIL);

  return 0;
}

inline int stdcond_timedwait(stdcond *cond, stdmutex *mut, long ns) {
  struct timeval now;
  struct timespec timeout;
  ldiv_t div;
  int err;

  gettimeofday(&now, 0);
  div = ldiv(ns + now.tv_usec * 1000, 1000000000); /* 1e9 */

  timeout.tv_sec  = now.tv_sec + div.quot;
  timeout.tv_nsec = div.rem;

  if ((err = pthread_cond_timedwait(cond, mut, &timeout))) {
    if (err == ETIMEDOUT)
      return STDCOND_TIMEOUT;
    else
      return STD_ERROR(STDCOND_FAIL);
  }
  return 0;
}

inline int stdcond_signal(stdcond *cond) {
  if (pthread_cond_signal(cond))
    return STD_ERROR(STDCOND_FAIL);

  return 0;
}

inline int stdcond_broadcast(stdcond *cond) {
  if (pthread_cond_broadcast(cond))
    return STD_ERROR(STDCOND_FAIL);

  return 0;
}

#endif

