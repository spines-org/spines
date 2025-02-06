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
#include "util/data_link.h"
#include "stdutil/src/stdutil/stdhash.h"
#include "stdutil/src/stdutil/stdcarr.h"

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "link.h"
#include "network.h"
#include "reliable_datagram.h"
#include "link_state.h"
#include "hello.h"
#include "udp.h"
#include "reliable_udp.h"
#include "realtime_udp.h"
#include "protocol.h"
#include "route.h"
#include "session.h"
#include "state_flood.h"
#include "multicast.h"


/* Global vriables */

extern int32     My_Address;
extern stdhash   All_Nodes;
extern Link*     Links[MAX_LINKS];
extern int       network_flag;
extern stdhash   All_Groups_by_Node; 
extern stdhash   All_Groups_by_Name; 
extern stdhash   Neighbors;
extern int       Security;
extern int       Unicast_Only;



/* Local constatnts */

static const sp_time zero_timeout  = {     0,    0};




/***********************************************************/
/* void Process_RT_UDP_data_packet(int32 sender_id,        */
/*                              char *buff,                */
/*                              int16u data_len,           */
/*			        int32u type, int mode)     */
/*                                                         */
/* Processes a Realtime UDP data packet                    */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender_id: IP of the node that gave me the message      */
/* buff:      a buffer containing the message              */
/* data_len:  length of the data in the packet             */
/* type:      type of the packet                           */
/* mode:      mode of the link the packet arrived on       */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Process_RT_UDP_data_packet(int32 sender_id, char *buff, int16u data_len, 
			     int16u ack_len, int32u type, int mode)
{
    udp_header *hdr;
    Node *next_hop, *sender_node;
    Link *lk;
    Realtime_Data *rt_data;
    stdit it, ngb_it;
    int32 seq_no;
    int32 diff;
    sp_time now;
    int ret, i;
    stdhash *neighbors;


    /* First check the receiving sideof the link.
       Then we will worry about what to do with this packet */

    /* Check if we knew about the sender of the message */
    stdhash_find(&All_Nodes, &it, &sender_id);
    if(stdhash_is_end(&All_Nodes, &it)) { /* I had no idea about the sender node */
	/* This guy should first send hello messages to setup 
	   the link, etc. Ignore the packet. It's either a bug, 
	   race condition, or an attack */

	   return;
    }
    sender_node = *((Node **)stdhash_it_val(&it));

    /* See if we have a valid link to this guy. If we don't have, ignore the packet. */
    if(sender_node->node_id < 0) {
	/*This is not a neighbor. Return */
	return;
    }
    
    /* Ok, this node is a neighbor. Let's see the link to it */
    /* Alarm(DEBUG, "Neighbor node\n"); */
    
    lk = sender_node->link[REALTIME_UDP_LINK];
    if(lk == NULL) {
	/* Got a realtime packet on a link that is not initialized 
	 * Ignore the packet... 
	 */
	
	return;
    }
    if(lk->prot_data == NULL) {
	Alarm(PRINT, "Process_RT_UDP_packet(): Not a realtime link\n");
	return;
    }
    rt_data = (Realtime_Data*)lk->prot_data;


    hdr = (udp_header*)buff;
    seq_no = *(int*)(buff+data_len);

    if(!Same_endian(type)) {
	Flip_udp_hdr(hdr);
	seq_no = Flip_int32(seq_no);
    }


    /* decrement the TTL */
    if(hdr->ttl != 0) {
        hdr->ttl--;
    } else {  
        /* the ttl value in this function should never be 0, becuase we should never be sending out packets of ttl 0 */
        Alarm(EXIT, "Process_RT_UDP_data_packet(): processing a packet with TTL of 0, not allowed.\n");
    }

    if(seq_no < rt_data->recv_tail) {
	/* This is an old packet. Ignore it. */
	return;
    }
    if((seq_no < rt_data->recv_head)&&
       (rt_data->recv_window[seq_no%MAX_HISTORY].flags != EMPTY_CELL)) {
	/* This is a duplicate. Ignore it. */
	return;
    }

    now = E_get_time();

    /* Advance the receive tail if possible */

    while(rt_data->recv_tail < rt_data->recv_head) {
	i = rt_data->recv_tail;
	while((rt_data->recv_window[i%MAX_HISTORY].flags == EMPTY_CELL)&&
	      (i<(int)rt_data->recv_head)) {
	    i++;
	}
	if(i == (int)rt_data->recv_head) {
	    /* No packets since the last tail. Keep it there as we wait for them */
	    break;
	}

	/* Check whether the oldest packet is old or new */
	diff = now.sec - rt_data->recv_window[i%MAX_HISTORY].timestamp.sec;
	diff *= 1000000;
	diff += now.usec - rt_data->recv_window[i%MAX_HISTORY].timestamp.usec;
	if(diff <= HISTORY_TIME) {
	    /* This is a a recent packet. Keep it the tail */
	    break;
	}

	/* The oldest packet is old. Move the tail up */
	
	rt_data->recv_window[i%MAX_HISTORY].flags = EMPTY_CELL;
	rt_data->recv_tail = i+1;
    }
    
    if(seq_no >= rt_data->recv_head) {
	/* This is a new (and higher) packet. If we don't have room for 
	 it in the history, advance the tail */
	while(seq_no - rt_data->recv_tail >= MAX_HISTORY) {
	    rt_data->recv_window[rt_data->recv_tail%MAX_HISTORY].flags = EMPTY_CELL;
	    rt_data->recv_tail++;
	}
	for(i=rt_data->recv_head; i<(int)seq_no; i++) {
	    rt_data->recv_window[i%MAX_HISTORY].flags = EMPTY_CELL;
	    /* Add lost packet to the retransm. request */
	    
	    if(rt_data->num_nacks*sizeof(int32) + 2*sizeof(int32)< 
	       sizeof(packet_body) - sizeof(udp_header)) {
		*(int*)(rt_data->nack_buff+rt_data->num_nacks*sizeof(int32)) = i;
		rt_data->num_nacks++;
	    }
	}
	if(rt_data->num_nacks > 0) {
	    E_queue(Send_RT_Nack, (int)lk->link_id, NULL, zero_timeout);
	}
	rt_data->recv_head = seq_no+1;;
    }
    
    rt_data->recv_window[seq_no%MAX_HISTORY].flags = RECVD_CELL;
    rt_data->recv_window[seq_no%MAX_HISTORY].timestamp = now;
    

    Alarm(DEBUG, "recv_tail: %d; diff: %d\n", rt_data->recv_tail, 
	  rt_data->recv_head - rt_data->recv_tail);
    
    /* Now process the packet */
    if(hdr->len + sizeof(udp_header) == data_len) {	
	if(!Is_mcast_addr(hdr->dest) && !Is_acast_addr(hdr->dest)) {
	    /* This is not a multicast address */
	    
	    /* Is this for me ? */
	    if(hdr->dest == My_Address) {
		ret = Deliver_UDP_Data(buff, data_len, 0);
		return;
	    }
	    
	    /* Nope, it's for smbd else. See where we should forward it */
	    next_hop = Get_Route(hdr->source, hdr->dest);
        if ((next_hop != NULL) && (hdr->ttl > 0)) {
		ret = Forward_RT_UDP_Data(next_hop, buff, data_len);
		return;
	    }
	    else {
		return;
	    }
	}
	else { /* This is multicast */
	    if(Unicast_Only == 1) {
		return;
	    }
	    if(Find_State(&All_Groups_by_Node, My_Address, hdr->dest) != NULL) {
		/* Hey, I joined this group !*/
		Deliver_UDP_Data(buff, data_len, 0);
	    }
#if 0
	    stdhash_find(&All_Groups_by_Name, &grp_it, &hdr->dest);
	    if(!stdhash_is_end(&All_Groups_by_Name, &grp_it)) {
		s_chain_grp = *((State_Chain **)stdhash_it_val(&grp_it));
		stdhash_begin(&s_chain_grp->states, &st_it);
		while(!stdhash_is_end(&s_chain_grp->states, &st_it)) {
		    g_state = *((Group_State **)stdhash_it_val(&st_it));
		    if(g_state->flags & ACTIVE_GROUP) {
			next_hop = Get_Route(hdr->source, g_state->source_addr);
			if(next_hop != NULL) {
			    stdhash_find(&Neighbors, &ngb_it, &next_hop->address);
			    if(stdhash_is_end(&Neighbors, &ngb_it)) {
				stdhash_insert(&Neighbors, &ngb_it, &next_hop->address, 
					       &next_hop);
			    }
			}
		    }
		    stdhash_it_next(&st_it);
		}
	    }
	    ret = NO_ROUTE;
#endif
	    
	    if((neighbors = Get_Mcast_Neighbors(hdr->source, hdr->dest)) != NULL) {
              if (hdr->ttl > 0) {
	        stdhash_begin(neighbors, &ngb_it);
		while(!stdhash_is_end(neighbors, &ngb_it)) {
		    next_hop = *((Node **)stdhash_it_val(&ngb_it));
		    ret = Forward_RT_UDP_Data(next_hop, buff, data_len);
		    stdhash_it_next(&ngb_it);
		}
	      }
              return;
            }
	}
    }
    else {
	Alarm(PRINT, "Process_udp_data: Packed data... not available yet\n");
	return;
    }
}



