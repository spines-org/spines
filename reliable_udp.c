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


#include <stdlib.h>

#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/memory.h"

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "route.h"
#include "reliable_datagram.h"
#include "udp.h"
#include "link.h"
#include "session.h"
#include "reliable_udp.h"
#include "state_flood.h"
#include "multicast.h"

/* Global vriables */

extern int32     My_Address;
extern stdhash   All_Nodes;
extern stdhash   All_Groups_by_Node; 
extern stdhash   All_Groups_by_Name; 
extern stdhash   Neighbors;
extern int       Padding;


/* Local variables */
static const sp_time zero_timeout        = {     0,     0};
static const sp_time short_timeout       = {     0, 10000};



/***********************************************************/
/* void Process_rel_udp_data_packet(int32 sender_id,       */
/*                              char *buff,                */
/*                              int16u data_len,           */
/*                              int16u ack_len,            */
/*                              int32u type, int mode)     */
/*                                                         */
/* Processes a reliable UDP data packet                    */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender_id: IP of the node that gave me the message      */
/* buff:      a buffer containing the message              */
/* data_len:  length of the data in the packet             */
/* ack_len:   length of the ack in the packet              */
/* type:      type of the packet                           */
/* mode:      mode of the link the packet arrived on       */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/


void Process_rel_udp_data_packet(int32 sender_id, char *buff, 
				 int16u data_len, int16u ack_len,
				 int32u type, int mode)
{
    stdhash_it it, ngb_it, grp_it, st_it;
    udp_header *hdr;
    Node *sender_node, *next_hop;
    Link *lk;
    reliable_tail *r_tail;
    Reliable_Data *r_data;
    Group_State *g_state;
    State_Chain *s_chain_grp;
    int flag, ret;


    /* Check if we knew about the sender of the message */
    stdhash_find(&All_Nodes, &it, &sender_id);
    if(stdhash_it_is_end(&it)) { /* I had no idea about the sender node */
	/* This guy should first send hello messages to setup 
	   the link, etc. Ignore the packet. It's either a bug, 
	   race condition, or an attack */

	   return;
    }

    /* Take care about the reliability stuff. First, get the other side node */
    sender_node = *((Node **)stdhash_it_val(&it));
   
    /* See if we have a valid link to this guy, and take care about the acks.
     * If we don't have, ignore the packet. */
    if(sender_node->node_id >= 0) {
	/* Ok, this node is a neighbor. Let's see the link to it */
        /* Alarm(DEBUG, "Neighbor node\n"); */

	lk = sender_node->link[RELIABLE_UDP_LINK];
	if(lk == NULL) {
	    /* Got a reliable packet on a link that is not initialized 
	     * Ignore the packet... 
	     */
	    
	    return;
	}
	if(lk->r_data == NULL) {
	    Alarm(PRINT, "Process_rel_udp_packet(): Not a reliable link\n");
	    return;
	}
	r_data = lk->r_data;

	if(r_data->flags&CONNECTED_LINK) {

	    /* Get the reliable tail from the packet */
	    r_tail = (reliable_tail*)(buff+data_len);

	    /* Process the ack part. 
	     * If the packet is not needed (we processed it earlier) just return */ 
	    flag = Process_Ack(lk->link_id, (char*)r_tail, ack_len, type);
	
	    /* We should send an acknowledge for this message. 
	     * So, let's schedule it 
	     */
	    if(flag == -1) {
		/* This is just an ack packet */
		Alarm(PRINT, "Warning !!! Ack packets should be treated differently !\n");
		return;
	    }
	    
	    r_data->scheduled_ack = 1;
	    E_queue(Send_Ack, (int)lk->link_id, NULL, zero_timeout);
	
	    if(flag == 0) {
		/* This is an old packet (retrans), and therefore not useful */
		
		Alarm(DEBUG, "retransm. received\n");
		return;
	    }
	    
	    /* If we got up to here, this is a new packet that needs to
	     * be processsed. All the link reliablility issues are already
	     * taken care of.
	     */
	    
	    hdr = (udp_header*)buff; 
	    
	    if(!Same_endian(type))
		Flip_udp_hdr(hdr);
	    
	    if(hdr->len + sizeof(udp_header) == data_len) {
		if((hdr->dest & 0xF0000000) != 0xE0000000) {
		    /* This is not a multicast address */
		    
		    /* Is this for me ? */
		    if(hdr->dest == My_Address) {
			ret = Deliver_UDP_Data(buff, data_len, 0);
			return;
		    }
		    
		    /* Nope, it's for smbd else. See where we should forward it */
		    next_hop = Get_Route(hdr->source, hdr->dest);
		    if(next_hop != NULL) {
			ret = Forward_Rel_UDP_Data(next_hop, buff, data_len, 0);
			return;
		    }
		    else {
			return;
		    }
		}
		else { /* This is multicast */
		    if(Find_State(&All_Groups_by_Node, My_Address, hdr->dest) != NULL) {
			/* Hey, I joined this group !*/
			Deliver_UDP_Data(buff, data_len, 0);
		    }
		    stdhash_find(&All_Groups_by_Name, &grp_it, &hdr->dest);
		    if(!stdhash_it_is_end(&grp_it)) {
			s_chain_grp = *((State_Chain **)stdhash_it_val(&grp_it));
			stdhash_begin(&s_chain_grp->states, &st_it);
			while(!stdhash_it_is_end(&st_it)) {
			    g_state = *((Group_State **)stdhash_it_val(&st_it));
			    if(g_state->flags & ACTIVE_GROUP) {
				next_hop = Get_Route(hdr->source, g_state->source_addr);
				if(next_hop != NULL) {
				    stdhash_find(&Neighbors, &ngb_it, &next_hop->address);
				    if(stdhash_it_is_end(&ngb_it)) {
					stdhash_insert(&Neighbors, &ngb_it, &next_hop->address, &next_hop);
				    }
				}
			    }
			    stdhash_it_next(&st_it);
			}
		    }
		    
		    ret = NO_ROUTE;
		    
		    stdhash_begin(&Neighbors, &ngb_it);
		    while(!stdhash_it_is_end(&ngb_it)) {
			next_hop = *((Node **)stdhash_it_val(&ngb_it));
			ret = Forward_Rel_UDP_Data(next_hop, buff, data_len, 0);
			stdhash_it_next(&ngb_it);
		    }
		    stdhash_clear(&Neighbors);
		    return;
		}
	    }
	    else {
		Alarm(PRINT, "Process_rel_udp_data: Packed data... not available yet\n");
		return;
	    }
	}
    }
}





