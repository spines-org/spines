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
 * Copyright (c) 2003 The Johns Hopkins University.
 * All rights reserved.
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

#define BASE_OBJ                1
#define PACK_HEAD_OBJ           2
#define PACK_BODY_OBJ           3

/* Non-Transmitted objects */
#define SCATTER                 20
#define QUEUE_ELEMENT           21
#define QUEUE                   22
#define TREE_NODE               23
#define DIRECT_LINK             24
#define OVERLAY_EDGE            25
#define CHANGED_EDGE            26
#define TIME_EVENT              27
#define QUEUE_SET               28
#define MQUEUE_ELEMENT          29
#define SYS_SCATTER             30
#define BUFFER_CELL             31
#define UDP_CELL                32

#define CONTROL_DATA            41
#define RELIABLE_DATA           42
#define UDP_DATA                43

#define SESSION_OBJ             51

/* Special objects */
#define UNKNOWN_OBJ             52      /* This represents an object of undertermined or 
                                         * variable type.  Can only be used when appropriate.
                                         * i.e. when internal structure of object is not accessed.
                                         * This is mainly used with queues
                                         */

/* Global Functions to manipulate objects */
int     Is_Valid_Object(int32u oid);
char    *Objnum_to_String(int32u obj_type);

#endif /* OBJECTS_H */


