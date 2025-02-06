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

#include <stdlib.h>

#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/memory.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "objects.h"
#include "link.h"
#include "node.h"
#include "route.h"
#include "state_flood.h"
#include "net_types.h"
#include "hello.h"
#include "session.h"
#include "multicast.h"

#define MCAST_SNAPSHOT "/tmp/mcast.snapshot"

/* Global variables */
extern int32    My_Address;
extern stdhash  All_Nodes;
extern stdhash  All_Groups_by_Node;
extern stdhash  All_Groups_by_Name;
extern stdhash  Changed_Group_States;
extern Prot_Def Groups_Prot_Def;

int File_Open = 0;

/* Local variables */
static const sp_time zero_timeout        = {     0,      0};

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
/* int Groups_Process_state_header(char *pos, int32 type); */
/*                                                         */
/* Process the join/leave packet header additional fields  */
/* (nothing for now)                                       */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* pos: pointer to where to set the fields in the packet   */
/* type: contains the endianess of the message             */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) Number of bytes processed                         */
/*                                                         */
/***********************************************************/

int Groups_Process_state_header(char *pos, int32 type)
{
    /* Nothing for now... */
    /* Just flip the header endianess if necessary */
    
    group_state_packet *g_st_pkt;
    
    g_st_pkt = (group_state_packet*)pos;

    if(!Same_endian(type)) {
	g_st_pkt->source = Flip_int32(g_st_pkt->source);
	g_st_pkt->num_cells = Flip_int16(g_st_pkt->num_cells);
	g_st_pkt->src_data = Flip_int16(g_st_pkt->src_data);
    }

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
    int leave_group_state = 0;
    sp_time now;

    group_cell = (group_cell_packet*)pos; 

    g_state = (Group_State*)Find_State(&All_Groups_by_Node, source, 
			 group_cell->dest);

    if(source == My_Address) {
	/* Somebody tells me about my groups */
	Alarm(DEBUG, "Somebody tells me about my groups\n");
	if(g_state == NULL) {
	    Alarm(DEBUG, "g_state null\n");
	    leave_group_state = 1;
	}
    }

    /* Did I know about this group ? */
    if(g_state == NULL) {
	/* This is about a group that I didn't know about until now... */ 
	
	g_state = Create_Group(source, group_cell->dest);
	g_state->timestamp_sec = 0;
	g_state->timestamp_usec = 0;
    }

    Alarm(DEBUG, "\nGot group: %d.%d.%d.%d -> %d.%d.%d.%d, flags: %d\n",
	  IP1(source), IP2(source), 
	  IP3(source), IP4(source), 
	  IP1(group_cell->dest), IP2(group_cell->dest), 
	  IP3(group_cell->dest), IP4(group_cell->dest),
	  group_cell->flags);
    
    
    Alarm(DEBUG, "Got: %d : %d ||| mine: %d : %d\n", 
	  group_cell->timestamp_sec, group_cell->timestamp_usec,
	  g_state->timestamp_sec, g_state->timestamp_usec);

    /* Update group structure here... */
    g_state->timestamp_sec = group_cell->timestamp_sec;
    g_state->timestamp_usec = group_cell->timestamp_usec;
    g_state->age = group_cell->age;
    g_state->flags = group_cell->flags;


    if(leave_group_state == 1) {
	now = E_get_time();
	g_state->timestamp_sec = now.sec; 
	g_state->timestamp_usec = now.usec; 
	g_state->my_timestamp_sec  = now.sec;
	g_state->my_timestamp_usec = now.usec;
	g_state->age = 0;
	g_state->flags = g_state->flags & !ACTIVE_GROUP;

	Add_to_changed_states(&Groups_Prot_Def, My_Address, (void*)g_state, NEW_CHANGE);	

	return(NULL);
    }
    else {
        return((void*)g_state);
    }
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
    stdit it;
    int ret = 1;
    
    if(!Is_mcast_addr(mcast_address) && !Is_acast_addr(mcast_address)) {
	Alarm(DEBUG, "Join_Group: This is not a multicast address\n");
	return(-1);		
    }
    
    Alarm(DEBUG, "Join group: %d.%d.%d.%d\n", 
	  IP1(mcast_address), IP2(mcast_address), IP3(mcast_address), IP4(mcast_address));

    stdhash_find(&ses->joined_groups, &it, &mcast_address);
    if(!stdhash_is_end(&ses->joined_groups, &it)) {
	/* The session already joined that group. Just return */
	return(1);
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
    Alarm(PRINT,"Insert group into &ses->joined_groups.\n");
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
    stdit it;
   
    if(!Is_mcast_addr(mcast_address) && !Is_acast_addr(mcast_address)) {
	Alarm(DEBUG, "Leave_Group: This is not a multicast address\n");
	return(-1);		
    }

    Alarm(PRINT, "Leave group: %d.%d.%d.%d\n", 
	  IP1(mcast_address), IP2(mcast_address), IP3(mcast_address), IP4(mcast_address));


    now = E_get_time();
    stdhash_find(&ses->joined_groups, &it, &mcast_address);
    if(stdhash_is_end(&ses->joined_groups, &it)) {
	/* The session did not join that group. Just return */
	return(1);
    }

    g_state = *((Group_State **)stdhash_it_val(&it));
    stdhash_erase(&ses->joined_groups, &it);

    stdhash_find(&g_state->joined_sessions, &it, &ses->sess_id);
    if(stdhash_is_end(&g_state->joined_sessions, &it)) {
	Alarm(PRINT, "BUG Leave_Group(): Session not in the group array\n");
	//Alarm(EXIT, "BUG Leave_Group(): Session not in the group array\n");
    } else 
        stdhash_erase(&g_state->joined_sessions, &it);
    
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
	if((g_state->flags & ACTIVE_GROUP) == 0) {
            Alarm(DEBUG, "BUG: Should have been active");
        }
	g_state->flags = g_state->flags & !ACTIVE_GROUP;

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
    stdit it, st_it;


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
			  NULL, NULL, 0);
    }
    
    /* Insert the group in the global data structures */

    /* All_Groups_by_Node */
    stdhash_find(&All_Groups_by_Node, &it, &node_address);
    if(stdhash_is_end(&All_Groups_by_Node, &it)) {
	if((s_chain_addr = (State_Chain*) new(STATE_CHAIN))==NULL)
	    Alarm(EXIT, "Create_Group: Cannot allocte object\n");
	s_chain_addr->address = node_address;
	s_chain_addr->p_states = NULL;

       	stdhash_construct(&s_chain_addr->states, sizeof(int32), sizeof(Group_State*), 
			  NULL, NULL, 0);
	stdhash_insert(&All_Groups_by_Node, &it, &node_address, &s_chain_addr);
	stdhash_find(&All_Groups_by_Node, &it, &node_address);
    }    
    s_chain_addr = *((State_Chain **)stdhash_it_val(&it));
    stdhash_insert(&s_chain_addr->states, &it, &mcast_address, &g_state);


    /* All_Groups_by_Name */
    stdhash_find(&All_Groups_by_Name, &it, &mcast_address);
    if(stdhash_is_end(&All_Groups_by_Name, &it)) {
	if((s_chain_grp = (State_Chain*) new(STATE_CHAIN))==NULL)
	    Alarm(EXIT, "Create_Group: Cannot allocte object\n");
	s_chain_grp->address = mcast_address;
	s_chain_grp->p_states = NULL;
	stdhash_construct(&s_chain_grp->states, sizeof(int32), sizeof(Group_State*), 
			  NULL, NULL, 0);
	stdhash_insert(&All_Groups_by_Name, &it, &mcast_address, &s_chain_grp);
	stdhash_find(&All_Groups_by_Name, &it, &mcast_address);
    }
    s_chain_grp = *((State_Chain **)stdhash_it_val(&it));
    stdhash_insert(&s_chain_grp->states, &st_it, &node_address, &g_state);

    return(g_state);
}


