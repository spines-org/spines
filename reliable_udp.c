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
#include "reliable_link.h"
#include "udp.h"
#include "link.h"
#include "session.h"
#include "reliable_udp.h"

/* Global vriables */

extern int32     My_Address;
extern stdhash   All_Nodes;


/* Local variables */
static const sp_time zero_timeout        = {     0,    0};



/***********************************************************/
/* void Process_udp_data_packet(int32 sender_id,           */
/*                              char *buff,                */
/*                              int16u data_len,           */
/*                              int16u ack_len,            */
/*                              int32u type, int mode)     */
/*                                                         */
/* Processes a UDP data packet                             */
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
    stdhash_it it;
    udp_header *hdr;
    Node *sender_node, *next_hop;
    Link *lk;
    reliable_tail *r_tail;
    Reliable_Data *r_data;
    int flag, ret;


    /* Check if we knew about the sender of the message */
    stdhash_find(&All_Nodes, &it, &sender_id);
    if(stdhash_it_is_end(&it)) { /* I had no idea about the sender node */
	/* This guy should first send hello messages to setup 
	   the link, etc. The only reason for getting this link state update 
	   is that I crashed, recovered, and the sender didn't even notice. 
	   Create the node as it seems that it will be a neighbor in the 
	   future. Still, for now I will just make it remote. Hello protocol
	   will take care to make it a neighbor if needed */
	
	Create_Node(sender_id, REMOTE_NODE);
	stdhash_find(&All_Nodes, &it, &sender_id);
    }

    /* Take care about the reliability stuff. First, get the other side node */
    sender_node = *((Node **)stdhash_it_val(&it));
   
    /* See if we have a valid link to this guy, and take care about the acks.
     * If we don't have, ignore the acks and just consider the data in the packet,
     * it might be useful */
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
	if(lk->r_data == NULL)
	    Alarm(EXIT, "Process_rel_udp_packet(): Not a reliable link\n");
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
		if(hdr->dest == My_Address) {
		    ret = Deliver_UDP_Data(buff, data_len, type);
		}
		else {
		    next_hop = Get_Route(hdr->dest);
		    if(next_hop != NULL) {
			ret = Forward_Rel_UDP_Data(next_hop, buff, data_len, type);
		    }
		    else {
			return;
		    }
		}
	    }
	    else {
		Alarm(EXIT, "Process_rel_udp_data: Packed data... not available yet\n");
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

    
    lk = next_hop->link[RELIABLE_UDP_LINK];
    if(lk == NULL) {
	return(BUFF_DROP);
    }

    send_type = type & (ECN_DATA_MASK | ECN_ACK_MASK);
    send_type = send_type | REL_UDP_DATA_TYPE;

    ret = Reliable_Send_Msg(lk->link_id, buff, buf_len, send_type);
    
    Alarm(DEBUG, "Sent %d bytes\n", ret); 
    
    return(BUFF_OK);
}




/* This shouldn't be here. Just for testing... */

void Random_Send(int dest_t, void *dummy_p)
{
    sp_time timeout, now;
    double size_pkt;
    char* pkt;
    udp_header *hdr;
    Node *next_hop;
    int32 dest = (int32)dest_t;
    static int16u seq = 0;

    timeout.sec = 1;
    timeout.usec = 0;
    now = E_get_time();

    srand(now.usec);
    size_pkt = rand();
    size_pkt /= RAND_MAX;
    size_pkt *= sizeof(packet_body) - sizeof(udp_header) - sizeof(reliable_tail);

    if((pkt = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) 
	Alarm(EXIT, "Random_Send(): Cannot allocte pack_body object\n");

    hdr = (udp_header*)pkt;
    hdr->source = My_Address;
    hdr->dest = dest;
    hdr->dest_port = 1234;
    hdr->source_port = 1235;
    hdr->len = (int)size_pkt;
    hdr->seq_no = seq;
    seq = (seq+1)%PACK_MAX_SEQ;

    
    next_hop = Get_Route(dest);
    if(next_hop != NULL) {
	Alarm(PRINT, "Sending UDP packet of %d bytes\n", (int)size_pkt + sizeof(udp_header));

	
	Forward_Rel_UDP_Data(next_hop, pkt, (int16u)((int)size_pkt + 
						 sizeof(udp_header)), 0);
	 
    }

    dec_ref_cnt(pkt);

    E_queue(Random_Send, dest_t, NULL, timeout);
}