/***********************************************************/
/* int Forward_RT_UDP_data((Node *next_hop, char *buff,    */
/*                       int16u buf_len)                   */
/*                                                         */
/*                                                         */
/*                                                         */
/* Forward a UDP data packet                               */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* next_hop:  the next node on the path                    */
/* buff:      buffer containing the message                */
/* buf_len:   length of the packet                         */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the status of the packet (see udp.h)              */
/*                                                         */
/***********************************************************/

int Forward_RT_UDP_Data(Node *next_hop, char *buff, int16u buf_len)
{
    Link *lk;
    packet_header hdr;
    sys_scatter scat;
    Realtime_Data *rt_data;
    History_Cell *h_cell;
    int ret, diff;
    sp_time now;

    
    if(next_hop->address == My_Address) {
	Process_udp_data_packet(My_Address, buff, buf_len, 
				UDP_DATA_TYPE, UDP_LINK);
	
	return(BUFF_EMPTY);
    }


    lk = next_hop->link[REALTIME_UDP_LINK];
    if(lk == NULL) {
	return(BUFF_DROP);
    }

    rt_data = lk->prot_data;
    if(rt_data == NULL) {
	return(BUFF_DROP);
    }

    now = E_get_time();

    /* Clean the history window of old packets */
    while(rt_data->tail < rt_data->head) {
	h_cell = &rt_data->window[rt_data->tail%MAX_HISTORY];
	diff = (now.usec - h_cell->timestamp.usec) +
	    1000000*(now.sec - h_cell->timestamp.sec);
	if(diff > HISTORY_TIME) {
	    dec_ref_cnt(rt_data->window[rt_data->tail%MAX_HISTORY].buff);
	    rt_data->window[rt_data->tail%MAX_HISTORY].buff = NULL;
	    rt_data->tail++;
	    Alarm(DEBUG, "Forward_RT_UDP_Data: History time limit reached\n");
	}
	else {
	    break;
	}
    }
    
    /* Drop the last packet if there is no more room in the window */
    if(rt_data->head - rt_data->tail >= MAX_HISTORY){
	dec_ref_cnt(rt_data->window[rt_data->tail%MAX_HISTORY].buff);
	rt_data->window[rt_data->tail%MAX_HISTORY].buff = NULL;
	rt_data->tail++;
	Alarm(DEBUG, "Forward_RT_UDP_Data: History window limit reached\n");
    }


    scat.num_elements = 2;
    scat.elements[0].len = sizeof(packet_header);
    scat.elements[0].buf = (char *) &hdr;
    scat.elements[1].len = buf_len+sizeof(int32);
    scat.elements[1].buf = buff;
    
    hdr.type    = REALTIME_DATA_TYPE;
    hdr.type    = Set_endian(hdr.type);

    hdr.sender_id = My_Address;
    hdr.data_len  = buf_len;
    hdr.ack_len   = sizeof(int32);
    hdr.seq_no    = Set_Loss_SeqNo(lk->other_side_node);

    /* Set the sequence number of the packet */
    *(int*)(buff+buf_len) = rt_data->head;

    /* Save the packet in the window */
    rt_data->window[rt_data->head%MAX_HISTORY].buff = buff;
    inc_ref_cnt(buff);
    rt_data->window[rt_data->head%MAX_HISTORY].len = buf_len;
    rt_data->window[rt_data->head%MAX_HISTORY].timestamp = now;

    /* Advance the head of the window */
    rt_data->head++;

    if(rt_data->bucket < MAX_BUCKET) {
	rt_data->bucket++;
    }

    if(network_flag == 1) {
#ifdef SPINES_SSL
	if (!Security) {
#endif
   	    ret = DL_send(lk->chan, 
			  next_hop->address,
			  lk->port, 
			  &scat );
#ifdef SPINES_SSL
	} else {
	    ret = DL_send_SSL(lk->chan, 
			      lk->link_node_id,
			      next_hop->address,
			      lk->port, 
			      &scat );
	}
#endif
	if(ret < 0) {
	    return(BUFF_DROP);
	}
    }
    return(BUFF_EMPTY);
}