/***********************************************************/
/* stdhash* Get_Mcast_Neighbors(int32 sender,              */
/*                              int32 mcast_address)       */
/*                                                         */
/* Gets a hash with neighbors to which the mcast or acast  */
/* packet needs to be forwarded                            */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender: address of the sender node                      */
/* mcast_address: group address                            */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (stdhash*) pointer to the neighbors hash                */
/*                                                         */
/***********************************************************/

stdhash* Get_Mcast_Neighbors(int32 sender, int32 mcast_address)
{
    stdit grp_it, nd_it, ngb_it, st_it;
    State_Chain *s_chain_grp;
    stdhash *nodes_list, *neighbors;
    Node *next_hop, *best_next_hop;
    Group_State *g_state;
    Route *route;    
    int16 lowest_cost = 30001; /* Max Cost is 30000 */

    stdhash_find(&All_Groups_by_Name, &grp_it, &mcast_address);
    if(stdhash_is_end(&All_Groups_by_Name, &grp_it)) {
	return(NULL);
    }
    s_chain_grp = *((State_Chain **)stdhash_it_val(&grp_it));

    if(s_chain_grp->p_states == NULL) {
	if((s_chain_grp->p_states = (stdhash*) new(STDHASH_OBJ)) == NULL) {
	    Alarm(EXIT, "Get_Mcast_Neighbors(): cannot allocate memory\n");
	}
	stdhash_construct(s_chain_grp->p_states, sizeof(int32), sizeof(stdhash*), 
			  NULL, NULL, 0);
    }

    /* Look in the cache */
    nodes_list = s_chain_grp->p_states;
    stdhash_find(nodes_list, &nd_it, &sender);
    if(!stdhash_is_end(nodes_list, &nd_it)) {
	return(*((stdhash **)stdhash_it_val(&nd_it)));
    }

    if((neighbors = (stdhash*) new(STDHASH_OBJ)) == NULL) {
	Alarm(EXIT, "Get_Mcast_Neighbors(): cannot allocate memory\n");
    }
    stdhash_construct(neighbors, sizeof(int32), sizeof(Node*), 
		      NULL, NULL, 0);

    best_next_hop = NULL;
    stdhash_begin(&s_chain_grp->states, &st_it);
    if (sender != My_Address && Get_Route(My_Address, sender) == NULL) {
        stdhash_end(&s_chain_grp->states, &st_it);
    } 
    while(!stdhash_is_end(&s_chain_grp->states, &st_it)) {
	g_state = *((Group_State **)stdhash_it_val(&st_it));
	if(g_state->flags & ACTIVE_GROUP) {
	    /* If I find myself, I am the acast destination */
	    if (Is_acast_addr(mcast_address) && 
		    g_state->source_addr == My_Address ) {
		best_next_hop = NULL;
		break;
	    }
	    next_hop = Get_Route(sender, g_state->source_addr);

	    if(next_hop != NULL) {
		stdhash_find(neighbors, &ngb_it, &next_hop->address);
		if (Is_mcast_addr(mcast_address)) { 
		    /* Multicast */
		    if(stdhash_is_end(neighbors, &ngb_it)) {
			stdhash_insert(neighbors, &ngb_it, &next_hop->address, &next_hop);
		    }
		} else if (Is_acast_addr(mcast_address)) {
		    /* Anycast */
		    route = Find_Route(sender, g_state->source_addr);
		    /* Could add rand when found one that is equal, but
		       will alternate routes */
		    if ( route != NULL && route->cost < lowest_cost ) {
			best_next_hop = next_hop;
			lowest_cost = route->cost;
		    }
		}
	    }
	}
	stdhash_it_next(&st_it);
    }
    if (Is_acast_addr(mcast_address) && best_next_hop != NULL) { 
	stdhash_insert(neighbors, &ngb_it, &best_next_hop->address, &best_next_hop); 
    }
    stdhash_insert(nodes_list, &nd_it, &sender, &neighbors);

    return(neighbors);
}

