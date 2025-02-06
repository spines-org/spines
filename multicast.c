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


#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/memory.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "objects.h"
#include "link.h"
#include "node.h"
#include "route.h"
#include "multicast.h"
#include "state_flood.h"
#include "net_types.h"
#include "hello.h"
#include "session.h"


/* Global variables */
extern int32    My_Address;

extern stdhash  All_Groups_by_Node;
extern stdhash  All_Groups_by_Name;
extern stdhash  Changed_Group_States;
extern Prot_Def Groups_Prot_Def;


/* Local variables */
static const sp_time zero_timeout        = {     0,    0};


/***********************************************************/
/* stdhash* Groups_All_States(void)                        */
/*                                                         */
/* Returns the hash containing all the known groups        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (stdhash*) Hash with all the groups                     */
/*                                                         */
/***********************************************************/

stdhash* Groups_All_States(void)
{
    return(&All_Groups_by_Node);
}

/***********************************************************/
/* stdhash* Groups_All_States_by_Name(void)                */
/*                                                         */
/* Returns the hash containing all the known groups        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (stdhash*) Hash with all the groups                     */
/*                                                         */
/***********************************************************/

stdhash* Groups_All_States_by_Name(void)
{
    return(&All_Groups_by_Name);
}


/***********************************************************/
/* stdhash* Groups_Changed_States(void)                    */
/*                                                         */
/* Returns the hash containing the buffer of changed groups*/
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (stdhash*) Hash with all the groups                     */
/*                                                         */
/***********************************************************/

stdhash* Groups_Changed_States(void)
{
    return(&Changed_Group_States);
}


/***********************************************************/
/* int Groups_State_type(void)                             */
/*                                                         */
/* Returns the packet header type for join/leave msgs.     */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) type of the packet                                */
/*                                                         */
/***********************************************************/

int Groups_State_type(void)
{
    return(GROUP_STATE_TYPE);
}


/***********************************************************/
/* int Groups_State_header_size(void)                      */
/*                                                         */
/* Returns the size of the join/leave header               */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) size of the join/leave header                     */
/*                                                         */
/***********************************************************/

int Groups_State_header_size(void)
{
    return(sizeof(group_state_packet));
}


/***********************************************************/
/* int Groups_Cell_packet_size(void)                       */
/*                                                         */
/* Returns the size of the join/leave cell                 */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) size of the join/leave cell                       */
/*                                                         */
/***********************************************************/

int Groups_Cell_packet_size(void)
{
    return(sizeof(group_cell_packet));
}



/***********************************************************/
/* int Groups_Is_route_change(void)                        */
/*                                                         */
/* Returns false, join/leave does not change routing...    */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) 0 ; false...                                      */
/*                                                         */
/***********************************************************/

int Groups_Is_route_change(void) 
{
    return(0);
}




/***********************************************************/
/* int Groups_Is_state_relevant(void* state)               */
/*                                                         */
/* Returns true if the group is not removed, false         */
/* otherwise so that the group will not be resent, and     */
/* eventually will be arbage-collected                     */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* state: pointer to the group structure                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) 1 if group is up                                  */
/*       0 otherwise                                       */
/*                                                         */
/***********************************************************/

int Groups_Is_state_relevant(void *state)
{
    Group_State *g_state;

    g_state = (Group_State*)state;
    if(g_state->flags & ACTIVE_GROUP) {
	return(1);
    }
    else {
	return(0);
    }
}





/***********************************************************/
/* int Groups_Set_state_header(void *state, char *pos)     */
/*                                                         */
/* Sets the join/leave packet header additional fields     */
/* (nothing for now)                                       */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* state: pointer to the group structure                   */
/* pos: pointer to where to set the fields in the packet   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) Number of bytes set                               */
/*                                                         */
/***********************************************************/

int Groups_Set_state_header(void *state, char *pos)
{
    /* Nothing for now... */
    return(0);
}




