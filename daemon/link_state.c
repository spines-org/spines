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

#include <stdlib.h>
#include <math.h>

#ifdef ARCH_PC_WIN95
#include <winsock2.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/memory.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "objects.h"
#include "link.h"
#include "node.h"
#include "route.h"
#include "link_state.h"
#include "state_flood.h"
#include "net_types.h"
#include "hello.h"
#include "kernel_routing.h"


/* Global variables */
extern stdhash  All_Edges;
extern stdhash  Changed_Edges;
extern stdhash  All_Nodes;
extern Node*    Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
extern int16    Num_Neighbors;
extern Link*    Links[MAX_LINKS];
extern Prot_Def Edge_Prot_Def;
extern int32    My_Address;
extern int      Route_Weight;
extern int16    KR_Flags;
extern int      Wireless_monitor;

/* Local variables */
static const sp_time zero_timeout        = {     0,      0};

#define MAX_RETR_DELAY 30



/***********************************************************/
/* stdhash* Edge_All_States(void)                          */
/*                                                         */
/* Returns the hash containing all the known edges         */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (stdhash*) Hash with all the edges                      */
/*                                                         */
/***********************************************************/

stdhash* Edge_All_States(void)
{
    return(&All_Edges);
}

/***********************************************************/
/* stdhash* Edge_All_States_by_Dest(void)                  */
/*                                                         */
/* Returns the hash containing all the known edges         */
/*         indexed by destination. Not used.               */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (stdhash*) NULL                                         */
/*                                                         */
/***********************************************************/

stdhash* Edge_All_States_by_Dest(void)
{
    return(NULL);
}


/***********************************************************/
/* stdhash* Edge_Changed_States(void)                      */
/*                                                         */
/* Returns the hash containing the buffer of changed edges */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (stdhash*) Hash with all the edges                      */
/*                                                         */
/***********************************************************/

stdhash* Edge_Changed_States(void)
{
    return(&Changed_Edges);
}


/***********************************************************/
/* int Edge_State_type(void)                               */
/*                                                         */
/* Returns the packet header type for link-state msgs.     */
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

int Edge_State_type(void)
{
    return(LINK_STATE_TYPE);
}


/***********************************************************/
/* int Edge_State_header_size(void)                        */
/*                                                         */
/* Returns the size of the link_state header               */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) size of the link_state header                     */
/*                                                         */
/***********************************************************/

int Edge_State_header_size(void)
{
    return(sizeof(link_state_packet));
}


/***********************************************************/
/* int Edge_Cell_packet_size(void)                         */
/*                                                         */
/* Returns the size of the link_state cell                 */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) size of the link_state cell                       */
/*                                                         */
/***********************************************************/

int Edge_Cell_packet_size(void)
{
    return(sizeof(edge_cell_packet));
}



/***********************************************************/
/* int Edge_Is_route_change(void)                          */
/*                                                         */
/* Returns true, link_state changes routing...             */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) 1 ; true...                                       */
/*                                                         */
/***********************************************************/

int Edge_Is_route_change(void) 
{
    return(1);
}




/***********************************************************/
/* int Edge_Is_state_relevant(void* state)                 */
/*                                                         */
/* Returns true if the edge is not deleted, false otherwise*/
/* so that the edge will not be resent, and eventually     */
/* will be arbage-collected                                */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* state: pointer to the edge structure                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) 1 if edge is up                                   */
/*       0 otherwise                                       */
/*                                                         */
/***********************************************************/

int Edge_Is_state_relevant(void *state)
{
    Edge *edge;

    edge = (Edge*)state;
    if(edge->cost >= 0) {
	return(1);
    }
    else {
	return(0);
    }
}





/***********************************************************/
/* int Edge_Set_state_header(void *state, char *pos)       */
/*                                                         */
/* Sets the link_state_packet header additional fields     */
/* (nothing for now)                                       */
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

int Edge_Set_state_header(void *state, char *pos)
{
    /* Nothing for now... */
    return(0);
}




/***********************************************************/
/* int Edge_Set_state_cell(void *state, char *pos)         */
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

