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

#ifndef stderror_h_2000_05_15_14_04_16_jschultz_at_cnds_jhu_edu
#define stderror_h_2000_05_15_14_04_16_jschultz_at_cnds_jhu_edu

/* stderr error routines */

/* change this and recompile if 1024 characters isn't long enough for your error msgs */
#define STDERR_MAX_ERR_MSG_LEN 1024

int stderr_msg(const char *fmt, ...);
int stderr_ret(const char *fmt, ...);
int stderr_quit(const char *fmt, ...);
int stderr_abort(const char *fmt, ...);
int stderr_pabort(const char *file_name, unsigned line_num, const char *fmt, ...);
int stderr_sys(const char *fmt, ...);
int stderr_dump(const char *fmt, ...);

#define STD_EXCEPTION(x) \
stderr_dump("STD_EXCEPTION: file: %s, line: %d, msg: %s", __FILE__, __LINE__, #x)

#ifdef STD_USE_EXCEPTIONS
# define STD_ERROR(x)      STD_EXCEPTION(x)
# define STD_ON_ERROR(x)   ((x) ? STD_EXCEPTION(x) : 0)
# define STD_MEM_ERROR(x)  ((x) ? (x) : STD_EXCEPTION(x))
#else
# define STD_ERROR(x)      (x)
# define STD_ON_ERROR(x)   (x)
# define STD_MEM_ERROR(x)  (x)
#endif

#ifdef STD_BOUNDS_CHECKS
# define STD_BOUNDS_CHECK(x) if (!(x)) STD_EXCEPTION(bounds check (x) failed)
#else
# define STD_BOUNDS_CHECK(x)
#endif

#ifdef STD_CONSTRUCT_CHECKS
# define STD_CONSTRUCT_CHECK(x) if (!(x)) STD_EXCEPTION(construction check (x) failed)
#else
# define STD_CONSTRUCT_CHECK(x)
#endif

#ifdef STD_SAFE_CHECKS
# define STD_SAFE_CHECK(x) if (!(x)) STD_EXCEPTION(safety check (x) failed)
#else
# define STD_SAFE_CHECK(x) 
#endif

/* error codes returned by stdutil fcns */
enum {
  STD_SUCCESS       =  0, /* hmmm... I'm not sure about this one */
  STD_MEM_FAILURE   = -1, /* a call to an alloc fcn failed or a memory request size overflowed */
  STD_MEM_FAILURE2  =  0, /* used for mem failures when a ptr is returned */
  STD_ILLEGAL_PARAM = -2, /* an illegal parameter was passed by the caller */
  STD_FAILURE       = -5  /* an unnamed failure occurred */
};

/* internal use - didn't feel like putting in _p.h file :( */
enum { STD_NO_MEM_CHANGE = -3, STD_MEM_CHANGE = -4 };

#endif
