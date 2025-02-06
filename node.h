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


#ifndef NODE_H
#define NODE_H

#include "link.h"

#define LOCAL_NODE               0x0001
#define NEIGHBOR_NODE            0x0002
#define CONNECTED_NODE           0x0004
#define NOT_YET_CONNECTED_NODE   0x0008
#define REMOTE_NODE              0x0010


typedef struct Node_d {
    int32 address;               /* IP Address */
    int16 node_id;               /* Index in the global neighbor nodes array */
    int16 flags;                 /* Connected, Neighbor, etc. */
    sp_time last_time_heard;     /* Last time I heard from this node */
    int16 counter;               /* Number of unacked "hello" messages */
    struct Link_d *link[MAX_LINKS_4_EDGE]; /* Links to this node, if any */

    /* Routing info */
    struct Node_d *forwarder;    /* Neighbor to route packets for this node */
    int32 distance;              /* Number of hops to this node */
    int32 cost;                  /* Cost of sending to this node */
    struct Node_d *prev;         /* Used to build a linked list for Dijkstra's
				  * algorithm */
    struct Node_d *next;         /* Used to build a linked list for Dijkstra's
				  * algorithm */
} Node;


void Init_Nodes(void);
void Create_Node(int32 address, int16 mode);
void Disconnect_Node(int32 address);
int  Try_Remove_Node(int32 address);

#endif
