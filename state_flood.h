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


#ifndef STATE_FLOOD_H
#define STATE_FLOOD_H

#include "link.h"

typedef struct Prot_Def_d {
    stdhash* (*All_States)(void); 
    stdhash* (*All_States_by_Dest)(void); 
    stdhash* (*Changed_States)(void); 
    int (*State_type)(void);
    int (*State_header_size)(void);
    int (*Cell_packet_size)(void);
    int (*Is_route_change)(void);
    int (*Is_state_relevant)(void *state);
    int (*Set_state_header)(void *state, char *pos);
    int (*Set_state_cell)(void *state, char *pos);
    int (*Process_state_header)(char *pos);
    void* (*Process_state_cell)(int32 source, char *pos);
    int (*Destroy_State_Data)(void *state);
} Prot_Def;



/* Represents a state update to be sent. It refers to a state
   and the mask is set to which nodes sould not hear about 
   this update (b/c they already know it) */
typedef struct Changed_State_d {
    void *state;
    int32u mask[MAX_LINKS/(MAX_LINKS_4_EDGE*32)]; 
} Changed_State;


/* This is the begining of any state data structure */
typedef struct State_Data_d {
    int32 source_addr;           /* Source address */
    int32 dest_addr;             /* Destination address */
    int32 timestamp_sec;         /* Original timestamp of the last change (secnds) */
    int32 timestamp_usec;        /* ...microseconds */
    int32 my_timestamp_sec;      /* Local timestamp of the last upadte (seconds) */
    int32 my_timestamp_usec;     /* ...microseconds */
    int16 value;                 /* Value of the state */
    int16 age;                   /* Life of the state (in tens of seconds) */
} State_Data;


typedef struct State_Chain_d {
    int32 address;
    stdhash states;
} State_Chain;


/* This is the begining of any state packet */
typedef struct  State_Packet_d {
    int32           source;
    int16u          num_cells;
    int16           src_data; /* Data about the source source itself. 
                                  Not used yet */
} State_Packet;



/* This is the begining of any state cell */
typedef struct  State_Cell_d {
    int32           dest;
    int32           timestamp_sec;
    int32           timestamp_usec;
    int16           value;
    int16           age;
} State_Cell;


void Process_state_packet(int32 sender, char *buf, 
			  int16u data_len, int16u ack_len, 
			  int32u type, int mode);
void Net_Send_State_All(int lk_id, void *p_data); 
int Net_Send_State_Updates(Prot_Def *p_def, int16 node_id);
Prot_Def* Get_Prot_Def(int32u type);
void Add_to_changed_states(Prot_Def *p_def, int32 source, 
			   State_Data *s_data, int mode);
State_Data* Find_State(stdhash *hash_struct, int32 source, 
		       int32 dest); 
Changed_State* Find_Changed_State(stdhash *hash_struct, 
				  int32 source, int32 dest);
void Empty_Changed_States(stdhash *hash_struct);
void Resend_States(int dummy_int, void* p_data);
void State_Garbage_Collect(int dummy_int, void* p_data);


#endif