/***********************************************************/
/* void Clean_RT_history((Node *neighbor)                  */
/*                                                         */
/* Advances the history tail discarding old packets        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* neighbor:  the neighbor node                            */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Clean_RT_history(Node *neighbor)
{
    Link *lk;
    Realtime_Data *rt_data;
    History_Cell *h_cell;
    int diff;
    sp_time now;


    lk = neighbor->link[REALTIME_UDP_LINK];
    if(lk == NULL) {
	return;
    }
    
    rt_data = lk->prot_data;
    if(rt_data == NULL) {
	return;
    }

    now = E_get_time();
    
    /* Clean the history window of old packets */
    while(rt_data->tail < rt_data->head) {
	h_cell = &rt_data->window[rt_data->tail%MAX_HISTORY];
	diff = (now.usec - h_cell->timestamp.usec) +
	    1000000*(now.sec - h_cell->timestamp.sec);
	if(diff > HISTORY_TIME) {
	    dec_ref_cnt(rt_data->window[rt_data->tail%MAX_HISTORY].buff);
	    rt_data->window[rt_data->tail%MAX_HISTORY].buff = NULL;
	    rt_data->tail++;
	    Alarm(DEBUG, "Clean_RT_history: History cleaned\n");
	}
	else {
	    break;
	}
    }
}


