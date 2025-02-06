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


#include <string.h>
#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/memory.h"
#include "util/data_link.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "objects.h"
#include "node.h"
#include "link.h"
#include "protocol.h"
#include "network.h"
#include "reliable_link.h"
#include "link_state.h"
#include "hello.h"
#include "route.h"
#include "udp.h"
#include "session.h"

/* Extern variables, as defined in on.c */
extern stdhash   All_Nodes;
extern stdhash   All_Edges;
extern stdhash   Changed_Edges;
extern Link*     Links[MAX_LINKS];
extern channel   Local_Send_Channels[MAX_LINKS_4_EDGE];
extern int32     My_Address;
extern int16     Port;
extern int16     Link_Sessions_Blocked_On;



/***********************************************************/
/* int16 Create_Link(int32 address, int16 mode)            */
/*                                                         */
/* Creates a link between the current node and some        */
/* neighbour                                               */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* address: the address of the neighbour                   */
/* mode: mode of the link (CONTROL, UDP, etc.)             */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int16) the ID of the link in the global Link array     */
/*                                                         */
/***********************************************************/

int16 Create_Link(int32 address, int16 mode) {
    stdhash_it it;
    Node       *nd;
    Link       *lk;
    Edge       *edge; 
    UDP_Data      *u_data;
    Control_Data  *c_data;
    Reliable_Data *r_data;
    int32      ip_address = address;
    int        i; 
    int16      linkid;


    /* Find an empty spot in the Links array */
    for(linkid=0; linkid<MAX_LINKS && Links[linkid] != NULL; linkid++);
    if(linkid == MAX_LINKS)
        Alarm(EXIT, "Create_Link() No link IDs available; too many open links\n");

    /* Find the neighbour node structure */
    stdhash_find(&All_Nodes, &it, &ip_address);
    if(!stdhash_it_is_end(&it)) {
        nd = *((Node **)stdhash_it_val(&it));
	if(!(nd->flags & NEIGHBOR_NODE))
	    Alarm(EXIT, "Create_Link: Non neighbor node: %X; %d\n", 
		  nd->address, nd->flags);
      
	/* Create the link structure */
	if((lk = (Link*) new(DIRECT_LINK))==NULL)
	    Alarm(EXIT, "Create_Link: Cannot allocte link object\n");

	/* Initialize the link structure */
	lk->other_side_node = nd;
	lk->link_node_id = mode;
	lk->link_id = linkid;
	lk->chan = Local_Send_Channels[mode];
	lk->port = Port + mode;
	lk->r_data = NULL;
	lk->prot_data = NULL;

	/* Update the global Links structure */
	Links[linkid] = lk;
	
	/* Update the node structure */
	nd->link[mode] = lk;

	/* Create the reliable_data structure */
	if((mode == CONTROL_LINK)||
	   (mode == RELIABLE_UDP_LINK)) {  
	    if((r_data = (Reliable_Data*) new(RELIABLE_DATA))==NULL)
	        Alarm(EXIT, "Create_Link: Cannot allocte reliable_data object\n");
	    r_data->flags = UNAVAILABLE_LINK;
	    r_data->seq_no = 0;
	    stdcarr_construct(&(r_data->msg_buff), sizeof(Buffer_Cell*));
	    for(i=0;i<MAX_WINDOW;i++) {
	        r_data->window[i].buff = NULL;
	        r_data->window[i].data_len = 0;
		r_data->recv_window[i].flag = EMPTY_CELL;
	    }
	    r_data->head = 0;
	    r_data->tail = 0;
	    r_data->recv_head = 0;
	    r_data->recv_tail = 0;
	    r_data->nack_buff = NULL;
	    r_data->nack_len = 0;
	    r_data->scheduled_ack = 0;
	    r_data->scheduled_timeout = 0;
	    r_data->timeout_multiply = 1;
	    r_data->rtt = 0;
	    r_data->congestion_flag = 0;
	    r_data->connect_state = AVAILABLE_LINK;

	    /* Congestion control */
	    r_data->window_size = 1.0; 
	    r_data->max_window = MAX_CG_WINDOW;
	    r_data->ssthresh = MAX_CG_WINDOW;
	    
	    Alarm(DEBUG, "created reliable link: %d\n", linkid);

	    lk->r_data = r_data;
	}

	/* Create the control_data structure */
	if(mode == CONTROL_LINK) {
	    if((c_data = (Control_Data*) new(CONTROL_DATA))==NULL)
	        Alarm(EXIT, "Create_Link: Cannot allocte ctrl_data object\n");
	    c_data->hello_seq = 0;
	    c_data->other_side_hello_seq = 0;
	    c_data->diff_time = 0;
	    c_data->rtt = 0;
	    lk->prot_data = c_data;
	   
	    /* If this is the first link to this node,
	     * create an overlay edge */
	    if((edge = Find_Edge(&All_Edges, My_Address, 
				 nd->address, 0)) == NULL) {
	        edge = Create_Overlay_Edge(My_Address, nd->address);
	    }
	    else if(edge->cost < 0){
		edge->cost = 0;
		edge->timestamp_usec++;
		if(edge->timestamp_usec >= 1000000) {
		    edge->timestamp_usec = 0;
		    edge->timestamp_sec++;
		}

	    }
	    edge->flags = CONNECTED_EDGE;
	    Add_to_changed_edges(My_Address, edge, NEW_CHANGE);
	    Set_Routes();
	}

	/* Create the udp_data structure */
	if(mode == UDP_LINK) {
	    if((u_data = (UDP_Data*) new(UDP_DATA))==NULL)
	        Alarm(EXIT, "Create_Link: Cannot allocte udp_data object\n");

	    stdcarr_construct(&(u_data->udp_ses_buff), sizeof(UDP_Cell*));
	    stdcarr_construct(&(u_data->udp_net_buff), sizeof(UDP_Cell*));
	    u_data->block_flag = 0;
	    u_data->my_seq_no = 0;
	    u_data->other_side_seq_no = 0;
	    u_data->last_seq_noloss = 0;
	    u_data->no_of_rounds_wo_loss = 0;
	    lk->prot_data = u_data;
	    Alarm(DEBUG, "Created UDP link\n");
	}
    }
    else
        Alarm(EXIT, "Create_Link Could not find %X in node data structures\n", 
			  address);

    return linkid;
}