/***********************************************************/
/* int Groups_Set_state_cell(void *state, char *pos)       */
/*                                                         */
/* Sets the link_state cell additional fields              */
/* (cost, maybe loss rate, etc.)                           */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* state: pointer to the edge structure                    */
/* pos: pointer to where to set the fields in the packet   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) Number of bytes set                               */
/*                                                         */
/***********************************************************/

int Groups_Set_state_cell(void *state, char *pos)
{
    /* Nothing for now... */
    return(0);
}


/***********************************************************/
/* int Groups_Destroy_State_Data(void *state)              */
/*                                                         */
/* Destroys specific info from the group structure         */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* void* g_state                                           */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if ok                                          */
/*       -1 if not                                         */
/*                                                         */
/***********************************************************/

int Groups_Destroy_State_Data(void* state)
{
    Group_State *g_state;

    g_state = (Group_State*)state;
    
    if(g_state->source_addr == My_Address) {
	stdhash_destruct(&g_state->joined_sessions);
    }
    return(1);
}




/***********************************************************/
/* int Groups_Process_state_header(char *pos);             */
/*                                                         */
/* Process the join/leave packet header additional fields  */
/* (nothing for now)                                       */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* pos: pointer to where to set the fields in the packet   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) Number of bytes processed                         */
/*                                                         */
/***********************************************************/

int Groups_Process_state_header(char *pos)
{
    /* Nothing for now... */
    return(0);
}



/***********************************************************/
/* void* Groups_Process_state_cell(int32 source, char *pos)*/
/*                                                         */
/* Processes the join/leave cell additional fields         */
/* (flags, etc.)                                           */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* source: node that joins/leaves                          */
/* pos: pointer to the begining of the cell                */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (void*) pointer to the group processed (new or old)     */
/*                                                         */
/***********************************************************/

void* Groups_Process_state_cell(int32 source, char *pos)
{
    Group_State *g_state;
    group_cell_packet *group_cell;


    group_cell = (group_cell_packet*)pos; 

    if(source == My_Address) {
	/* Somebody tells me about my groups */
	g_state = (Group_State*)Find_State(&All_Groups_by_Node, source, 
			 group_cell->dest);
	return((void*)g_state);
    }

    /* Did I know about this group ? */
    if((g_state = (Group_State*)Find_State(&All_Groups_by_Node, source, 
			 group_cell->dest)) == NULL) {
	/* This is about a group that I didn't know about until now... */ 
	
	g_state = Create_Group(source, group_cell->dest);
	g_state->timestamp_sec = 0;
	g_state->timestamp_usec = 0;
    }

    Alarm(DEBUG, "\nGot group: %d.%d.%d.%d -> %d.%d.%d.%d\n",
	  IP1(source), IP2(source), 
	  IP3(source), IP4(source), 
	  IP1(group_cell->dest), IP2(group_cell->dest), 
	  IP3(group_cell->dest), IP4(group_cell->dest));
    
    
    Alarm(DEBUG, "Got: %d : %d ||| mine: %d : %d\n", 
	  group_cell->timestamp_sec, group_cell->timestamp_usec,
	  g_state->timestamp_sec, g_state->timestamp_usec);

    /* Update group structure here... */
    g_state->timestamp_sec = group_cell->timestamp_sec;
    g_state->timestamp_usec = group_cell->timestamp_usec;
    g_state->age = group_cell->age;
    g_state->flags = group_cell->flags;

    return((void*)g_state);
}



/***********************************************************/
/* int Join_Group(int32 mcast_address, Session *ses)       */
/*                                                         */
/* Joins a group locally                                   */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* mcast_address: group address                            */
/* ses: Session that joined the group                      */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  2 if created/activated a new group               */ 
/*        1 if this overlay node already joined the group, */
/*       -1 if failure                                     */
/*                                                         */
/***********************************************************/

