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
 * Copyright (c) 2003 - 2007 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
 *
 */


#ifndef NODE_H
#define NODE_H


#define LOCAL_NODE               0x0001
#define NEIGHBOR_NODE            0x0002
#define CONNECTED_NODE           0x0004
#define NOT_YET_CONNECTED_NODE   0x0008
#define REMOTE_NODE              0x0010

#ifdef SPINES_SSL
#include <openssl/ssl.h>
#endif
#include "link.h"


typedef struct Node_d {
    int32 address;               /* IP Address */
    int16 node_id;               /* Index in the global neighbor nodes array */
    int16 node_no;               /* Ordered id of all the nodes in the system */
    int16 flags;                 /* Connected, Neighbor, etc. */
    int16 counter;               /* Number of outstanding "hello" messages */

    sp_time last_time_heard;     /* Last time I heard from this node (not fully implemented) */
    struct Link_d *link[MAX_LINKS_4_EDGE]; /* Links to this node, if a neighbor */

    /* Routing info */
    stdhash multicast_routes;    /* hash keyed on address of destination, and
				    having data of address of forwarder */

    /* Routing info for Dijkstra only */
    int cost;
    int distance;
    struct Node_d *forwarder;

    struct Node_d *prev;         /* Used to build an ordered linked list */
    struct Node_d *next;         /* Used to build an ordered linked list */

    char *device_name;           /* Device name to reach this node, if a neighbors */ 
} Node;

#ifdef SPINES_SSL
struct send_item {
    SSL *ssl;
    char pseudo_scat[MAX_PACKET_SIZE];
    int size;
};
#endif

void Init_Nodes(void);
void Create_Node(int32 address, int16 mode);
void Disconnect_Node(int32 address);
int  Try_Remove_Node(int32 address);

#endif