/***********************************************************/
/* void Discard_Mcast_Neighbors(int32 mcast_address)       */
/*                                                         */
/* Discards the hash with neighbors to which the           */
/* mcast packet are forwarded                              */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* mcast_address: group address                            */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/
 
void Discard_Mcast_Neighbors(int32 mcast_address) 
{
    stdit grp_it, nd_it;
    State_Chain *s_chain_grp;
    stdhash *nodes_list, *neighbors;

    stdhash_find(&All_Groups_by_Name, &grp_it, &mcast_address);
    if(stdhash_is_end(&All_Groups_by_Name, &grp_it)) {
	return;
    }
    s_chain_grp = *((State_Chain **)stdhash_it_val(&grp_it));

    if(s_chain_grp->p_states == NULL) {
	return;
    }
    nodes_list = s_chain_grp->p_states;
    stdhash_begin(nodes_list, &nd_it);
    while(!stdhash_is_end(nodes_list, &nd_it)) {
	neighbors = *((stdhash **)stdhash_it_val(&nd_it));
	stdhash_destruct(neighbors);
	dispose(neighbors);
	stdhash_it_next(&nd_it);
    }
    stdhash_destruct(nodes_list);
    dispose(nodes_list);
    s_chain_grp->p_states = NULL;
}


/***********************************************************/
/* void Discard_All_Mcast_Neighbors(void)                  */
/*                                                         */
/* Discards all the hash with neighbors to which the       */
/* mcast packet are forwarded                              */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Discard_All_Mcast_Neighbors(void) 
{
    stdit grp_it, nd_it;
    State_Chain *s_chain_grp;
    stdhash *nodes_list, *neighbors;

    stdhash_begin(&All_Groups_by_Name, &grp_it);
    while(!stdhash_is_end(&All_Groups_by_Name, &grp_it)) {
	s_chain_grp = *((State_Chain **)stdhash_it_val(&grp_it));
	
	if(s_chain_grp->p_states != NULL) {
	    nodes_list = s_chain_grp->p_states;
	    stdhash_begin(nodes_list, &nd_it);
	    while(!stdhash_is_end(nodes_list, &nd_it)) {
		neighbors = *((stdhash **)stdhash_it_val(&nd_it));
		stdhash_destruct(neighbors);
		dispose(neighbors);
		stdhash_it_next(&nd_it);
	    }
	    stdhash_destruct(nodes_list);
	    dispose(nodes_list);
	    s_chain_grp->p_states = NULL;
	}
	stdhash_it_next(&grp_it);
    }
}