int Join_Group(int32 mcast_address, Session *ses)
{
    sp_time now;
    Group_State *g_state;
    stdhash_it it;
    int ret = 1;
    
    if((mcast_address & 0xF0000000) != 0xE0000000) {
	Alarm(DEBUG, "Join_Group: This is not a multicast address\n");
	return(-1);		
    }
    
    Alarm(DEBUG, "Join group: %d.%d.%d.%d\n", 
	  IP1(mcast_address), IP2(mcast_address), IP3(mcast_address), IP4(mcast_address));

    stdhash_find(&ses->joined_groups, &it, &mcast_address);
    if(!stdhash_it_is_end(&it)) {
	/* The session already joined that group. Just return */
	return(-1);
    }

    if(ses->type != UDP_SES_TYPE) {
	Alarm(PRINT, "The session cannot join a group\n");
	return(-1);
    }
          

    now = E_get_time();
    if((g_state = (Group_State*)Find_State(&All_Groups_by_Node, My_Address, mcast_address)) != NULL) {
	/* The group already exists. */
	/* The group might be active (joined) or not */
	if((g_state->flags & ACTIVE_GROUP) == 0) {
	    /* Group is not active... so let's activate it ! */
	    g_state->timestamp_usec++; 
	    if(g_state->timestamp_usec >= 1000000) {
		g_state->timestamp_usec = 0;
		g_state->timestamp_sec++;
	    } 
	    g_state->my_timestamp_sec  = now.sec;
	    g_state->my_timestamp_usec = now.usec;
	    g_state->flags = g_state->flags | ACTIVE_GROUP;
	    Add_to_changed_states(&Groups_Prot_Def, My_Address, (void*)g_state, NEW_CHANGE);
	    ret = 2;
	}
    }
    else {
	/* This is a new join to a new group. Create the group */
	g_state = Create_Group(My_Address, mcast_address);
	if(g_state == NULL) {
	    return(-1);
	}
	g_state->timestamp_sec  = now.sec;
	g_state->timestamp_usec = now.usec;
	g_state->my_timestamp_sec  = now.sec;
	g_state->my_timestamp_usec = now.usec;
	g_state->age = 0;
	g_state->flags = g_state->flags | ACTIVE_GROUP;
	Add_to_changed_states(&Groups_Prot_Def, My_Address, (void*)g_state, NEW_CHANGE);
	ret = 2;
    }
    stdhash_insert(&ses->joined_groups, &it, &mcast_address, &g_state);
    stdhash_insert(&g_state->joined_sessions, &it, &ses->sess_id, &ses);

    return(ret);
}




/***********************************************************/
/* int Leave_Group(int32 mcast_address, Session *ses)      */
/*                                                         */
/* Leaves a group locally                                  */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* mcast_address: group address                            */
/* ses: Session that left the group                        */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if leave was successful                        */
/*       -1 if not                                         */
/*                                                         */
/***********************************************************/

int Leave_Group(int32 mcast_address, Session *ses)
{
    sp_time now;
    Group_State *g_state;
    stdhash_it it;
   
    if((mcast_address & 0xF0000000) != 0xE0000000) {
	Alarm(DEBUG, "Leave_Group: This is not a multicast address\n");
	return(-1);		
    }

    Alarm(DEBUG, "Leave group: %d.%d.%d.%d\n", 
	  IP1(mcast_address), IP2(mcast_address), IP3(mcast_address), IP4(mcast_address));


    now = E_get_time();
    stdhash_find(&ses->joined_groups, &it, &mcast_address);
    if(stdhash_it_is_end(&it)) {
	/* The session did not join that group. Just return */
	return(1);
    }

    g_state = *((Group_State **)stdhash_it_val(&it));
    stdhash_erase(&it);

    stdhash_find(&g_state->joined_sessions, &it, &ses->sess_id);
    if(stdhash_it_is_end(&it)) {
	Alarm(EXIT, "BUG Leave_Group(): Session not in the group array\n");
    }
    stdhash_erase(&it);
    
    if(stdhash_empty(&g_state->joined_sessions)) {
	/* There are no more local sessions joining this group */
	g_state->timestamp_usec++; 
	if(g_state->timestamp_usec >= 1000000) {
	    g_state->timestamp_usec = 0;
	    g_state->timestamp_sec++;
	} 
	g_state->my_timestamp_sec  = now.sec;
	g_state->my_timestamp_usec = now.usec;
	g_state->age = 0;
	g_state->flags = g_state->flags ^ ACTIVE_GROUP;

	Add_to_changed_states(&Groups_Prot_Def, My_Address, (void*)g_state, NEW_CHANGE);	
    }
    return(1);
}




