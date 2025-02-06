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
#include "reliable_datagram.h"
#include "link_state.h"
#include "hello.h"
#include "route.h"
#include "udp.h"
#include "session.h"
#include "state_flood.h"
#include "link_state.h"

/* Extern variables, as defined in on.c */
extern stdhash   All_Nodes;
extern stdhash   All_Edges;
extern stdhash   Changed_Edges;
extern Link*     Links[MAX_LINKS];
extern channel   Local_Send_Channels[MAX_LINKS_4_EDGE];
extern int32     My_Address;
extern int16     Port;
extern int16     Link_Sessions_Blocked_On;
extern Prot_Def  Edge_Prot_Def;
extern Prot_Def  Groups_Prot_Def;
extern int       Minimum_Window;



/***********************************************************/
/* int16 Create_Link(int32 address, int16 mode)            */
/*                                                         */
/* Creates a link between the current node and some        */
/* neighbor                                                */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* address: the address of the neighbor                    */
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
    Control_Data  *c_data;
    Reliable_Data *r_data;
    int32      ip_address = address;
    int        i; 
    int16      linkid;


    /* Find an empty spot in the Links array */
    for(linkid=0; linkid<MAX_LINKS && Links[linkid] != NULL; linkid++);
    if(linkid == MAX_LINKS)
        Alarm(EXIT, "Create_Link() No link IDs available; too many open links\n");

    /* Find the neighbor node structure */
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
	    r_data->cong_flag = 1;
	    r_data->last_tail_resent = 0;
	    r_data->padded = 1;

	    /* Congestion control */
	    r_data->window_size = Minimum_Window; 
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
	    c_data->est_loss_rate = UNKNOWN;
	    c_data->est_tcp_rate = UNKNOWN;
	    c_data->reported_rtt = UNKNOWN;
	    c_data->reported_loss_rate = UNKNOWN;
	    

	    /* For determining loss_rate */
	    c_data->l_data.my_seq_no = -1;
	    c_data->l_data.other_side_tail = 0;
	    c_data->l_data.other_side_head = 0;
	    c_data->l_data.lost_packets = 0;
	    c_data->l_data.received_packets = 0;
	    for(i=0; i<MAX_REORDER; i++) {
		c_data->l_data.recv_flags[i] = 0;
	    }
	    for(i=0; i<LOSS_HISTORY; i++) {
		c_data->l_data.loss_interval[i].received_packets = UNKNOWN;
		c_data->l_data.loss_interval[i].lost_packets = UNKNOWN;
	    }
	    c_data->l_data.loss_event_idx = 0;	
	    c_data->l_data.loss_rate = UNKNOWN;

	    lk->prot_data = c_data;
	   
	    /* If this is the first link to this node,
	     * create an overlay edge */
	    if((edge = (Edge*)Find_State(&All_Edges, My_Address, 
				 nd->address)) == NULL) {
	        edge = Create_Overlay_Edge(My_Address, nd->address);
	    }
	    else if(edge->cost < 0){
		edge->cost = 1;
		edge->timestamp_usec++;
		if(edge->timestamp_usec >= 1000000) {
		    edge->timestamp_usec = 0;
		    edge->timestamp_sec++;
		}

	    }
	    edge->flags = CONNECTED_EDGE;
	    Add_to_changed_states(&Edge_Prot_Def, My_Address, (State_Data*)edge, NEW_CHANGE);
	    Set_Routes();
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
/* neighbor                                                */
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

	for(i=r_data->recv_tail; i<=r_data->recv_head; i++) {
	    if(r_data->recv_window[i%MAX_WINDOW].data.buff != NULL) {
		dec_ref_cnt(r_data->recv_window[i%MAX_WINDOW].data.buff);
		r_data->recv_window[i%MAX_WINDOW].data.buff = NULL;
	    }
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

	E_dequeue(Pad_Link, (int)linkid, NULL);

	if(link_obj->link_node_id == CONTROL_LINK) {
	    E_dequeue(Send_Hello, (int)linkid, NULL);
	    E_dequeue(Net_Send_State_All, (int)linkid, &Edge_Prot_Def);
	    E_dequeue(Net_Send_State_All, (int)linkid, &Groups_Prot_Def);
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
/* void Check_Link_Loss(int32 sender, int16u seq_no)       */
/*                                                         */
/* Checks the link loss probability on link                */
/* between two neighbours                                  */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender: IP of the neighbour                             */
/* seq_no: sequence number of the packet received          */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Check_Link_Loss(int32 sender, int16u seq_no) {
    Link *lk;
    stdhash_it it;
    Node *sender_node;
    Control_Data *c_data;
    Loss_Data *l_data;
    

    if(seq_no != 0xffff) {
	/* Get the link to the sender */
	stdhash_find(&All_Nodes, &it, &sender);
	if(stdhash_it_is_end(&it)) { /* I had no idea about the sender node */
	    return;
	}
	sender_node = *((Node **)stdhash_it_val(&it));
	lk = sender_node->link[CONTROL_LINK];
	if(lk == NULL)
	    return;
	if(lk->prot_data == NULL)
	    return;
	c_data = (Control_Data*)lk->prot_data;
	l_data = &(c_data->l_data);

	Alarm(DEBUG, "seq: %d; tail: %d; head: %d\n",
	      seq_no,  l_data->other_side_tail,  l_data->other_side_head);

	if(Relative_Position(l_data->other_side_tail, seq_no) <= 0) {
	    /* this is an old packet */
	    return;
	}
	else if(Relative_Position(l_data->other_side_head, seq_no) >= 0) {
	    l_data->other_side_head = (seq_no + 1)%PACK_MAX_SEQ;
	}
	if(Relative_Position(l_data->other_side_tail, l_data->other_side_head) > 2+MAX_REORDER/2) {
	    /* LOSS !!!! */
	    while((l_data->other_side_tail+1)%PACK_MAX_SEQ != l_data->other_side_head) {
		if(l_data->recv_flags[l_data->other_side_tail%MAX_REORDER] == 0) {
		    l_data->lost_packets++;
		}
		l_data->recv_flags[l_data->other_side_tail%MAX_REORDER] = 0;
		l_data->other_side_tail = (l_data->other_side_tail+1)%PACK_MAX_SEQ;
	    }
	    l_data->received_packets++;
	    l_data->recv_flags[seq_no%MAX_REORDER] = 1;

	    Alarm(DEBUG, "Update loss rate: seq: %d; recv: %d; lost: %d; sum: %d\n",
		  seq_no, l_data->received_packets, l_data->lost_packets,
		  l_data->received_packets + l_data->lost_packets);

	    /* Do smthg w/ lost_packets and received_packets*/
	    l_data->loss_event_idx++;
	    l_data->loss_interval[l_data->loss_event_idx%LOSS_HISTORY].received_packets = 
		l_data->received_packets;
	    l_data->loss_interval[l_data->loss_event_idx%LOSS_HISTORY].lost_packets = 
		l_data->lost_packets;
	    
	    l_data->received_packets = 0;
	    l_data->lost_packets = 0;

	    return;
	}
	if(l_data->recv_flags[seq_no%MAX_REORDER] == 0) {
	    l_data->received_packets++;
	    l_data->recv_flags[seq_no%MAX_REORDER] = 1;
	}
	while((l_data->recv_flags[(l_data->other_side_tail+1)%MAX_REORDER] == 1)&&
	      ((l_data->other_side_tail+1)%PACK_MAX_SEQ != l_data->other_side_head)) {
	    l_data->recv_flags[l_data->other_side_tail%MAX_REORDER] = 0;
	    l_data->other_side_tail = (l_data->other_side_tail+1)%PACK_MAX_SEQ;
	}
    }
    else {
	Alarm(DEBUG, "seq_no IGNORE\n");
    }
}


/***********************************************************/
/* int32 Relative_Position(int32 base, int32 seq)          */
/*                                                         */
/* Computes the relative position between two sequence     */
/* numbers in a circular array.                            */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* base: base sequence number                              */
/* seq_no: sequence number of offset                       */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int32) offset                                          */
/*                                                         */
/***********************************************************/

int32 Relative_Position(int32 base, int32 seq) {
    if(seq >= base) {
	if(seq - base > PACK_MAX_SEQ/2) {
	    /* this is an old packet */
	    return(seq - base - PACK_MAX_SEQ);
	}
	else {
	    return(seq - base);
	}
    }
    else {
	if(base - seq < PACK_MAX_SEQ/2) {
	    /* this is an old packet */
	    return(seq - base);
	}
	else {
	    return(seq + PACK_MAX_SEQ - base);
	}
    }
}



/***********************************************************/
/* int32 Compute_Loss_Rate(Node* nd)                       */
/*                                                         */
/* Computes the average loss rate from a neighbor          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* nd: The neighbor node                                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int32) average loss rate multiplied by LOSS_RATE_SCALE */
/*                                                         */
/***********************************************************/

int32 Compute_Loss_Rate(Node* nd) {
    Link *lk;
    Control_Data *c_data;
    Loss_Data *l_data;
    int i;
    float avg_interval, weighted_interval, weighted_loss, loss_rate, loss_report, total_weight, weight;

    /* get the Control_Data */        
    lk = nd->link[CONTROL_LINK];
    if(lk == NULL) {
	Alarm(DEBUG, "Compute_Loss_Rate(): loss rate unknown (lk==NULL)\n");
	return(UNKNOWN);
    } else if(lk->prot_data == NULL) {
	Alarm(DEBUG,"Compute_Loss_Rate(): loss rate unknown (lk->prot_data==NULL)\n");
	return(UNKNOWN);
    } else {
	c_data = (Control_Data*)lk->prot_data;
	l_data = &(c_data->l_data);

	/* get the loss rate */
	weighted_interval = 0.0;
	weighted_loss = 0.0;
	total_weight = 0.0;
	for(i=0; i<LOSS_HISTORY; i++) {
	    if(l_data->loss_event_idx > i) {
		if(i < LOSS_HISTORY/2) {
		    weight = 1;
		}
		else {
		    /* weight = 1 - (i+1-n/2)/(n/2+1); */
		    weight = i + 1 - LOSS_HISTORY/2;
		    weight = weight/(LOSS_HISTORY/2+1);
		    weight = 1 - weight;
		}
		weighted_interval += weight*l_data->loss_interval[(l_data->loss_event_idx-i)%LOSS_HISTORY].received_packets;
		weighted_loss += weight*l_data->loss_interval[(l_data->loss_event_idx-i)%LOSS_HISTORY].lost_packets;

		total_weight += weight;
		
		Alarm(DEBUG, "loss: %5.3f; interval: %5.3f\n", weighted_loss, weighted_interval);
	    }
	    else {
		return(UNKNOWN);
	    }
	}
	loss_rate = weighted_loss/(weighted_interval + weighted_loss);
	avg_interval = weighted_interval/total_weight;
	Alarm(DEBUG, "Compute_Loss_Rate(): loss: %5.3f\%; interval: %d\n", 
	      loss_rate*100, (int)avg_interval);

	if(l_data->received_packets > avg_interval) {
	    weighted_interval = l_data->received_packets;
	    weighted_loss = 1.0;
	    total_weight = 1.0;
	    for(i=0; i<LOSS_HISTORY-1; i++) {
		if(l_data->loss_event_idx >= i) {
		    if(i < LOSS_HISTORY/2 - 1) {
			weight = 1;
		    }
		    else {
			/* weight = 1 - (i+2-n/2)/(n/2+1); */
			weight = i + 2 - LOSS_HISTORY/2;
			weight = weight/(LOSS_HISTORY/2+1);
			weight = 1 - weight;
		    }
		    weighted_interval += weight*l_data->loss_interval[(l_data->loss_event_idx-i)%LOSS_HISTORY].received_packets;
		    weighted_loss += weight*l_data->loss_interval[(l_data->loss_event_idx-i)%LOSS_HISTORY].lost_packets;

		    total_weight += weight;		
		}
		else {
		    return(UNKNOWN);
		}
	    }
	    loss_rate = weighted_loss/(weighted_interval + weighted_loss);
	    avg_interval = weighted_interval/total_weight;
	    
	    Alarm(DEBUG, "\t\tchanged to: loss: %5.3f\%; interval: %d\n", 
		  loss_rate*100, (int)avg_interval);	
	}
	l_data->loss_rate = loss_rate;

	/*loss_report = LOSS_RATE_SCALE/avg_interval;*/ 
	loss_report = LOSS_RATE_SCALE*l_data->loss_rate;
	return((int32)loss_report);
    }
}



/***********************************************************/
/* int16 Set_Loss_SeqNo(Node* nd)                          */
/*                                                         */
/* Sets the sequence number of a packet                    */
/*             (for detecting loss rate)                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* nd: The neighbor node                                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int16) Sequence number                                 */
/*                                                         */
/***********************************************************/

int16 Set_Loss_SeqNo(Node* nd) 
{
    Link *lk;
    Control_Data *c_data;
    Loss_Data *l_data;

    lk = nd->link[CONTROL_LINK];
    if(lk == NULL) {
	return(UNKNOWN);
    } else if(lk->prot_data == NULL) {
	return(UNKNOWN);
    } else {
	c_data = (Control_Data*)lk->prot_data;
	l_data = &(c_data->l_data);

	l_data->my_seq_no++;
	if(l_data->my_seq_no >= PACK_MAX_SEQ)
	    l_data->my_seq_no = 0;
	
	Alarm(DEBUG, "SET LOSS SEQ: %d\n", l_data->my_seq_no);
	return(l_data->my_seq_no);
    }
}