/***********************************************************/
/* void Send_RT_Nack(int16 linkid, void* dummy)            */
/*                                                         */
/* Sends an Realtime NACK                                  */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid:    ID of the link to send on                    */
/* dummy:     Not used                                     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Send_RT_Nack(int linkid, void* dummy) 
{
    sys_scatter scat;
    packet_header hdr;
    Link *lk;
    Realtime_Data *rt_data;
    int ret;


    /* Getting Link and protocol data from linkid */
    lk = Links[linkid];
    if(lk == NULL)
	Alarm(EXIT, "Send_RT_nack(): link not valid\n");

    if(lk->prot_data == NULL) 
	Alarm(EXIT, "Send_RT_nack: Not a reliable link\n");
    
    rt_data = (Realtime_Data*)lk->prot_data;
    
    scat.num_elements = 2; /* For now there are only two elements in 
			      the scatter */
    scat.elements[0].len = sizeof(packet_header);
    scat.elements[0].buf = (char *)(&hdr);
    scat.elements[1].len = rt_data->num_nacks*sizeof(int32);
    scat.elements[1].buf = rt_data->nack_buff;
	
    /* Preparing a packet header */
    hdr.type      = REALTIME_NACK_TYPE;
    hdr.type      = Set_endian(hdr.type);
    hdr.sender_id = My_Address;
    hdr.data_len  = 0; 
    hdr.ack_len   = rt_data->num_nacks*sizeof(int32);
    hdr.seq_no    = Set_Loss_SeqNo(lk->other_side_node);
    
    rt_data->num_nacks = 0;
    
    /* Sending the ack*/
    if(network_flag == 1) {
#ifdef SPINES_SSL
	if (!Security) {
#endif
	    ret = DL_send(Links[linkid]->chan, 
			  Links[linkid]->other_side_node->address,
			  Links[linkid]->port, 
			  &scat);
#ifdef SPINES_SSL
	} else {
	    ret = DL_send_SSL(Links[linkid]->chan, 
			      Links[linkid]->link_node_id,
			      Links[linkid]->other_side_node->address,
			      Links[linkid]->port, 
			      &scat);
	}
#endif
	
	Alarm(DEBUG, "Sent: data: %d; ack: %d; hdr: %d; total: %d\n",
	      hdr.data_len, hdr.ack_len, sizeof(packet_header), ret);
    }
}


