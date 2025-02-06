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
#include "state_flood.h"
#include "link_state.h"
#include "route.h"
#include "multicast.h"

/* Global variables */

extern int32    Address[256];
extern int16    Num_Initial_Nodes;
extern stdhash  All_Nodes;
extern stdhash  All_Edges;
extern stdhash  Changed_Edges;
extern Node*    Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
extern int16    Num_Neighbors;
extern Link*    Links[MAX_LINKS];
extern int32    My_Address;
extern Prot_Def Edge_Prot_Def;
extern stdhash  All_Groups_by_Node; 
extern stdhash  All_Groups_by_Name; 
extern stdhash  Changed_Group_States;
extern stdhash  Monitor_Loss;



/***********************************************************/
/* void Init_Nodes(void)                                   */
/*                                                         */
/* Initializes/creates the node structures                 */
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

void Init_Nodes(void) {
    int16 i;

    Num_Neighbors = 0;
    for(i=0;i<MAX_LINKS/MAX_LINKS_4_EDGE;i++) {
        Neighbor_Nodes[i] = NULL;
    }

    stdhash_construct(&All_Nodes, sizeof(int32), sizeof(Node*), 
		      stdhash_int_equals, stdhash_int_hashcode);

    stdhash_construct(&All_Edges, sizeof(int32), sizeof(State_Chain*), 
		      stdhash_int_equals, stdhash_int_hashcode);
    stdhash_construct(&Changed_Edges, sizeof(int32), sizeof(Changed_Edge*), 
		      stdhash_int_equals, stdhash_int_hashcode);

    stdhash_construct(&All_Groups_by_Node, sizeof(int32), sizeof(State_Chain*), 
		      stdhash_int_equals, stdhash_int_hashcode);
    stdhash_construct(&All_Groups_by_Name, sizeof(int32), sizeof(State_Chain*), 
		      stdhash_int_equals, stdhash_int_hashcode);
    stdhash_construct(&Changed_Group_States, sizeof(int32), sizeof(Group_State*), 
		      stdhash_int_equals, stdhash_int_hashcode);

    stdhash_construct(&Monitor_Loss, sizeof(int32), sizeof(struct Loss_d), 
		      stdhash_int_equals, stdhash_int_hashcode);


    for(i=0; i<MAX_LINKS; i++)
        Links[i] = NULL;

    Create_Node(My_Address, LOCAL_NODE);

    for(i=0;i<Num_Initial_Nodes;i++) {
        Create_Node(Address[i], NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE);
	Create_Link(Address[i], CONTROL_LINK);
    }
}



/***********************************************************/
/* void Create_Node(int32 address, int16 mode)             */
/*                                                         */
/* creates a new node structure                            */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* address: IP address of the node                         */
/* mode:    type of the node                               */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Create_Node(int32 address, int16 mode) {
    int i, nodeid;
    Node *node_p;
    stdhash_it it;
    int32 ip_address = address;

    if((node_p = (Node*) new(TREE_NODE))==NULL)
        Alarm(EXIT, "Create_Node: Cannot allocte node object\n");

    /* Initialize the node structure */

    node_p->address = address;
    node_p->node_id = -1;
    node_p->flags = 0x0;
    node_p->counter = 0;
    node_p->last_time_heard = E_get_time();
    node_p->flags = mode;
    stdhash_construct(&node_p->routes, sizeof(int32), sizeof(Route*),
		      stdhash_int_equals, stdhash_int_hashcode);
    node_p->prev = NULL;
    node_p->next = NULL;

    for(i=0; i<MAX_LINKS_4_EDGE; i++)
        node_p->link[i] = NULL;

    /* Insert the node in the global data structures */
    stdhash_insert( &All_Nodes, &it, &ip_address, &node_p);

    Alarm(DEBUG, "\nNum_Neighbors: %d\n\n", Num_Neighbors);

    if(mode & NEIGHBOR_NODE) {	
	for(nodeid=0; nodeid<MAX_LINKS/MAX_LINKS_4_EDGE; nodeid++) {
	    if(Neighbor_Nodes[nodeid] == NULL)
	        break;
	}

	if(nodeid == MAX_LINKS/MAX_LINKS_4_EDGE)
	    Alarm(EXIT, "Create_Node() No node IDs available; too many neighbors\n");
	if(nodeid+1 > Num_Neighbors)
	    Num_Neighbors = nodeid+1;

	node_p->node_id = nodeid;
	Neighbor_Nodes[nodeid] = node_p;

	Alarm(DEBUG, "Create_Node(): Neighbor %d.%d.%d.%d on nodeid %d; %d\n",
	      IP1(address), IP2(address), IP3(address), IP4(address), 
	      nodeid, Num_Neighbors);


    }
    Alarm(DEBUG, "Create_Node(): Node %d.%d.%d.%d created with mode %d\n",
	  IP1(address), IP2(address), IP3(address), IP4(address), 
	  node_p->flags);
}