int Edge_Set_state_cell(void *state, char *pos)
{
    /* Nothing for now... */
    return(0);
}




/***********************************************************/
/* int Edge_Process_state_header(char *pos);               */
/*                                                         */
/* Process the link_state_packet header additional fields  */
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

int Edge_Process_state_header(char *pos, int32 type)
{
    /* Nothing for now... */
    /* Just flip the header endianess if necessary */
    
    link_state_packet *lk_st_pkt;
    
    lk_st_pkt = (link_state_packet*)pos;

    if(!Same_endian(type)) {
	lk_st_pkt->source = Flip_int32(lk_st_pkt->source);
	lk_st_pkt->num_edges = Flip_int16(lk_st_pkt->num_edges);
	lk_st_pkt->src_data = Flip_int16(lk_st_pkt->src_data);
    }

    return(0);
}

/***********************************************************/
/* int Edge_Destroy_State_Data(void *state)                */
/*                                                         */
/* Destroys specific info from the edge structure          */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* void* state                                             */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if ok                                          */
/*       -1 if not                                         */
/*                                                         */
/***********************************************************/

int Edge_Destroy_State_Data(void *state)
{
    return(1);
}


/***********************************************************/
/* void* Edge_Process_state_cell(int32 source, char *pos)  */
/*                                                         */
/* Processes the link_state cell additional fields         */
/* (cost, maybe loss rate, etc.)                           */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* source: Source of the edge                              */
/* pos: pointer to the begining of the cell                */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (void*) pointer to the edge processed (new or old)      */
/*                                                         */
/***********************************************************/

void* Edge_Process_state_cell(int32 source, char *pos)
{
    Edge *edge;
    Node *nd_source, *nd_dest;
    Node *nd = NULL;
    edge_cell_packet *edge_cell;
    stdit it;
    int16 nodeid, linkid;
    int flag_tmp;

    /* Check if we knew about the source node */
    stdhash_find(&All_Nodes, &it, &source);
    if(stdhash_is_end(&All_Nodes, &it)) { /* I had no idea about this node */
	Create_Node(source, REMOTE_NODE);
	stdhash_find(&All_Nodes, &it, &source);
    }
    nd_source = *((Node **)stdhash_it_val(&it));
  
    edge_cell = (edge_cell_packet*)pos; 

    /* Check if we knew about the destination node */
    stdhash_find(&All_Nodes, &it, &edge_cell->dest);
    if(stdhash_is_end(&All_Nodes, &it)) { /* I had no idea about this node */
	Create_Node(edge_cell->dest, REMOTE_NODE);
	stdhash_find(&All_Nodes, &it, &edge_cell->dest);
    }
    nd_dest = *((Node **)stdhash_it_val(&it));


    if((edge_cell->cost < 0)&&(nd_source->address == My_Address)) {
	/* somebody tells me that an edge of mine is deleted. */
	Alarm(PRINT, "Hey, this is my edge !!!\n");
#if 0
	edge = (Edge*)Find_State(&All_Edges, nd_source->address, 
			 nd_dest->address);
	return((void*)edge);
#endif
    }

    /* Did I know about this edge ? */
    if((edge = (Edge*)Find_State(&All_Edges, nd_source->address, 
			 nd_dest->address)) == NULL) {
	/* This is about an edge that I didn't know about until 
	 * now... deleted or not. */ 
	
	edge = Create_Overlay_Edge(nd_source->address, nd_dest->address);
	edge->timestamp_sec = 0;
	edge->timestamp_usec = 0;
    }

    Alarm(DEBUG, "\nGot edge: %d.%d.%d.%d -> %d.%d.%d.%d\n",
	  IP1(edge->source->address), IP2(edge->source->address), 
	  IP3(edge->source->address), IP4(edge->source->address), 
	  IP1(edge->dest->address), IP2(edge->dest->address), 
	  IP3(edge->dest->address), IP4(edge->dest->address));
    
    
    Alarm(DEBUG, "Got: %d : %d ||| mine: %d : %d ### got cost: %d | %d\n", 
	  edge_cell->timestamp_sec, edge_cell->timestamp_usec,
	  edge->timestamp_sec, edge->timestamp_usec,
	  edge_cell->cost, edge->cost);

    
    /* Check if the edge is mine, and if so, check if I have a link
     * with this edge. It is possible that smbd. just told me about 
     * a new edge of mine, I created it, I don't have a link with 
     * it yet */
    if(edge_cell->cost > 0) {
	flag_tmp = 0;
	if(nd_source->address == My_Address) {
	    nd = nd_dest;
	    flag_tmp = 1;
	}
	if(nd_dest->address == My_Address) {
	    nd = nd_source;
	    flag_tmp = 1;
	}
	
	if(flag_tmp == 1) {
	    if(nd->link[CONTROL_LINK] == NULL) {
		nd->flags = NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE;	      	  
		nd->counter = 0;
		
		for(nodeid=0; nodeid< MAX_LINKS/MAX_LINKS_4_EDGE; nodeid++) {
		    if(Neighbor_Nodes[nodeid] == NULL)
			break;
		}
		
		if(nodeid == MAX_LINKS/MAX_LINKS_4_EDGE)
		    Alarm(EXIT, "No node IDs available; too many neighbors\n");
		if(nodeid+1 > Num_Neighbors)
		    Num_Neighbors = nodeid+1;
		
		nd->node_id = nodeid;
		Neighbor_Nodes[nodeid] = nd;
		
		linkid = Create_Link(nd->address, CONTROL_LINK);
		
		E_queue(Send_Hello, linkid, NULL, zero_timeout);
	    }
	}
    }

    /* Update edge structure here... */
    Alarm(PRINT, "Updating edge: %d.%d.%d.%d -> %d.%d.%d.%d; %d -> %d\n",
	  IP1(edge->source->address), IP2(edge->source->address), 
	  IP3(edge->source->address), IP4(edge->source->address), 
	  IP1(edge->dest->address), IP2(edge->dest->address), 
	  IP3(edge->dest->address), IP4(edge->dest->address),
	  edge->cost, edge_cell->cost);
    
    edge->timestamp_sec = edge_cell->timestamp_sec;
    edge->timestamp_usec = edge_cell->timestamp_usec;
    edge->age = edge_cell->age;
    edge->cost = edge_cell->cost;
    
    return((void*)edge);
}





