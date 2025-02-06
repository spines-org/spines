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
#include <stdio.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdutil/stddefines.h>
#include <stdutil/stderror.h>

/* Print a message and return to caller. If errnoflag != 0, print error msg from errno. */
inline static void stderr_doit(int errno_copy, const char *fmt, va_list ap) {
  int ret1, ret2;
  char buf[STDERR_MAX_ERR_MSG_LEN + 1];

  ret1 = vsnprintf(buf, sizeof(buf), fmt, ap);

  if (ret1 < 1 || ret1 > sizeof(buf)) /* couldn't fit in buffer */
    buf[STDERR_MAX_ERR_MSG_LEN] = 0;  /* ensure null termination */

  if (errno_copy != 0) {
    --ret1;                           /* overwrite first null termination */

    ret2 = vsnprintf(buf + ret1, sizeof(buf) - ret1, ": %s", strerror(errno_copy));

    if (ret2 < 1 || ret2 > sizeof(buf) - ret1) /* couldn't fit in buffer */
      buf[STDERR_MAX_ERR_MSG_LEN] = 0;         /* ensure null termination */
  }
  fprintf(stderr, "%s\n", buf);
  fflush(stderr);
}

/* Nonfatal error unrelated to a system call. Print a message and return. */
int stderr_msg(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  stderr_doit(0, fmt, ap);
  va_end(ap);

  return 0;
}

/* Nonfatal error related to a system call. Print a message and return. */
int stderr_ret(const char *fmt, ...) {
  int errno_copy = errno;
  va_list ap;

  va_start(ap, fmt);
  stderr_doit(errno_copy, fmt, ap);
  va_end(ap);

  return 0;
}

/* Fatal error unrelated to a system call. Print a message and terminate. */
int stderr_quit(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  stderr_doit(0, fmt, ap);
  va_end(ap);
  exit(1);

  return 0;
}

/* Fatal error unrelated to a system call. Print a message and abort. */
int stderr_abort(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  stderr_doit(0, fmt, ap);
  va_end(ap);
  abort();

  return 0;
}

int stderr_pabort(const char *file_name, unsigned line_num, const char *fmt, ...) {
  char fmt2[STDERR_MAX_ERR_MSG_LEN + 1];    /* buffer for format string to stderr_doit */
  int errno_copy = errno, ret;
  va_list ap;

  ret = snprintf(fmt2, sizeof(fmt2), "pabort: File: %s, Line: %u, %s", file_name, line_num, fmt);

  if (ret < 1 || ret > sizeof(fmt2)) /* didn't fit in buffer */
    fmt2[STDERR_MAX_ERR_MSG_LEN] = 0;       /* ensure null termination */

  va_start(ap, fmt);
  stderr_doit(errno_copy, fmt2, ap);
  va_end(ap);
  abort();

  return 0;
}

/* Fatal error related to a system call. Print a message and terminate. */
int stderr_sys(const char *fmt, ...) {
  int errno_copy = errno;
  va_list ap;

  va_start(ap, fmt);
  stderr_doit(errno_copy, fmt, ap);
  va_end(ap);
  exit(1);

  return 0;
}

/* Fatal error related to a system call. Print a message and abort. */
int stderr_dump(const char *fmt, ...) {
  int errno_copy = errno;
  va_list ap;

  va_start(ap, fmt);
  stderr_doit(errno_copy, fmt, ap);
  va_end(ap);
  abort();

  return 0;
}