/***********************************************************/
/* void Destroy_Link(int16 linkid)                         */
/*                                                         */
/* Destroys a link between the current node and some       */
/* neighbour                                               */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid: the ID of the link in the global Link array     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Destroy_Link(int16 linkid)
{
    Link *link_obj;
    int32u i;
    UDP_Data      *u_data;
    Control_Data  *c_data;
    Reliable_Data *r_data;
    stdcarr_it it;
    Buffer_Cell *buf_cell;
    char* msg;

    link_obj = Links[linkid];

    /* Take care about the reliability stuff */
    if(link_obj->r_data != NULL) {
	r_data = link_obj->r_data;

	/* Remove data from the window */
	for(i=r_data->tail; i<=r_data->head; i++) {
	    if(r_data->window[i%MAX_WINDOW].buff != NULL) {
		dec_ref_cnt(r_data->window[i%MAX_WINDOW].buff);
	    }
	    r_data->window[i%MAX_WINDOW].buff = NULL;
	}

	/* Remove data from the queue */
	while(!stdcarr_empty(&(r_data->msg_buff))) {
	    stdcarr_begin(&(r_data->msg_buff), &it);
	    
	    buf_cell = *((Buffer_Cell **)stdcarr_it_val(&it));
	    msg = buf_cell->buff;
	    dec_ref_cnt(msg);
	    dispose(buf_cell);
	    stdcarr_pop_front(&(r_data->msg_buff));
	}
	stdcarr_destruct(&(r_data->msg_buff));

	if(r_data->nack_buff != NULL) {
	    E_dequeue(Send_Nack_Retransm, (int)linkid, NULL);
	    dispose(r_data->nack_buff);
	    r_data->nack_len = 0;
	}

	if(r_data->scheduled_timeout == 1) {
	    E_dequeue(Reliable_timeout, (int)linkid, NULL);
	}
	r_data->scheduled_timeout = 0;

	if(r_data->scheduled_ack == 1) {
	    E_dequeue(Send_Ack, (int)linkid, NULL);
	}
	r_data->scheduled_ack = 0;

	E_dequeue(Try_to_Send, (int)linkid, NULL);

	if(link_obj->link_node_id == CONTROL_LINK) {
	    E_dequeue(Send_Hello, (int)linkid, NULL);
	    E_dequeue(Net_Send_Link_State_All, (int)linkid, NULL);
	}
	dispose(r_data);

	if(Link_Sessions_Blocked_On == linkid) {
	    Resume_All_Sessions();
	    Link_Sessions_Blocked_On = -1;
	}
    }

    /* Protocol data */

    if(link_obj->prot_data != NULL) {
        if(link_obj->link_node_id == CONTROL_LINK) {
	    c_data = (Control_Data*)link_obj->prot_data;
	    dispose(c_data);
	}
        if(link_obj->link_node_id == UDP_LINK) {
	    u_data = (UDP_Data*)link_obj->prot_data;


#if 0
	    /* Remove data from the queues. THIS IS NOT IMPLEMENTED YET */

	    /* Network buffer */
	    while(!stdcarr_empty(&(u_data->udp_net_buff))) {
		stdcarr_begin(&(u_data->udp_net_buff), &it);
		
		udp_cell = *((UDP_Cell **)stdcarr_it_val(&it));
		msg = udp_cell->buff;
		dec_ref_cnt(msg);
		dispose(udp_cell);
		stdcarr_pop_front(&(u_data->udp_net_buff));
	    }

	    /* Session buffer */
	    while(!stdcarr_empty(&(u_data->udp_ses_buff))) {
		stdcarr_begin(&(u_data->udp_ses_buff), &it);
		
		udp_cell = *((UDP_Cell **)stdcarr_it_val(&it));
		msg = udp_cell->buff;
		dec_ref_cnt(msg);
		dispose(udp_cell);
		stdcarr_pop_front(&(u_data->udp_ses_buff));
	    }
#endif


	    stdcarr_destruct(&(u_data->udp_net_buff));
	    stdcarr_destruct(&(u_data->udp_ses_buff));

	    dispose(u_data);
	    Alarm(DEBUG, "Destroyed UDP link\n");
	}
    }

    if(link_obj->other_side_node->link[link_obj->link_node_id] != NULL) {
	Alarm(DEBUG, "Good case !!!\n");
	link_obj->other_side_node->link[link_obj->link_node_id] = NULL;
    }
    else {
	Alarm(DEBUG, "Link is dead already !!!\n");
    }
    dispose(link_obj);

    Links[linkid] = NULL;
}