/***********************************************************/
/* Edge* Create_Overlay_Edge(int32 source, int32 dest)     */
/*                                                         */
/* Creates an edge                                         */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* source: IP address of the source                        */
/* dest:   IP address of the destination                   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Edge*) a pointer to the Edge structure                 */
/*                                                         */
/***********************************************************/

Edge* Create_Overlay_Edge(int32 source, int32 dest) {
    stdit it;
    Edge *edge;
    Node *src_nd;
    Node *dst_nd;
    State_Chain *s_chain;
    sp_time now;

 
    Alarm(DEBUG, "Create edge: %d.%d.%d.%d -> %d.%d.%d.%d\n", 
	  IP1(source), IP2(source), IP3(source), IP4(source), 
	  IP1(dest), IP2(dest), IP3(dest), IP4(dest));
    
    now = E_get_time();
    if(Find_State(&All_Edges, source, dest) != NULL)
        Alarm(EXIT, "Create_Overlay_Edge(): Edge already exists\n");

    /* Find the src and dest node structures */
    stdhash_find(&All_Nodes, &it, &source);
    if(stdhash_is_end(&All_Nodes, &it))
        Alarm(EXIT, "Create_Overlay_Edge(): Non existent source\n");

    src_nd = *((Node **)stdhash_it_val(&it));

    stdhash_find(&All_Nodes, &it, &dest);
    if(stdhash_is_end(&All_Nodes, &it))
        Alarm(EXIT, "Create_Overlay_Edge(): Non existent destination\n");

    dst_nd = *((Node **)stdhash_it_val(&it));

    /* Create the edge structure */
    if((edge = (Edge*) new(OVERLAY_EDGE))==NULL)
	Alarm(EXIT, "Create_Overlay_Edge: Cannot allocte edge object\n");
    if(My_Address == source) {
	edge->timestamp_sec = (int32)now.sec;
	edge->timestamp_usec = (int32)now.usec;
    }
    else {
	edge->timestamp_sec = 0;
	edge->timestamp_usec = 0;
    }
    
    /* Initialize the edge structure */

    edge->my_timestamp_sec = (int32)now.sec;
    edge->my_timestamp_usec = (int32)now.usec;	    
    edge->source_addr = source;
    edge->dest_addr = dest;
    edge->source = src_nd;
    edge->dest = dst_nd;
    edge->age = 0;
    if((Route_Weight == LATENCY_ROUTE)||(Route_Weight == AVERAGE_ROUTE)) {
    	edge->cost = 4;
    }
    else {
	edge->cost = 1;
    }
    edge->flags = REMOTE_EDGE;


    /* Insert the edge in the global data structures */

    stdhash_find(&All_Edges, &it, &source);
    if(stdhash_is_end(&All_Edges, &it)) {
	if((s_chain = (State_Chain*) new(STATE_CHAIN))==NULL)
	    Alarm(EXIT, "Create_Overlay_Edge: Cannot allocte object\n");
	s_chain->address = source;
	stdhash_construct(&s_chain->states, sizeof(int32), sizeof(Edge*), 
			  NULL, NULL, 0);
	stdhash_insert(&All_Edges, &it, &source, &s_chain);
	stdhash_find(&All_Edges, &it, &source);
    }
    
    s_chain = *((State_Chain **)stdhash_it_val(&it));
    stdhash_insert(&s_chain->states, &it, &dest, &edge);
    
    return edge;
}