/***********************************************************/
/* void Disconnect_Node(int32 address)                     */
/*                                                         */
/* Disconnects a neighbor node                             */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* address: IP address of the node                         */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Disconnect_Node(int32 address) 
{
    stdhash_it it;
    Node *nd;
    Edge *edge;
    int32 address_t;
    int i;
    int16 tmp_nodeid;

    address_t = address;
    stdhash_find(&All_Nodes, &it, &address_t);
    if(stdhash_it_is_end(&it)) {
        Alarm(EXIT, "Disconnect_Node(): Node does not exist: %d.%d.%d.%d\n",
	      IP1(address), IP2(address), IP3(address), IP4(address));
    }
    nd = *((Node **)stdhash_it_val(&it));
    if(!(nd->flags & NEIGHBOR_NODE)) {
        Alarm(EXIT, "Disconnect_Node(): Node alreadyy disconnected: %d.%d.%d.%d\n",
	     IP1(address), IP2(address), IP3(address), IP4(address));
    }

    for(i=0; i<MAX_LINKS_4_EDGE; i++) {
        if(nd->link[i] != NULL) {
	    Destroy_Link(nd->link[i]->link_id);
	}
    }
    
    edge = Destroy_Edge(My_Address, address, 1);
    if(edge != NULL) {
        Add_to_changed_states(&Edge_Prot_Def, My_Address, (State_Data*)edge, NEW_CHANGE);
	Set_Routes();
    }

    if(nd->flags & NEIGHBOR_NODE)
        nd->flags = nd->flags ^ NEIGHBOR_NODE;
    if(nd->flags & CONNECTED_NODE)
         nd->flags = nd->flags ^ CONNECTED_NODE;
    if(nd->flags & NOT_YET_CONNECTED_NODE)
         nd->flags = nd->flags ^ NOT_YET_CONNECTED_NODE;

    nd->flags = nd->flags | REMOTE_NODE;
    
    Alarm(DEBUG, "Disconnect_Node(): node_id %d; Num_Neighbors: %d\n", 
	  nd->node_id, Num_Neighbors);

    tmp_nodeid = nd->node_id;
    Neighbor_Nodes[nd->node_id] = NULL;
    nd->node_id = -1;

    while(Num_Neighbors > 0) {
        if(Neighbor_Nodes[Num_Neighbors - 1] != NULL)
	    break;
	Num_Neighbors--;
    }

    Alarm(DEBUG, "Disconnected node %d.%d.%d.%d\n",
	  IP1(address), IP2(address), IP3(address), IP4(address));
}


/***********************************************************/
/* int Try_Remove_Node(int32 address)                      */
/*                                                         */
/* Garbage collect of orphan nodes (not attached to edges) */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* address: IP address of the node                         */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/*  1 if the node was removed                              */
/*  0 if it wasn't                                         */
/* -1 if error                                             */
/*                                                         */
/***********************************************************/


int Try_Remove_Node(int32 address)
{
    stdhash_it it, node_it, src_it, route_it;
    int flag;
    State_Chain *s_chain;
    Node* nd;
    Route* route;
   

    if(address == My_Address)
	return(0);

    /* See if the  node is the source of an existing edge */
    stdhash_find(&All_Edges, &src_it, &address);
    if(stdhash_it_is_end(&src_it)) {
	/* it's not, so let's see if the node is a destination */
	stdhash_begin(&All_Edges, &src_it);
	
	flag = 0;
	while(!stdhash_it_is_end(&src_it)) {
	    s_chain = *((State_Chain **)stdhash_it_val(&src_it));
	    stdhash_find(&s_chain->states, &it, &address);
	    if(!stdhash_it_is_end(&it)) {
		flag = 1;
		break;
	    }
	    stdhash_it_next(&src_it); 
	}
		    
	if(flag == 0) {
	    /* The node is not a destination either. Delete the node */ 

	    Alarm(PRINT, "Deleting node: %d.%d.%d.%d\n",
		  IP1(address), IP2(address), IP3(address), IP4(address));

	    stdhash_find(&All_Nodes, &node_it, &address);
	    if(stdhash_it_is_end(&node_it)) { 
		Alarm(PRINT, "Try_Remove_Node(): No node structure !\n");
		return(-1);
	    }
			
	    nd = *((Node **)stdhash_it_val(&node_it));
			
	    if(nd->flags & NEIGHBOR_NODE) {
		Alarm(PRINT, "Try_Remove_Node(): Node is a neighbor\n");
		return(-1);
	    }
	    
	    stdhash_begin(&nd->routes, &route_it);
	    while(!stdhash_it_is_end(&route_it)) {
		route = *((Route **)stdhash_it_val(&route_it));	    
		dispose(route);
		stdhash_it_next(&route_it);
	    }
	    stdhash_destruct(&nd->routes);
	    stdhash_erase(&node_it);
	    dispose(nd);
	    return(1);
	}
    }
    return 0;
}