/***********************************************************/
/* Edge* Destroy_Edge(int32 source, int32 dest,            */
/*                   int local_call)                       */
/*                                                         */
/* Actually it marks an edge as not available (Deleted),   */
/* moves its pointers around from available to deleted     */
/* structures, and updates some counters                   */
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
    stdhash_it it;
    int32 src;
    int32 dst;
    Edge *edge;
    sp_time now;
    
    src = source; dst = dest;
    
    now = E_get_time();
    Alarm(DEBUG, "Destroy edge: %d.%d.%d.%d -> %d.%d.%d.%d; local: %d\n", 
	  IP1(src), IP2(src), IP3(src), IP4(src), 
	  IP1(dst), IP2(dst), IP3(dst), IP4(dst), local_call);

    stdhash_find(&All_Edges, &it, &src);
    if(stdhash_it_is_end(&it)) {
        Alarm(DEBUG, "Destroy_Edge(): Edge non existent\n");
	return NULL;
    }
    edge = *((Edge **)stdhash_it_val(&it));

    if(edge->dest->address == dst) {
	if(edge->cost < 0) {
	    Alarm(DEBUG, "Destroy_Edge(): Edge already deleted\n");
	    return NULL;
	}
        
	if(src == My_Address) {
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
	}
        return edge;
    }
    stdhash_it_keyed_next(&it); 
    while(!stdhash_it_is_end(&it)) {
        edge = *((Edge **)stdhash_it_val(&it));
	if(edge->dest->address == dst) {
	    if(edge->cost < 0) {
		Alarm(DEBUG, "Destroy_Edge(): Edge already deleted\n");
		return NULL;
	    }
	    
	    if(src == My_Address) {
	        if(local_call != 1) {
		    if(edge->dest->link[0] != NULL)
		        Alarm(EXIT, "Destroy_edge(): Somebody told me to kill my link\n");
		}
		edge->cost = -1;
		edge->timestamp_usec++;
		if(edge->timestamp_usec >= 1000000) {
		    edge->timestamp_usec = 0;
		    edge->timestamp_sec++;
		}
	    }
	    return edge;
	}
	stdhash_it_keyed_next(&it); 
    }
    Alarm(DEBUG, "Destroy_Edge(): Edge non existent ...shouldn't be a problem\n");
    return NULL;
}