void Trace_Group(int32 mcast_address, spines_trace *spines_tr) 
{
    stdit nd_it;
    Node *nd;
    Route *route;
    spines_trace spt;
    int i, j, current_ed, is_reachable;

    /* Get all active group members */
    memset(&spt, 0, sizeof(spt));
    Get_Group_Members(mcast_address, &spt);

    /* For each possible source, what is the maximum distance to any
       of the group members (Group Euclidean Distance) */
    i = 0;
    stdhash_begin(&All_Nodes, &nd_it); 
    while(!stdhash_is_end(&All_Nodes, &nd_it)) {
        nd = *((Node **)stdhash_it_val(&nd_it));

        /* Is this node at all reachable */
        is_reachable = 0;
        if (nd->address == My_Address) {
            is_reachable = 1;
        } else {
            route = Find_Route(My_Address, nd->address);
            if (route != NULL) {
                if(route->forwarder != NULL) {
                    is_reachable = 1;
                }
            }
        }
        if (is_reachable == 0) {
	    stdhash_it_next(&nd_it);
            continue;
        }

        current_ed = 0;
        for (j=0;j<spt.count;j++) {
	    route = Find_Route(nd->address, spt.address[j]);
            if (route != NULL) {
                if (route->distance >=0 && route->cost >=0) {
                    if (route->distance > current_ed) {
                        current_ed = route->distance;
                    }
                }
            }
            /* TODO: How many links need to be crossed to reach
               all of the grouop members (Group Euclidean Cost) */
        }
        spines_tr->address[i]=nd->address;
        spines_tr->distance[i]=current_ed;
        spines_tr->cost[i]=0;
        i++;
	stdhash_it_next(&nd_it);
    }
    spines_tr->count = i;
}