/***********************************************************/
/* Edge* Destroy_Edge(int32 source, int32 dest,            */
/*                   int local_call)                       */
/*                                                         */
/* Actually it marks an edge as not available (Deleted),   */
/* and updates some counters                               */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* source: IP address of the source                        */
/* dest:   IP address of the destination                   */
/* local_call: 1 if this is a local decision               */
/*             0 if the decision comes from another node   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Edge*) a pointer to the deleted Edge structure         */
/*         or NULL if the edge is does not exist           */
/*                                                         */
/***********************************************************/

Edge* Destroy_Edge(int32 source, int32 dest, int local_call)
{
    Edge *edge;
    sp_time now;
    
    now = E_get_time();
    Alarm(DEBUG, "Destroy edge: %d.%d.%d.%d -> %d.%d.%d.%d; local: %d\n", 
	  IP1(source), IP2(source), IP3(source), IP4(source), 
	  IP1(dest), IP2(dest), IP3(dest), IP4(dest), local_call);


    if((edge = (Edge*)Find_State(&All_Edges, source, dest)) == NULL) {
	Alarm(DEBUG, "Destroy_Edge(): Edge non existent\n");
	return NULL;
    }

    if(edge->cost < 0) {
	Alarm(DEBUG, "Destroy_Edge(): Edge already deleted\n");
	return NULL;
    }
        
    if(source == My_Address) {
	if(local_call != 1) {
	    if(edge->dest->link[0] != NULL)
		Alarm(EXIT, "Destroy_edge(): Somebody told me to kill my link !\n");
	}
	edge->cost = -1;
	edge->timestamp_usec++;
	if(edge->timestamp_usec >= 1000000) {
	    edge->timestamp_usec = 0;
	    edge->timestamp_sec++;
	}
	edge->age = 0;
    }
    return edge;
}



/***********************************************************/
/* int Edge_Update_Cost(int linkid, int mode)              */
/*                                                         */
/* Updates the cost of a local edge based on the new       */
/* characteristics of the corresponding link               */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid: ID of the control link of the local edge        */
/* mode:   What changed (LATENCY_ROUTE, LOSSRATE_ROUTE)    */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)   1 if the cost was updated successfully          */
/*        -1 if not                                        */
/*                                                         */
/***********************************************************/