/***********************************************************/
/* Edge* Create_Overlay_Edge(int32 source, int32 dest)     */
/*                                                         */
/* Creates an edge if necessary or recovers it from the    */
/* deleted data stucture                                   */
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
    stdhash_it it;
    Edge *edge;
    Node *src_nd;
    Node *dst_nd;
    sp_time now;
 
    Alarm(DEBUG, "Create edge: %d.%d.%d.%d -> %d.%d.%d.%d\n", 
	  IP1(source), IP2(source), IP3(source), IP4(source), 
	  IP1(dest), IP2(dest), IP3(dest), IP4(dest));
    
    now = E_get_time();
    if(Find_Edge(&All_Edges, source, dest, 0) != NULL)
        Alarm(EXIT, "Create_Overlay_Edge(): Edge already exists\n");

    /* Find the src and dest node structures */
    stdhash_find(&All_Nodes, &it, &source);
    if(stdhash_it_is_end(&it))
        Alarm(EXIT, "Create_Overlay_Edge(): Non existent source\n");

    src_nd = *((Node **)stdhash_it_val(&it));

    stdhash_find(&All_Nodes, &it, &dest);
    if(stdhash_it_is_end(&it))
        Alarm(EXIT, "Create_Overlay_Edge(): Non existent destination\n");

    dst_nd = *((Node **)stdhash_it_val(&it));

    /* Is this a known edge ? */
    if((edge = Find_Edge(&All_Edges, source, dest, 0)) == NULL) {    
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

	/* Insert the edge in the global data structures */
	stdhash_insert(&All_Edges, &it, &src_nd->address, &edge);
    }
    else {
	if(My_Address == source) {
	    edge->timestamp_usec++;
	    if(edge->timestamp_usec >= 1000000) {
		edge->timestamp_usec = 0;
		edge->timestamp_sec++;
	    }
	}
	else {
	    edge->timestamp_sec = 0;
	    edge->timestamp_usec = 0;
	}
	
    }

    edge->my_timestamp_sec = (int32)now.sec;
    edge->my_timestamp_usec = (int32)now.usec;	
    
    /* Initialize the edge structure */

    edge->source = src_nd;
    edge->dest = dst_nd;
    edge->cost = 0;
    edge->flags = REMOTE_EDGE;
    
    return edge;
}



/***********************************************************/
/* Edge* Find_Edge(stdhash *hash_struct, int32 source,     */
/*                 int32 dest, int del)                    */
/*                                                         */
/* Finds an edge in a hash structure                       */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* hash:   The hash structure to look into                 */
/* source: IP address of the source                        */
/* dest:   IP address of the destination                   */
/* del:    if 1, erase the edge from the hash              */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Edge*) a pointer to the Edge structure, or NULL        */
/* if the edge does not exist                              */
/*                                                         */
/***********************************************************/