/***********************************************************/
/* void Send_RT_Retransm(int16 linkid, void* dummy)        */
/*                                                         */
/* Sends an Realtime retransmission                        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid:    ID of the link to send on                    */
/* dummy:     Not used                                     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Send_RT_Retransm(int linkid, void* dummy) 
{
    Link *lk;
    packet_header hdr;
    sys_scatter scat;
    Realtime_Data *rt_data;
    History_Cell *h_cell;
    int diff, i, ret;
    sp_time now;
    int32u seq_no;
    char *buff;
    int16u buf_len;

   
    /* Getting Link and protocol data from linkid */
    lk = Links[linkid];
    if(lk == NULL) 
	Alarm(EXIT, "Send_RT_nack(): link not valid\n");

    if(lk->prot_data == NULL) 
	Alarm(EXIT, "Send_RT_nack: Not a reliable link\n");
    
    rt_data = (Realtime_Data*)lk->prot_data;

    if(rt_data->retransm_buff == NULL) {
	return;
    }
   

    now = E_get_time();

    /* Clean the history window of old packets */
    while(rt_data->tail < rt_data->head) {
	h_cell = &rt_data->window[rt_data->tail%MAX_HISTORY];
	diff = (now.usec - h_cell->timestamp.usec) +
	    1000000*(now.sec - h_cell->timestamp.sec);
	if(diff > HISTORY_TIME) {
	    dec_ref_cnt(rt_data->window[rt_data->tail%MAX_HISTORY].buff);
	    rt_data->window[rt_data->tail%MAX_HISTORY].buff = NULL;
	    rt_data->tail++;
	    Alarm(DEBUG, "Clean_RT_history: History cleaned\n");
	}
	else {
	    break;
	}
    }

    /* Resend the packets here... */

    for(i=0; i < rt_data->num_retransm; i++) {
	seq_no = *(int*)(rt_data->retransm_buff+i*sizeof(int32));
	if(seq_no >= rt_data->head) {
	    Alarm(DEBUG, "Request RT retransm for a message that wasn't sent\n");
	    continue;
	}
	if(seq_no >= rt_data->tail) {
	    if(rt_data->bucket < RT_RETRANSM_TOK) {
		break;
	    }
	    rt_data->bucket -= RT_RETRANSM_TOK;

	    buff = rt_data->window[seq_no%MAX_HISTORY].buff;
	    buf_len = rt_data->window[seq_no%MAX_HISTORY].len;
	    
	    Alarm(DEBUG, "resending %d\n", seq_no);

	    /* Send the retransm. */	    
	    scat.num_elements = 2;
	    scat.elements[0].len = sizeof(packet_header);
	    scat.elements[0].buf = (char *) &hdr;
	    scat.elements[1].len = buf_len+sizeof(int32);
	    scat.elements[1].buf = buff;
	    
	    hdr.type    = REALTIME_DATA_TYPE;
	    hdr.type    = Set_endian(hdr.type);

	    hdr.sender_id = My_Address;
	    hdr.data_len  = buf_len;
	    hdr.ack_len   = sizeof(int32);
	    hdr.seq_no    = Set_Loss_SeqNo(lk->other_side_node);

	    /* Set the sequence number of the packet */
	    *(int*)(buff+buf_len) = seq_no;
    
	    if(network_flag == 1) {
#ifdef SPINES_SSL
		if (!Security) {
#endif
		    ret = DL_send(lk->chan, 
				  lk->other_side_node->address,
				  lk->port, 
				  &scat );
#ifdef SPINES_SSL
		} else {
		    ret = DL_send_SSL(lk->chan, 
				      lk->link_node_id,
				      lk->other_side_node->address,
				      lk->port, 
				      &scat );
		}
#endif		
		if(ret < 0) {
		    break;
		}
            }
	}
    }
    

    dec_ref_cnt(rt_data->retransm_buff);
    rt_data->retransm_buff = NULL;
    rt_data->num_retransm = 0;

    Alarm(DEBUG, "Send_RT_Retransm\n");

}