int Edge_Update_Cost(int linkid, int mode)
{
    Link *lk;
    Control_Data *c_data;
    Edge *edge;
    float tmp, tmp1, tmp2, cost;
    sp_time now;

    now = E_get_time();

    if(mode != Route_Weight) {
	return(1);
    }
    
    lk = Links[linkid];
    if(lk == NULL) {
	Alarm(PRINT, "Edge_Update_Cost(): No link...\n");
	return(-1);
    }
    
    edge = (Edge*)Find_State(&All_Edges, My_Address, lk->other_side_node->address);
    if(edge == NULL) {
	Alarm(PRINT, "Edge_Update_Cost(): No edge...\n");
	return(-1);
    }
        
    c_data = (Control_Data*)lk->prot_data;
    if(c_data == NULL) {
	Alarm(PRINT, "Edge_Update_Cost(): No edge...\n");
	return(-1);
    }
    if(Route_Weight == LATENCY_ROUTE) {
	tmp = (float)c_data->rtt;

	/* One way delay */
	tmp = (float)(tmp/2.0);

	if(tmp < 1) {
	    tmp = 1;
	}
	tmp += 4.0;
	if(tmp < 300) {
	    cost = tmp;
	}
        else {
	    cost = 300.0;
	}
	
	if(abs((int)(edge->cost - cost)) < (int)(0.1*edge->cost)) {
	    return(-1);
	}

	edge->cost = (short)cost;

	edge->timestamp_usec++;
	if(edge->timestamp_usec >= 1000000) {
	    edge->timestamp_usec = 0;
	    edge->timestamp_sec++;
	}
	Add_to_changed_states(&Edge_Prot_Def, My_Address, (State_Data*)edge, NEW_CHANGE);
	Set_Routes(0, NULL);
    }
    else if(Route_Weight == LOSSRATE_ROUTE) {
	tmp = (float)log(1-c_data->est_loss_rate);
	tmp *= 10000;
	if(1-tmp < 30000) {
	    edge->cost = (short)(1-tmp);
	}
	else {
	    edge->cost = 30000;
	}	
	edge->timestamp_usec++;
	if(edge->timestamp_usec >= 1000000) {
	    edge->timestamp_usec = 0;
	    edge->timestamp_sec++;
	}
	Add_to_changed_states(&Edge_Prot_Def, My_Address, (State_Data*)edge, NEW_CHANGE);
	Set_Routes(0, NULL);
    }
    else if(Route_Weight == AVERAGE_ROUTE) {	
	tmp = (float)(c_data->rtt);
	/* one way delay */
	tmp = (float)(tmp/2.0);   

	if(c_data->est_loss_rate > 0.0) {
	    /* 2*p^2 * max_latency (lost even after recovery) */
	    tmp1 = (float)(2.0*c_data->est_loss_rate*c_data->est_loss_rate*100.0);
	    
	    /* (p - 2*p^2)(3*delay + delta) */
	    tmp2 = (float)((tmp*3.0 + 10.0)*(c_data->est_loss_rate - 2.0*c_data->est_loss_rate*c_data->est_loss_rate));
	    
	    /* (1-p)*delay + (p - 2*p^2)(3*delay + delta) + 2*p^2 * max_latency */
	    tmp = (float)(tmp*(1.0-c_data->est_loss_rate) + tmp1 + tmp2);
	    
	}
	
	if(tmp < 1) {
	    tmp = 1;
	}
	tmp += 4.0;
	if(tmp < 300.0) {
	    cost = tmp;
	}
        else {
	    cost = 300.0;
	}
	/*
	 *if(edge->cost > MAX_RETR_DELAY) {
	 *   if(cost > MAX_RETR_DELAY - 5) {
	 *	cost = cost + MAX_RETR_DELAY;
	 *   }
	 *}
	 *else {
	 *   if(cost > MAX_RETR_DELAY) {
	 *	cost = cost + MAX_RETR_DELAY;
	 *   }
	 *}
	 */

	if((edge->cost > cost)&&(edge->cost - cost < edge->cost*0.15)) {
	    return(-1);
	}
	else if((edge->cost <= cost)&&(cost - edge->cost < edge->cost*0.15)) {
	    return(-1);
	}

	
       	Alarm(DEBUG, "@\tupdate\t%d -- %d: delay: %5.3f loss: %5.3f; new_cost: %5.3f; old_cost: %d\n",
	      now.sec, linkid, (c_data->rtt/2.0), c_data->est_loss_rate, cost, edge->cost);
	

	edge->cost = (short)(cost+0.5);


	edge->timestamp_usec++;
	if(edge->timestamp_usec >= 1000000) {
	    edge->timestamp_usec = 0;
	    edge->timestamp_sec++;
	}
	Add_to_changed_states(&Edge_Prot_Def, My_Address, (State_Data*)edge, NEW_CHANGE);
	Set_Routes(0, NULL);
    }

    Alarm(DEBUG, "Edge_Update_Cost()\n");

    return(1);
}