Edge* Find_Edge(stdhash *hash_struct, int32 source, int32 dest, int del) 
{
    stdhash_it it;
    int32 src;
    int32 dst;
    Edge *edge;
    
    src = source; dst = dest;
    
    stdhash_find(hash_struct, &it, &src);
    if(stdhash_it_is_end(&it))
        return NULL;

    edge = *((Edge **)stdhash_it_val(&it));
    if(edge->dest->address == dst) {
        if(del == 1)
	    stdhash_erase(&it);	  
        return edge;
    }
    stdhash_it_keyed_next(&it); 
    while(!stdhash_it_is_end(&it)) {
        edge = *((Edge **)stdhash_it_val(&it));
	if(edge->dest->address == dst) {
	    if(del == 1)
	        stdhash_erase(&it);	  
	    return edge;
	}
	stdhash_it_keyed_next(&it); 
    }
    return NULL;
}



/***********************************************************/
/* void Add_to_changed_edges(int32 sender,                 */
/*                           struct Edge_d *edge, int how) */
/*                                                         */
/* Adds an edge to the buffer of changed edges to be sent  */
/* to neighbors                                            */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender: IP address of the node that informed me about   */
/*         the chenge                                      */
/* edge:   edge that has changed parameters                */
/* how:    NEW_CHANGE: this is first time I see the change */
/*         OLD_CHANGE: I already know about this           */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Add_to_changed_edges(int32 sender, struct Edge_d *edge, int how)
{
    stdhash_it it;
    Node *nd;
    int32 ip_address = sender;
    Changed_Edge *cg_edge;
    sp_time now;
    int remote_flag = 0;
    int temp;
    char how_str[5];  
    const sp_time short_timeout = {     0,    10000};

    if(how == NEW_CHANGE)
      strcpy(how_str, "NEW");
    else if(how == OLD_CHANGE)
      strcpy(how_str, "OLD");
    
    Alarm(DEBUG, "Add_to_changed_edges(): sender: %d.%d.%d.%d; how: %s\n",
	  IP1(sender), IP2(sender), IP3(sender), IP4(sender), how_str);
    Alarm(DEBUG, "\t\t%d.%d.%d.%d -> %d.%d.%d.%d\n", 
	  IP1(edge->source->address), IP2(edge->source->address), 
	  IP3(edge->source->address), IP4(edge->source->address), 
	  IP1(edge->dest->address), IP2(edge->dest->address), 
	  IP3(edge->dest->address), IP4(edge->dest->address)); 

    /* Find sender node in the datastructure */
    stdhash_find(&All_Nodes, &it, &ip_address);
    if(stdhash_it_is_end(&it)) {
        Alarm(EXIT, "Add_to_changed_edges(): non existing sender node\n");
    }
    nd = *((Node **)stdhash_it_val(&it));
    if((!(nd->flags&NEIGHBOR_NODE))&&(nd->address != My_Address)) {
        /* Sender node is not a neighbor !!! 
	   (probably it's not yet, Hello protocol will take care of it later)*/
	remote_flag = 1;
    }

    now = E_get_time();
   
    if(how == NEW_CHANGE) {
    	edge->my_timestamp_sec = (int32)now.sec;
    	edge->my_timestamp_usec = (int32)now.usec;	
    }
    

    if(stdhash_empty(&Changed_Edges)) {
        E_queue(Send_Link_Updates, 0, NULL, short_timeout);
    }
 
    if(nd->address == My_Address)
        Alarm(DEBUG, "nodeid: local, it's my update\n");
    else
        Alarm(DEBUG, "nodeid: %d\n", nd->node_id);
   
    /* Find the edge in the to_be_sent edges (if it exists and 
       it wasn't sent already) */
    if((cg_edge = 
	Find_Changed_Edge(edge->source->address, edge->dest->address)) != NULL) {
        if(how == NEW_CHANGE) {
	    /* Update the mask to be for this guy only */
	    if(sender == My_Address || remote_flag == 1) {
	        temp = 0;
	    }
	    else {
	        temp = 1;
		temp = temp << nd->node_id%32;
	    }
	    cg_edge->mask[nd->node_id/32] = temp;
	}
	if(how == OLD_CHANGE) {
	    /* Add this guy to the mask */
	    if(sender == My_Address || remote_flag == 1) {
	        temp = 0;
	    }
	    else {
	        temp = 1;
		temp = temp << nd->node_id%32;
	    }
	    cg_edge->mask[nd->node_id/32] |= temp;
	}
    }
    else if(how == NEW_CHANGE) {
        /* Add this edge to the list of to_be_sent */

        if((cg_edge = (Changed_Edge*) new(CHANGED_EDGE))==NULL)
	    Alarm(EXIT, "Add_to_changed_edges(): Cannot allocte edge object\n");  
    
	/* Initialize the changed_edge structure */
	if(sender == My_Address || remote_flag == 1) {
	    temp = 0;
	}
	else {
	    temp = 1;
	    temp = temp << nd->node_id%32;
	}
	cg_edge->mask[nd->node_id/32] = temp;
	cg_edge->edge = edge;
   
	/* Insert the cg_edge in the global data structures */
	stdhash_insert(&Changed_Edges, &it, &edge->source->address, &cg_edge);
    }
}