/***********************************************************/
/* int Forward_Rel_UDP_Data((Node *next_hop, char *buff,   */
/*                       int16u buf_len, int32u type)      */
/*                                                         */
/*                                                         */
/*                                                         */
/* Forwards a reliable UDP data packet                     */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* next_hop:  the next node on the path                    */
/* buff:      a buffer containing the message              */
/* buf_len:   length of the packet                         */
/* type:      type of the message                          */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the status of the packet (see udp.h)              */
/*                                                         */
/***********************************************************/

int Forward_Rel_UDP_Data(Node *next_hop, char *buff, int16u buf_len, int32u type)
{
    Link *lk;
    int ret;
    int32u send_type;
    Reliable_Data *r_data;
    int buff_size;

    
    if(next_hop->address == My_Address) {
	Process_udp_data_packet(My_Address, buff, buf_len, 
				 type|UDP_DATA_TYPE, UDP_LINK);
	
	return(BUFF_EMPTY);
    }

    lk = next_hop->link[RELIABLE_UDP_LINK];
    if(lk == NULL) {
	return(BUFF_DROP);
    }
    r_data = lk->r_data;
    if(r_data == NULL) {
	return(BUFF_DROP);
    }
    buff_size = stdcarr_size(&(r_data->msg_buff));
    if(buff_size > 2*MAX_BUFF_LINK) {
	return(BUFF_DROP);
    }
    

    send_type = type & (ECN_DATA_MASK | ECN_ACK_MASK);
    send_type = send_type | REL_UDP_DATA_TYPE;

    ret = Reliable_Send_Msg(lk->link_id, buff, buf_len, send_type);

    Alarm(DEBUG, "Sent %d bytes\n", ret); 
    
    if(Padding == 1) {
	if(r_data != NULL) {
	    E_dequeue(Pad_Link, (int)lk->link_id, NULL);
	    r_data->padded = 0;
	    E_queue(Pad_Link, (int)lk->link_id, NULL, short_timeout);
	}
    }

    return(BUFF_OK);
}

