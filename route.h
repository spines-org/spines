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


#ifndef ROUTE_H
#define ROUTE_H

#include "node.h"

#define REMOTE_ROUTE    0
#define LOCAL_ROUTE     1

#define DISTANCE_ROUTE  0
#define LATENCY_ROUTE   1
#define LOSSRATE_ROUTE  2


typedef struct Route_d {
    int32 source;                /* IP Address of source */
    int32 dest;                  /* IP Address of destination */
    int32 distance;              /* Number of hops on this route */
    int32 cost;                  /* Cost of sending on this route */
    Node *forwarder;             /* Neighbor that will forward towards dest. */
} Route;

void Init_Routes(void) ;
Route* Find_Route(int32 source, int32 dest); 
void Set_Routes(void);
Node* Get_Route(int32 source, int32 dest);
void Print_Routes(void); 

#endif