int Get_Group_Members(int32 mcast_address, spines_trace *spines_tr)
{
    stdit grp_it, st_it;
    State_Chain *s_chain_grp;
    Route *route;
    Group_State *g_state;
    int i;

    stdhash_find(&All_Groups_by_Name, &grp_it, &mcast_address);
    if(stdhash_is_end(&All_Groups_by_Name, &grp_it)) {
	return 0;
    }
    s_chain_grp = *((State_Chain **)stdhash_it_val(&grp_it));

    i=0;
    stdhash_begin(&s_chain_grp->states, &st_it);
    while(!stdhash_is_end(&s_chain_grp->states, &st_it)) {
	g_state = *((Group_State **)stdhash_it_val(&st_it));
	if(g_state->flags & ACTIVE_GROUP) {
            if (g_state->source_addr == My_Address) {
                spines_tr->address[i] = g_state->source_addr;
                spines_tr->distance[i] = 0;
                spines_tr->cost[i] = 0;
                i++;
            } else {
                route = Find_Route(My_Address, g_state->source_addr);
	        if (route != NULL) {
                    if (route->distance >= 0 && route->cost >= 0) {
                        spines_tr->address[i] = g_state->source_addr;
                        spines_tr->distance[i] = route->distance;
                        spines_tr->cost[i] = route->cost;
                        i++;
                    }
                }
            } 
        }
        if (i == MAX_COUNT) {
            break;
        }
        stdhash_it_next(&st_it);
    }
    spines_tr->count = i;
    return 1;
}

/***********************************************************/
/* void Print_Mcast_Groups(void)                           */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* fp                                                      */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

#define MCAST_SNAPSHOT_FILE "/tmp/spines.groups"

void Print_Mcast_Groups(int dummy_int, void* dummy)
{
    const sp_time print_timeout = {    15,    0};
    FILE *fp = NULL;
    stdit grp_it, ngb_it;
    State_Chain *s_chain_grp;
    Node *next_hop;
    stdhash *neighbors;
    spines_trace spt;
    int i;

    stdhash_begin(&All_Groups_by_Name, &grp_it);
    fp = fopen(MCAST_SNAPSHOT_FILE, "w");
    if (fp == NULL) {
	perror("Could not open mcast snapshot file\n");
	return;
    }
    while(!stdhash_is_end(&All_Groups_by_Name, &grp_it)) {
	s_chain_grp = *((State_Chain **)stdhash_it_val(&grp_it));

        /* Print Group */
	fprintf(fp, "\n\n\n"IPF, IP(s_chain_grp->address));
       
        /* Print Current Membership */
        memset(&spt, 0, sizeof(spt));
        Get_Group_Members(s_chain_grp->address, &spt);
        fprintf(fp, "\n\n\tOverlay Membership: %d\n", spt.count); 
        for (i=0;i<spt.count;i++) {
            fprintf(fp, "\n\t\t"IPF": %d: %d", 
                    IP((int)(spt.address[i])), spt.distance[i], spt.cost[i]);
        }

        /* Print Forwarding Rule */
        neighbors = Get_Mcast_Neighbors(My_Address, s_chain_grp->address);
        fprintf(fp, "\n\n\tForwarding Table, Source=ME\n"); 
	if(neighbors != NULL) {
	    stdhash_begin(neighbors, &ngb_it);
	    while(!stdhash_is_end(neighbors, &ngb_it)) {
		next_hop = *((Node **)stdhash_it_val(&ngb_it));
                if (next_hop != NULL) {
		    fprintf(fp, "\n\t\t-->"IPF, IP(next_hop->address));
		}
		stdhash_it_next(&ngb_it);
	    }
	}
	fprintf(fp, "\n");
	stdhash_it_next(&grp_it);
    }
    fclose(fp);
    E_queue(Print_Mcast_Groups, 0, NULL, print_timeout);
}

