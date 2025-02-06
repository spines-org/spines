/*
 * Spines.
 *     
 * The contents of this file are subject to the Spines Open-Source
 * License, Version 1.0 (the ``License''); you may not use
 * this file except in compliance with the License.  You may obtain a
 * copy of the License at:
 *
 * http://www.spines.org/LICENSE.txt
 *
 * or in the file ``LICENSE.txt'' found in this distribution.
 *
 * Software distributed under the License is distributed on an AS IS basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License.
 *
 * The Creators of Spines are:
 *  Yair Amir and Claudiu Danilov.
 *
 * Copyright (c) 2003 - 2008 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
 *
 */


#ifndef OBJECTS_H
#define OBJECTS_H

#include "util/arch.h"

#define MAX_OBJECTS             200
#define MAX_OBJ_USED            (UNKNOWN_OBJ+1)

/* Object types 
 *
 * Object types must start with 1 and go up. 0 is reserved 
 */


/* Util objects */
#define BASE_OBJ                1
#define TIME_EVENT              2
#define QUEUE                   3
#define QUEUE_SET               4
#define QUEUE_ELEMENT           5
#define MQUEUE_ELEMENT          6
#define SCATTER                 7


/* Transmitted objects */
#define PACK_HEAD_OBJ           10
#define PACK_BODY_OBJ           11
#define SYS_SCATTER             12
#define STDHASH_OBJ             13


/* Non-Transmitted objects */
#define TREE_NODE               21
#define DIRECT_LINK             22
#define OVERLAY_EDGE            23
#define OVERLAY_ROUTE           24
#define CHANGED_STATE           25
#define STATE_CHAIN             26
#define MULTICAST_GROUP         27

#define BUFFER_CELL             31
#define UDP_CELL                32
#define FRAG_PKT                33

#define CONTROL_DATA            41
#define RELIABLE_DATA           42
#define REALTIME_DATA           43

#define SESSION_OBJ             51


#define REL_MCAST_TREE_OBJ	52 

#ifdef SPINES_SSL
/* added for security part */
#define SSL_IP_BUFFER           53
#define SSL_PKT_BUFFER          54
#define SSL_SOCKADDR_IN         55
#endif

/* Special objects */
#define UNKNOWN_OBJ             56      /* This represents an object of undertermined or 
                                         * variable type.  Can only be used when appropriate.
                                         * i.e. when internal structure of object is not accessed.
                                         * This is mainly used with queues
                                         */

/* Global Functions to manipulate objects */
int     Is_Valid_Object(int32u oid);
char    *Objnum_to_String(int32u obj_type);

#endif /* OBJECTS_H */