/***********************************************************/
/* void Process_RT_nack_packet(int32 sender, char *buff,   */
/*                  int16u ack_len, int32u type, int mode) */
/*                                                         */
/* Processes an ACK packet                                 */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender:    IP of the sender                             */
/* buff:      buffer cointaining the ACK                   */
/* ack_len:   length of the ACK                            */
/* type:      type of the packet, cointaining endianess    */
/* mode:      mode of the link                             */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/
                                                         

void Process_RT_nack_packet(int32 sender, char *buf, int16u ack_len, int32u type, int mode)
{
    stdit it;
    Node *sender_node;
    Link *lk = NULL;
    int32 sender_ip = sender;
    Realtime_Data *rt_data;
    int32 *tmp;
    int i;


    if(mode != REALTIME_UDP_LINK) {
	Alarm(PRINT, "Got a realtime ack on a non-realtime link\n");
	return;
    }

    /* Check if we knew about the sender of thes message */
    stdhash_find(&All_Nodes, &it, &sender_ip);
    if(stdhash_is_end(&All_Nodes, &it)) { /* I had no idea about the sender node */
	/* This guy should first send hello messages to setup 
	   the link, etc. The only reason for getting this ack
	   is that I crashed, recovered, and the sender didn't even notice. 
	   Hello protocol will take care to make it a neighbor if needed */
	
	return;
    }

    /* First, get the other side node */
    sender_node = *((Node **)stdhash_it_val(&it));
    
    /* See if we have a valid link to this guy, and take care about the acks.
     * If we don't have a link, ignore the ack. */
    if(sender_node->node_id >= 0) {
	/* Ok, this node is a neighbor. Let's see the link to it */
	lk = sender_node->link[mode];

	if(lk == NULL) {
	    return;
	}
	if(lk->prot_data == NULL) { 
	    return;
	}
	rt_data = lk->prot_data;

	if(rt_data->retransm_buff != NULL) {
	    dec_ref_cnt(rt_data->retransm_buff);
	}

	tmp = (int32*)buf;
	if(!Same_endian(type)) {
	    for(i=0; i<ack_len/sizeof(int32); i++) {
		*tmp = Flip_int32(*tmp);
		tmp++;
	    }
	}

	rt_data->retransm_buff = buf;
	inc_ref_cnt(buf);
	rt_data->num_retransm = ack_len/sizeof(int32);

	E_queue(Send_RT_Retransm, (int)lk->link_id, NULL, zero_timeout);
    } 
}   
