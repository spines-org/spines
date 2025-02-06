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

#ifndef MULTICAST_H
#define MULTICAST_H


#include "node.h"

#define     JOIN_GROUP        0X0001
#define     LEAVE_GROUP       0x0010
#define     RELIABLE_GROUP    0x0100
#define     ACTIVE_GROUP      0x000f

#define     IS_JOIN_GROUP(x)     (x & JOIN_GROUP)
#define     IS_LEAVE_GROUP(x)    (x & LEAVE_GROUP)
#define     IS_RELIABLE_GROUP(x) (x & RELIABLE_GROUP)
#define     IS_ACTIVE_GROUP(x)   (x & ACTIVE_GROUP)

#define     GROUP_STATUS(x)      IS_RELIABLE_GROUP(x),IS_JOIN_GROUP(x),IS_LEAVE_GROUP(x),IS_ACTIVE_GROUP(x) 
#define     GSTAT                "R%dJ%dL%dA%d"

typedef struct Group_State_d {
    int32 source_addr;           /* Address of the node that joins / leaves*/
    int32 dest_addr;             /* Group name (address) */
    int32 timestamp_sec;         /* Original timestamp of the last change (seconds) */
    int32 timestamp_usec;        /* ...microseconds */
    int32 my_timestamp_sec;      /* Local timestamp of the last upadte (seconds) */
    int32 my_timestamp_usec;     /* ...microseconds */
    int16 flags;                 /* Group status (joined/ leaved, etc.) */
    int16 age;                   /* Life of the state (in tens of seconds) */

    stdhash joined_sessions;     /* Local sessions that joined the group */
} Group_State;


/* Represent a group update to be sent. It refers to a group update
   and the mask is set to which nodes sould not hear about 
   this update (b/c they may already know about it) */
typedef struct Changed_Group_State_d {
    struct Edge_d *edge;
	int32u mask[MAX_LINKS/(MAX_LINKS_4_EDGE*32)]; 
} Changed_Group_State;


stdhash* Groups_All_States(void); 
stdhash* Groups_All_States_by_Name(void); 
stdhash* Groups_Changed_States(void); 
int Groups_State_type(void);
int Groups_State_header_size(void);
int Groups_Cell_packet_size(void);
int Groups_Is_route_change(void);
int Groups_Is_state_relevant(void *state);
int Groups_Set_state_header(void *state, char *pos);
int Groups_Set_state_cell(void *state, char *pos);
int Groups_Process_state_header(char *pos, int32 type);
void* Groups_Process_state_cell(int32 source, char *pos);
int Groups_Destroy_State_Data(void *state);

int Join_Group(int32 mcast_address, Session *ses);
int Leave_Group(int32 mcast_address, Session *ses);
Group_State* Create_Group(int32 node_address, int32 mcast_address);
stdhash* Get_Mcast_Neighbors(int32 sender, int32 mcast_address);
void Discard_Mcast_Neighbors(int32 mcast_address);
void Discard_All_Mcast_Neighbors(void); 
void Trace_Group(int32 mcast_address, spines_trace *spines_tr);
int Get_Group_Members(int32 mcast_address, spines_trace *spines_tr);
void Print_Mcast_Groups(int dummy_int, void* dummy);

#endif

