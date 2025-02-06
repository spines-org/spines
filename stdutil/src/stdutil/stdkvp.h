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

#ifndef stdkvp_h_2000_06_02_02_20_47_jschultz_at_cnds_jhu_edu
#define stdkvp_h_2000_06_02_02_20_47_jschultz_at_cnds_jhu_edu

#include <stdutil/stddefines.h>

typedef struct {
  void *key;
  void *value;
} stdkvp;

typedef stdbool (*stdequals_fcn)(const void *, const void *);
typedef size_t (*stdhcode_fcn)(const void *);
typedef stdssize_t (*stdcmp)(const void *, const void *);

#endif