/***********************************************************/
/* Changed_Edge* Find_Changed_Edge(int32 source,           */
/*                                 int32 dest)             */
/*                                                         */
/* Finds an edge in the buffer of changed edges            */
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
/* (Changed_Edge*) a pointer to the Edge structure,        */
/* or NULL if the edge does not exist                      */
/*                                                         */
/***********************************************************/

Changed_Edge* Find_Changed_Edge(int32 source, int32 dest) 
{
    stdhash_it it;
    int32 src;
    int32 dst;
    Changed_Edge *cg_edge;
    
    src = source; dst = dest;
    
    stdhash_find(&Changed_Edges, &it, &src);
    if(stdhash_it_is_end(&it))
        return NULL;

    cg_edge = *((Changed_Edge **)stdhash_it_val(&it));
    if(cg_edge->edge->dest->address == dst) {
        return cg_edge;
    }
    stdhash_it_keyed_next(&it); 
    while(!stdhash_it_is_end(&it)) {
        cg_edge = *((Changed_Edge **)stdhash_it_val(&it));
	if(cg_edge->edge->dest->address == dst) {
	    return cg_edge;
	}
	stdhash_it_keyed_next(&it); 
    }
    return NULL;
}



/***********************************************************/
/* void Empty_Changed_Updates(void)                        */
/*                                                         */
/* Empties the buffer of changed edges                     */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Empty_Changed_Updates(void)
{
    stdhash_it it;
    Changed_Edge *cg_edge;

    stdhash_begin(&Changed_Edges, &it); 
    while(!stdhash_it_is_end(&it)) {
        cg_edge = *((Changed_Edge **)stdhash_it_val(&it));
	dispose(cg_edge);
	stdhash_it_next(&it);
    } 
    stdhash_clear(&Changed_Edges);
}
 



/* For debuging only */

void Print_Links(int dummy_int, void* dummy) 
{
    stdhash_it it;
    Edge *edge;
    const sp_time print_timeout = {    15,    0};


    Alarm(PRINT, "\n\nAvailable edges:\n");
    stdhash_begin(&All_Edges, &it); 
    while(!stdhash_it_is_end(&it)) {
        edge = *((Edge **)stdhash_it_val(&it));
	Alarm(PRINT, "\t\t%d.%d.%d.%d -> %d.%d.%d.%d :: %d | %d:%d\n", 
	      IP1(edge->source->address), IP2(edge->source->address), 
	      IP3(edge->source->address), IP4(edge->source->address), 
	      IP1(edge->dest->address), IP2(edge->dest->address), 
	      IP3(edge->dest->address), IP4(edge->dest->address), 
	      edge->cost, edge->timestamp_sec, edge->timestamp_usec); 	
	stdhash_it_next(&it);
    }

    Alarm(PRINT, "\n\n");
    Print_Routes();
    E_queue(Print_Links, 0, NULL, print_timeout);
}

