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
 * Copyright (c) 2003 - 2009 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
 *
 */

#ifndef LINK_STATE_H
#define LINK_STATE_H

#include "node.h"

typedef struct Edge_d {
    int32 source_addr;            /* Source address */
    int32 dest_addr;              /* Destination address */
    int32 timestamp_sec;          /* Original timestamp of the last change (seconds) */
    int32 timestamp_usec;         /* ...microseconds */
    int32 my_timestamp_sec;       /* Local timestamp of the last upadte (seconds) */
    int32 my_timestamp_usec;      /* ...microseconds */
    int16 cost;                   /* cost of the edge */
    int16 age;                    /* Life of the state (in tens of seconds) */

    struct Node_d *source;        /* Source node */
    struct Node_d *dest;          /* Destination node */
    int16  flags;                 /* Edge status */
} Edge;


/* Represent a link update to be sent. It refers to an edge
   and the mask is set to which nodes sould not hear about 
   this update (b/c they already know it) */
typedef struct Changed_Edge_d {
    struct Edge_d *edge;
	int32u mask[MAX_LINKS/(MAX_LINKS_4_EDGE*32)]; 
} Changed_Edge;


stdhash* Edge_All_States(void); 
stdhash* Edge_All_States_by_Dest(void); 
stdhash* Edge_Changed_States(void); 
int Edge_State_type(void);
int Edge_State_header_size(void);
int Edge_Cell_packet_size(void);
int Edge_Is_route_change(void);
int Edge_Is_state_relevant(void *state);
int Edge_Set_state_header(void *state, char *pos);
int Edge_Set_state_cell(void *state, char *pos);
int Edge_Process_state_header(char *pos, int32 type);
void* Edge_Process_state_cell(int32 source, char *pos);
int Edge_Destroy_State_Data(void *state);


Edge* Create_Overlay_Edge(int32 source, int32 dest);
Edge* Destroy_Edge(int32 source, int32 dest, int local_call);
int   Edge_Update_Cost(int link_id, int mode);
void  Print_Edges(int dummy_int, void* dummy); 

#endif