/* For debugging or logging purpose. 
 * The snapshot will write the current spines route every
 * print_timeout, overwriting the previous file.  So the 
 * latest state of Spines can be found in SNAPSHOT_FILE     
 */
#define PRINT_EDGES 1
#define PRINT_KERNEL_ROUTES 0
#define PRINT_WIRELESS_STATUS 1
#define SNAPSHOT 1
#define SNAPSHOT_FILE "/tmp/spines.snapshot"

void Print_Edges(int dummy_int, void* dummy) 
{
    sp_time print_timeout = {   20,    0};
    FILE *fp = NULL;
    char line[256];
    char file_name[50];

    sprintf(file_name, "%s", SNAPSHOT_FILE);

    if (SNAPSHOT) { 
	fp = fopen(file_name, "w"); 
	if (fp == NULL) { 
	    perror("Could not open spines snapshot file\n");
	    Alarm(PRINT,"\nWill continue without attempting to write to snapshot file\n");
	} else { 
#ifndef ARCH_PC_WIN95
		/* we dont know what to set it on win32 systems */
	    chmod(file_name, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
#endif
	    fprintf(fp, "\n\n"); 
	}
    }

    if (PRINT_EDGES) { 
	stdit it, c_it; 
	Edge *edge; 
	State_Chain *s_chain; 
	
	sprintf(line, "\n\nAvailable edges:\n"); 
	Alarm(PRINT, "%s", line); 
	if (fp != NULL) fprintf(fp, "%s", line); 
	stdhash_begin(&All_Edges, &it); 
	while(!stdhash_is_end(&All_Edges, &it)) { 
	    s_chain = *((State_Chain **)stdhash_it_val(&it)); 
	    stdhash_begin(&s_chain->states, &c_it); 
	    while(!stdhash_is_end(&s_chain->states, &c_it)) { 
		edge = *((Edge **)stdhash_it_val(&c_it)); 
		sprintf(line, "\t\t%d.%d.%d.%d -> %d.%d.%d.%d :: %d | %d:%d\n", 
			IP1(edge->source->address), IP2(edge->source->address), 
			IP3(edge->source->address), IP4(edge->source->address), 
			IP1(edge->dest->address), IP2(edge->dest->address), 
			IP3(edge->dest->address), IP4(edge->dest->address), 
			edge->cost, edge->timestamp_sec, edge->timestamp_usec); 
		stdhash_it_next(&c_it); 
		Alarm(PRINT, "%s", line); 
		if (fp != NULL) fprintf(fp, "%s", line); 
	    } 
	    stdhash_it_next(&it); 
	} 
    }

    sprintf(line, "\n\n");
    Alarm(PRINT, "%s", line);
    if (fp != NULL) fprintf(fp, "%s", line);

    Print_Routes(fp);

#ifdef SPINES_WIRELESS
    if (Wireless_monitor && PRINT_WIRELESS_STATUS) {
        Wireless_Print_Status(fp);
    }
#endif

#ifndef ARCH_PC_WIN95
    if (KR_Flags && PRINT_KERNEL_ROUTES) {
        KR_Print_Routes(fp);
    }
#endif

    if (fp != NULL) { 
	fclose(fp);
	fp = NULL;
    }

    E_queue(Print_Edges, 0, NULL, print_timeout);
}