/***********************************************************/
/* Group_State* Create_Group(int32 node_address,           */ 
/*                   int32 mcast_address)                  */
/*                                                         */
/* Creates a group                                         */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* node_address: address of the node                       */
/* mcast_address: group address                            */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Group_State*) pointer to the Group structure           */
/*                                                         */
/***********************************************************/

Group_State* Create_Group(int32 node_address, int32 mcast_address) 
{
    Group_State *g_state;
    State_Chain *s_chain_addr, *s_chain_grp;
    sp_time now;
    stdhash_it it, st_it;


    Alarm(DEBUG, "Create group: %d.%d.%d.%d joined %d.%d.%d.%d\n", 
	  IP1(node_address), IP2(node_address), IP3(node_address), IP4(node_address), 
	  IP1(mcast_address), IP2(mcast_address), IP3(mcast_address), IP4(mcast_address));

    now = E_get_time();
    /*
     * if(Find_State(&All_Groups_by_Node, node_address, mcast_address) != NULL)
     *    Alarm(EXIT, "Create_Group(): Group already exists\n");
     */


    /* Create the group structure */
    if((g_state = (Group_State*) new(MULTICAST_GROUP))==NULL)
	Alarm(EXIT, "Create_Group: Cannot allocte group object\n");

    /* Initialize the group structure */

    if(My_Address == node_address) {
	g_state->timestamp_sec = (int32)now.sec;
	g_state->timestamp_usec = (int32)now.usec;
    }
    else {
	g_state->timestamp_sec = 0;
	g_state->timestamp_usec = 0;
    }
    g_state->age = 0;
    g_state->my_timestamp_sec = (int32)now.sec;
    g_state->my_timestamp_usec = (int32)now.usec;	
   
    g_state->source_addr = node_address;
    g_state->dest_addr = mcast_address;
    g_state->flags = 0;
    if(g_state->source_addr == My_Address) {
	stdhash_construct(&g_state->joined_sessions, sizeof(int32), sizeof(Session*), 
			  stdhash_int_equals, stdhash_int_hashcode);
    }

    /* Insert the group in the global data structures */

    /* All_Groups_by_Node */
    stdhash_find(&All_Groups_by_Node, &it, &node_address);
    if(stdhash_it_is_end(&it)) {
	if((s_chain_addr = (State_Chain*) new(STATE_CHAIN))==NULL)
	    Alarm(EXIT, "Create_Group: Cannot allocte object\n");
	s_chain_addr->address = node_address;

       	stdhash_construct(&s_chain_addr->states, sizeof(int32), sizeof(Group_State*), 
			  stdhash_int_equals, stdhash_int_hashcode);
	stdhash_insert(&All_Groups_by_Node, &it, &node_address, &s_chain_addr);
	stdhash_find(&All_Groups_by_Node, &it, &node_address);
    }    
    s_chain_addr = *((State_Chain **)stdhash_it_val(&it));
    stdhash_insert(&s_chain_addr->states, &it, &mcast_address, &g_state);


    /* All_Groups_by_Name */
    stdhash_find(&All_Groups_by_Name, &it, &mcast_address);
    if(stdhash_it_is_end(&it)) {
	if((s_chain_grp = (State_Chain*) new(STATE_CHAIN))==NULL)
	    Alarm(EXIT, "Create_Group: Cannot allocte object\n");
	s_chain_grp->address = mcast_address;
	stdhash_construct(&s_chain_grp->states, sizeof(int32), sizeof(Group_State*), 
			  stdhash_int_equals, stdhash_int_hashcode);
	stdhash_insert(&All_Groups_by_Name, &it, &mcast_address, &s_chain_grp);
	stdhash_find(&All_Groups_by_Name, &it, &mcast_address);
    }
    s_chain_grp = *((State_Chain **)stdhash_it_val(&it));
    stdhash_insert(&s_chain_grp->states, &st_it, &node_address, &g_state);

    return(g_state);
}
