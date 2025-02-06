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


#ifndef	ARCH_PC_WIN95

#include <netdb.h>
#include <sys/socket.h>

#else

#include <winsock.h>

#endif

#include <stdlib.h>
#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/data_link.h"
#include "util/memory.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "link.h"
#include "network.h"
#include "reliable_link.h"
#include "link_state.h"
#include "hello.h"
#include "protocol.h"
#include "route.h"

/* Global Variables */ 

extern Node* Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
extern stdhash   All_Nodes;
extern stdhash   All_Edges;
extern stdhash   Changed_Edges;
extern int16 Num_Neighbors;
extern Link* Links[MAX_LINKS];
extern int32 My_Address;


/* Local variables */

static const sp_time zero_timeout        = {     0,    0};
static const sp_time short_timeout       = {     0,    50000}; /* 50 milliseconds */
static const sp_time edge_resend_time    = { 30000,    0}; 
static const sp_time resend_call_timeout = {  3000,    0}; 
static const sp_time gb_collect_remove   = { 90000,    0}; 
static const sp_time gb_collect_timeout  = { 10000,    0}; 



void	Flip_link_state_pack(link_state_packet *pack)
{
    pack->source      = Flip_int32(pack->source);
    pack->num_edges   = Flip_int16(pack->num_edges);
}

void	Flip_edge_cell_pack(edge_cell_packet *pack)
{
    pack->dest    = Flip_int32(pack->dest);
    pack->timestamp_sec  = Flip_int32(pack->timestamp_sec);
    pack->timestamp_usec = Flip_int32(pack->timestamp_usec);
    pack->cost	  = Flip_int32(pack->cost);
}



/***********************************************************/
/* void Net_Send_Link_State_All(int lk_id, void *dummy)    */
/*                                                         */
/* Sends the whole link state matrix to a neighbour        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* lk_id: link to the neighbour                            */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Net_Send_Link_State_All(int lk_id, void *dummy) 
{
    int16 linkid;
    char   *buff;
    link_state_packet *pkt;
    edge_cell_packet *edge_cell;
    stdhash_it it, src_it, tmp_it;
    stdhash  sources;
    Edge *edge_p, *tmp_edge_p;
    int32 src_address;
    int ret;
    int pack_bytes = 0;

    Alarm(DEBUG, "+++ Net_Send_Link_State_All()\n");

    linkid = (int16)lk_id;

    /* See if we have anything to send */
    stdhash_begin(&All_Edges, &it); 
    if(stdhash_it_is_end(&it)) 
        return;

    /* Allocating packet_body for the update */

    if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
           Alarm(EXIT, "Net_Send_Link_State_all(): Cannot allocte pack_body object\n");
    }

    /* 'sources' is a temporary hash where we keep the processed 
     * source nodes */
    stdhash_construct(&sources, sizeof(int32), 0, 
                      stdhash_int_equals, stdhash_int_hashcode);


    /* Create the link state packet */

    /* All the edges */
    while(!stdhash_it_is_end(&it)) {
        /* one by one... */
      	edge_p = *((Edge **)stdhash_it_val(&it));

	/* did we consider this source ? */
	stdhash_find(&sources, &src_it, &edge_p->source->address);
	if(stdhash_it_is_end(&src_it)) {
	    /* we didn't, so let's see the edges directed to it */
	    src_address = edge_p->source->address;
	    stdhash_find(&All_Edges, &tmp_it, &src_address);
	    tmp_edge_p = *((Edge **)stdhash_it_val(&tmp_it));

	    /* Do we still have room in the packet for a 
	     * link state packet containing at least a cost cell ? */

	    if(pack_bytes > sizeof(packet_body) - sizeof(reliable_tail) -
	       sizeof(link_state_packet) - sizeof(edge_cell_packet)) {
	        
	        Alarm(DEBUG, "%s%s",
		      "Net_Send_Link_State_All: not enough room in the packet (1)\n",
		      "\t...sending this one and starting a new one\n");
		
		/* Close the packet and send it... */
		ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, LINK_STATE_TYPE);
		Alarm(DEBUG, "LINK STATE sent %d bytes; header: %d; Pkt: %d\n", 
		      ret, sizeof(packet_header), pack_bytes);
		dec_ref_cnt(buff);

		/* Initiate a new packet and continue from there on */
		if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
		    Alarm(EXIT, "Net_Send_Link_State_all(): Cannot allocte pack_body object\n");
		}
		
		pack_bytes = 0;
	    }

	    /* Initialize the data structure pointers */
	    pkt = (link_state_packet*)(buff+pack_bytes);
	    pkt->source = tmp_edge_p->source->address;
	    pkt->num_edges = 1;

	    pack_bytes += sizeof(link_state_packet);

	    edge_cell = (edge_cell_packet*)(buff+pack_bytes);
	
	    edge_cell->dest = tmp_edge_p->dest->address;
	    edge_cell->timestamp_sec  = tmp_edge_p->timestamp_sec;
	    edge_cell->timestamp_usec = tmp_edge_p->timestamp_usec;
	    edge_cell->cost   = tmp_edge_p->cost;
	    
	    pack_bytes += sizeof(edge_cell_packet);
     
	    Alarm(DEBUG, "Packing edge: %d.%d.%d.%d -> %d.%d.%d.%d :: %d | %d:%d\n", 
		  IP1(pkt->source), IP2(pkt->source), 
		  IP3(pkt->source), IP4(pkt->source),
		  IP1(edge_cell->dest), IP2(edge_cell->dest), 
		  IP3(edge_cell->dest), IP4(edge_cell->dest),
		  edge_cell->cost, edge_cell->timestamp_sec, edge_cell->timestamp_usec); 
	   
	    
	    /* See if we still have other destinations from this source */
	    
	    stdhash_it_keyed_next(&tmp_it);
	    while(!stdhash_it_is_end(&tmp_it)) {
	        tmp_edge_p = *((Edge **)stdhash_it_val(&tmp_it));

		/* Do we still have room in the packet for a 
		 * at least a cost cell ? */
		if(pack_bytes > sizeof(packet_body) - sizeof(reliable_tail) -
		   sizeof(edge_cell_packet)) {
		  
		    Alarm(DEBUG, "%s%s",
			  "Net_Send_Link_State_All: not enough room in the packet (2)\n",
			  "\t...sending this one and starting a new one\n");
		    
		    /* Close the packet and send it... */
		    ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, LINK_STATE_TYPE);    
		    Alarm(DEBUG, "LINK STATE sent %d bytes; header: %d; Pkt: %d\n", 
			  ret, sizeof(packet_header), pack_bytes);

		    dec_ref_cnt(buff);
		    
		    /* Initiate a new packet and continue from there on */
		    if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
			Alarm(EXIT, "Net_Send_Link_State_all(): Cannot allocte pack_body object\n");
		    }
		    

		    /* Initialize the data structure pointers */
		    pkt = (link_state_packet*)buff;
		    pkt->source = tmp_edge_p->source->address;
		    pkt->num_edges = 0;
		    
		    pack_bytes = sizeof(link_state_packet);
		}

		edge_cell = (edge_cell_packet*)(buff+pack_bytes);
		
		edge_cell->dest = tmp_edge_p->dest->address;
		edge_cell->timestamp_sec  = tmp_edge_p->timestamp_sec;
		edge_cell->timestamp_usec = tmp_edge_p->timestamp_usec;
		edge_cell->cost = tmp_edge_p->cost;

		pack_bytes += sizeof(edge_cell_packet);

		pkt->num_edges++;
	        stdhash_it_keyed_next(&tmp_it);	    

		Alarm(DEBUG, "Packing another edge: %d.%d.%d.%d -> %d.%d.%d.%d :: %d | %d:%d\n", 
		      IP1(pkt->source), IP2(pkt->source), 
		      IP3(pkt->source), IP4(pkt->source),
		      IP1(edge_cell->dest), IP2(edge_cell->dest), 
		      IP3(edge_cell->dest), IP4(edge_cell->dest),
		      edge_cell->cost, edge_cell->timestamp_sec, edge_cell->timestamp_usec); 
	    }
	    
	    /* record this source, so we don't consider it again */

	    stdhash_insert(&sources, &src_it, &src_address, NULL);
	}

        stdhash_it_next(&it); 
    }
    
    ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, LINK_STATE_TYPE);
    Alarm(DEBUG, "LINK STATE sent %d bytes; header: %d; Pkt: %d\n", ret, 
	  sizeof(packet_header), pack_bytes);
    
    /* Dispose the allocated memory for the packet_body buffer and 'destinations' hash */
    dec_ref_cnt(buff);
    stdhash_destruct(&sources);
}


/***********************************************************/
/* void Send_Link_Updates(int dummy_int, void* dummy)      */
/*                                                         */
/* Called by the event system                              */
/* Sends the new updates to all the neighbors              */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* Not used                                                */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Send_Link_Updates (int dummy_int, void* dummy)
{
    int16 i;
    
    Alarm(DEBUG, "\nSend_Link_Updates()\n\n");

    for(i=0; i<Num_Neighbors; i++) {
        if(Neighbor_Nodes[i] != NULL) {
	    if(Neighbor_Nodes[i]->flags & CONNECTED_NODE) {
		Net_Send_Link_State_Updates(&Changed_Edges, i);
	    }
	}
    }
    Empty_Changed_Updates();
}



/***********************************************************/
/* void Net_Send_Link_State_Updates(stdhash* hash_struct,  */
/*                                  int16 node_id)         */
/*                                                         */
/* Sends the new link state updates to a neighbour         */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* hs_struct: hash structure containing the state matrix   */
/* node_id:   id of the neighbour                          */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Net_Send_Link_State_Updates(stdhash *hash_struct, int16 node_id) 
{
    char   *buff;
    link_state_packet *pkt;
    edge_cell_packet *edge_cell;
    stdhash_it it, src_it, tmp_it;
    stdhash  sources;
    Changed_Edge *cg_edge, *cg_edge_tmp;
    int32 src_address;
    int32u node_mask;
    int16 linkid;
    int ret;
    int pack_bytes = 0;
    int flag = 0;


    Alarm(DEBUG, "=== Net_Send_Link_State_Updates()\n");


    node_mask = 1;
    node_mask = node_mask << node_id%32;

    Alarm(DEBUG, "Send_Updates to node: %d.%d.%d.%d\n",
	  IP1(Neighbor_Nodes[node_id]->address), 
	  IP2(Neighbor_Nodes[node_id]->address), 
	  IP3(Neighbor_Nodes[node_id]->address), 
	  IP4(Neighbor_Nodes[node_id]->address));


    /* See if we have anything to send */
    stdhash_begin(hash_struct, &it); 
    while(!stdhash_it_is_end(&it)) {
        cg_edge = *((Changed_Edge **)stdhash_it_val(&it));
	Alarm(DEBUG, "edge mask: %X node mask: %X\n", 
	      cg_edge->mask[node_id/32], node_mask);
	if(!(cg_edge->mask[node_id/32]&node_mask)) {
	    flag = 1;
	    break;
	}
        stdhash_it_next(&it);
    }
    if(flag == 0)
        return;

    /* See which link connects me to this node */
    linkid = Neighbor_Nodes[node_id]->link[0]->link_id;

    /* Allocating packet_body buffer */
    if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
           Alarm(EXIT, "Net_Send_Link_State_Update(): Cannot allocte pack_body object\n");
    }

    /* 'sources' is a temporary hash where we keep the processed 
     * source nodes */
    stdhash_construct(&sources, sizeof(int32), 0, 
                      stdhash_int_equals, stdhash_int_hashcode);

    /* Create the link state packet */

    /* All the edges */
    stdhash_begin(hash_struct, &it); 
    while(!stdhash_it_is_end(&it)) {
        /* one by one... */
      	cg_edge = *((Changed_Edge **)stdhash_it_val(&it));

	/* did we consider this source ? */
	stdhash_find(&sources, &src_it, &cg_edge->edge->source->address);
	if(stdhash_it_is_end(&src_it)) {
	    /* we didn't, so let's see the edges directed from it */
	    src_address = cg_edge->edge->source->address;
	    stdhash_find(hash_struct, &tmp_it, &src_address);
	    cg_edge_tmp = *((Changed_Edge **)stdhash_it_val(&tmp_it));

	    /* Are we supposed to send this edge to this node ? */
	    if((cg_edge_tmp->mask[node_id/32]&node_mask) != 0) {
	        /* No, we aren't */
	        stdhash_it_keyed_next(&tmp_it);
		while(!stdhash_it_is_end(&tmp_it)) {
		    cg_edge_tmp = *((Changed_Edge **)stdhash_it_val(&tmp_it));
		    if((cg_edge_tmp->mask[node_id/32]&node_mask) == 0) {
			break;
		    }
		    stdhash_it_keyed_next(&tmp_it);
		}
		if(stdhash_it_is_end(&tmp_it)) {
		    /* Ok, this source was useless, we didn't find 
		     * anything for it. Move to the next one. */
		    stdhash_insert(&sources, &src_it, &src_address, NULL);
		    stdhash_it_next(&it); 
		    continue;
		}
	    }

	    /* Do we still have room in the packet for a 
	     * link state packet containing at least a cost cell ? */

	    if(pack_bytes > sizeof(packet_body)  - sizeof(reliable_tail) 
	       - sizeof(link_state_packet) - sizeof(edge_cell_packet)) {
	        
	        Alarm(DEBUG, "%s%s",
		      "Net_Send_Link_State_Updates: not enough room in the packet (1)\n",
		      "\t...sending this one and starting a new one\n");
		
		/* Close the packet and send it... */
		ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, LINK_STATE_TYPE);
		Alarm(DEBUG, "LINK STATE sent %d bytes; header: %d; Pkt: %d\n", 
		      ret, sizeof(packet_header), pack_bytes);

		dec_ref_cnt(buff);
		
		/* Initiate a new packet and continue from there on */
		if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
		    Alarm(EXIT, "Net_Send_Link_State_updates(): Cannot allocte pack_body object\n");
		}
		
		pack_bytes = 0;
	    }

	    /* Initialize the data structure pointers */
	    pkt = (link_state_packet*)(buff+pack_bytes);
	    pkt->source = cg_edge_tmp->edge->source->address;
	    pkt->num_edges = 1;

	    pack_bytes += sizeof(link_state_packet);

	    edge_cell = (edge_cell_packet*)(buff+pack_bytes);
	
	    edge_cell->dest = cg_edge_tmp->edge->dest->address;
	    edge_cell->timestamp_sec  = cg_edge_tmp->edge->timestamp_sec;
	    edge_cell->timestamp_usec = cg_edge_tmp->edge->timestamp_usec;
	    edge_cell->cost   = cg_edge_tmp->edge->cost;
	    
	    pack_bytes += sizeof(edge_cell_packet);
     
	    Alarm(DEBUG, "Update ! Packing edge: %d.%d.%d.%d -> %d.%d.%d.%d :: %d | %d:%d\n", 
		  IP1(pkt->source), IP2(pkt->source), 
		  IP3(pkt->source), IP4(pkt->source),
		  IP1(edge_cell->dest), IP2(edge_cell->dest), 
		  IP3(edge_cell->dest), IP4(edge_cell->dest),
		  edge_cell->cost, edge_cell->timestamp_sec, edge_cell->timestamp_usec); 
	   
	    
	    /* See if we still have other sources to this destination */
	    
	    stdhash_it_keyed_next(&tmp_it);
	    while(!stdhash_it_is_end(&tmp_it)) {
	        cg_edge_tmp = *((Changed_Edge **)stdhash_it_val(&tmp_it));

		/* Are we supposed to send this edge to this node ? */
		if((cg_edge_tmp->mask[node_id/32]&node_mask) != 0) {
		    /* No, we aren't */
		    stdhash_it_keyed_next(&tmp_it);
		    continue;
		}

		/* Do we still have room in the packet for a 
		 * at least a cost cell ? */
		if(pack_bytes > sizeof(packet_body)  - sizeof(reliable_tail) 
		   - sizeof(edge_cell_packet)) {
		  
		    Alarm(DEBUG, "%s%s",
			  "Net_Send_Link_State_Updates: not enough room in the packet (2)\n",
			  "\t...sending this one and starting a new one\n");
		    
		    /* Close the packet and send it... */
		    ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, LINK_STATE_TYPE);
		    Alarm(DEBUG, "LINK STATE sent %d bytes; header: %d; Pkt: %d\n", 
			  ret, sizeof(packet_header), pack_bytes);

		    dec_ref_cnt(buff);
		    
		    /* Initiate a new packet and continue from there on */
		    if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
			Alarm(EXIT, "Net_Send_Link_State_updates(): Cannot allocte pack_body object\n");
		    }


		    /* Initialize the data structure pointers */
		    pkt = (link_state_packet*)buff;
		    pkt->source = cg_edge_tmp->edge->source->address;
		    pkt->num_edges = 0;
		    
		    pack_bytes = sizeof(link_state_packet);
		}

		edge_cell = (edge_cell_packet*)(buff+pack_bytes);
		
		edge_cell->dest = cg_edge_tmp->edge->dest->address;
		edge_cell->timestamp_sec  = cg_edge_tmp->edge->timestamp_sec;
		edge_cell->timestamp_usec = cg_edge_tmp->edge->timestamp_usec;
		edge_cell->cost   = cg_edge_tmp->edge->cost;

		pack_bytes += sizeof(edge_cell_packet);

		pkt->num_edges++;
	        stdhash_it_keyed_next(&tmp_it);	    

		Alarm(DEBUG, "Update ! Packing another edge: %d.%d.%d.%d -> %d.%d.%d.%d :: %d | %d:%d\n", 
		      IP1(pkt->source), IP2(pkt->source), 
		      IP3(pkt->source), IP4(pkt->source),
		      IP1(edge_cell->dest), IP2(edge_cell->dest), 
		      IP3(edge_cell->dest), IP4(edge_cell->dest),
		      edge_cell->cost, edge_cell->timestamp_sec, edge_cell->timestamp_usec); 
	    }
	    
	    /* record this destination, so we don't consider it again */

	    stdhash_insert(&sources, &src_it, &src_address, NULL);
	}

        stdhash_it_next(&it); 
    }  
 
    ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, LINK_STATE_TYPE);
    Alarm(DEBUG, "LINK STATE sent %d bytes; header: %d; Pkt: %d\n", ret, 
	  sizeof(packet_header), pack_bytes);

    /* Dispose the allocated memory for the packet_ody buffer and 'destinations' hash */
    dec_ref_cnt(buff);
    stdhash_destruct(&sources);
}



/***********************************************************/
/* void Process_link_state_packet(int32 sender, char *buf, */
/*                                int16u data_len,         */
/*                                int16u ack_len,          */
/*                                int32u type,             */
/*                                int mode)                */
/*                                                         */
/* Process a link state packet                             */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender:   neighbor that gave me the packet              */
/* buf:      pointer to the message                        */
/* data_len: data length in the packet                     */
/* ack_len:  ack length in the packet                      */
/* type:     the first byte of the message                 */
/* mode:     type of the link the message was received on  */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Process_link_state_packet(int32 sender, char *buf, 
			       int16u data_len, int16u ack_len, 
			       int32u type, int mode)
{
    link_state_packet *pkt;
    edge_cell_packet  *edge_cell;
    stdhash_it it;
    int i;
    Node *dest;
    Node *nd = NULL;
    Node *source;
    Node *sender_node;
    Edge *edge;
    Link *lk;
    Reliable_Data *r_data;
    reliable_tail *r_tail;
    int32 sender_ip = sender;
    int16 linkid, nodeid;
    int processed_bytes = 0;
    int flag, flag_tmp;
    int changed_route_flag = 0;

    /* Check if we knew about the sender of thes message */
    stdhash_find(&All_Nodes, &it, &sender_ip);
    if(stdhash_it_is_end(&it)) { /* I had no idea about the sender node */
	/* This guy should first send hello messages to setup 
	   the link, etc. The only reason for getting this link state update 
	   is that I crashed, recovered, and the sender didn't even notice. 
	   Create the node as it seems that it will be a neighbor in the 
	   future. Still, for now I will just make it remote. Hello protocol
	   will take care to make it a neighbor if needed */
	
	Create_Node(sender, REMOTE_NODE);
	stdhash_find(&All_Nodes, &it, &sender_ip);
    }

    /* Take care about the reliability stuff. First, get the other side node */
    sender_node = *((Node **)stdhash_it_val(&it));
    
    /* See if we have a valid link to this guy, and take care about the acks.
     * If we don't have, ignore the acks and just consider the data in the packet,
     * it might be useful */
    if(sender_node->node_id >= 0) {
	/* Ok, this node is a neighbor. Let's see the link to it */
        Alarm(DEBUG, "Neighbor node\n");

	lk = sender_node->link[CONTROL_LINK];
	if(lk->r_data == NULL)
	    Alarm(EXIT, "Process_link_state_packet(): Not a reliable link\n");
	r_data = lk->r_data;

	if(r_data->flags&CONNECTED_LINK) {

	    /* Get the reliable tail from the packet */
	    r_tail = (reliable_tail*)(buf+data_len);

	    /* Process the ack part. 
	     * If the packet is not needed (we processed it earlier) just return */ 
	    flag = Process_Ack(lk->link_id, (char*)r_tail, ack_len, type);
	
	    Alarm(DEBUG, "flag: %d\n", flag);

	    if(flag == -1) { /* Ack packet... */
		Alarm(PRINT, "Warning !!! Ack packets should be treated differently !\n");
		return;
	    }

	    /* We should send an acknowledge for this message. So, let's schedule it */
	    r_data->scheduled_ack = 1;
	    E_queue(Send_Ack, (int)lk->link_id, NULL, zero_timeout);
	
	    if(flag == 0)
		return;

	}
	else {
	    /* Got a reliable packet from an existing link that is not 
	     * connected yet. This is because the other guy got my hello msgs,
	     * validated the link, but I lost his hello msgs, therefore on my
	     * part, the link is not available yet. */ 
	    /* Send a request for a hello message */

	    E_queue(Send_Hello_Request, (int)lk->link_id, NULL, zero_timeout);
	    
	}
    }

    /* Process the data in the packet */

    while(processed_bytes < data_len) {
	pkt = (link_state_packet*)(buf+processed_bytes);
	if(!Same_endian(type)) 
	    Flip_link_state_pack(pkt);

	/* Check if we knew about this destination node */
	stdhash_find(&All_Nodes, &it, &pkt->source);
	if(stdhash_it_is_end(&it)) { /* I had no idea about this node */
	    Create_Node(pkt->source, REMOTE_NODE);
	    stdhash_find(&All_Nodes, &it, &pkt->source);
	}
        source = *((Node **)stdhash_it_val(&it));
	
	processed_bytes += sizeof(link_state_packet);
	if(processed_bytes > data_len) {
	    Alarm(EXIT, "Process_link_state_packet(): processed: %d > data: %d\n",
		  processed_bytes, data_len);
	}
	for(i=0; i< pkt->num_edges; i++) {
	    if(processed_bytes + sizeof(edge_cell_packet) > data_len) {
	        Alarm(EXIT, "Process_link_state_packet(): num: %d; proc: %d; data: %d\n",
		      pkt->num_edges, processed_bytes, data_len);
	    }
	    edge_cell = (edge_cell_packet *)(buf+processed_bytes);
	    if(!Same_endian(type))
	        Flip_edge_cell_pack(edge_cell);
	    
	    /* Check if we knew about this source node */
	    stdhash_find(&All_Nodes, &it, &edge_cell->dest);
	    if(stdhash_it_is_end(&it)) { /* I had no idea about this node */
	        Create_Node(edge_cell->dest, REMOTE_NODE);
	        stdhash_find(&All_Nodes, &it, &edge_cell->dest);
	    }
	    dest = *((Node **)stdhash_it_val(&it));



	    if((edge_cell->cost < 0)&&(source->address == My_Address)) {
		/* somebody tells me that an edge of mine is deleted. */

		Alarm(DEBUG, "Hey, this is my edge !!!\n");
		processed_bytes += sizeof(edge_cell_packet);
		continue;
	    }
	    
	    /* Did I know about this edge ? */
	    if((edge = Find_Edge(&All_Edges, source->address, 
				 dest->address, 0)) == NULL) {
		/* This is about an edge that I didn't know about until 
		 * now... deleted or not. */ 

		edge = Create_Overlay_Edge(source->address, dest->address);
		edge->timestamp_sec = 0;
		edge->timestamp_usec = 0;
	    }

	    Alarm(DEBUG, "\nGot edge: %d.%d.%d.%d -> %d.%d.%d.%d\n",
		      IP1(edge->source->address), IP2(edge->source->address), 
		      IP3(edge->source->address), IP4(edge->source->address), 
		      IP1(edge->dest->address), IP2(edge->dest->address), 
		      IP3(edge->dest->address), IP4(edge->dest->address));
	    
	    
	    Alarm(DEBUG, "Got: %d : %d ||| mine: %d : %d\n", 
		  edge_cell->timestamp_sec, edge_cell->timestamp_usec,
		  edge->timestamp_sec, edge->timestamp_usec);

	    /* Check if the edge is mine, and if so, check if I have a link
	     * with this edge. It is possible that smbd. just told me about 
	     * a new edge of mine, I created it, I don't have a link with 
	     * it yet */
	    

	    flag_tmp = 0;
	    if(source->address == My_Address) {
	        nd = dest;
		flag_tmp = 1;
	    }
	    if(dest->address == My_Address) {
	        nd = source;
		flag_tmp = 1;
	    }

	    if(flag_tmp == 1) {
	        if(nd->link[CONTROL_LINK] == NULL) {
		    nd->flags = NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE;	      	  
		    nd->last_time_heard = E_get_time();
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


	    /* Update edge structure here... */
	    if((edge->timestamp_sec < edge_cell->timestamp_sec)||
	       ((edge->timestamp_sec == edge_cell->timestamp_sec)&&
		(edge->timestamp_usec < edge_cell->timestamp_usec))) {
	        Alarm(DEBUG, "Updating edge: %d.%d.%d.%d -> %d.%d.%d.%d\n",
		      IP1(edge->source->address), IP2(edge->source->address), 
		      IP3(edge->source->address), IP4(edge->source->address), 
		      IP1(edge->dest->address), IP2(edge->dest->address), 
		      IP3(edge->dest->address), IP4(edge->dest->address));

		edge->timestamp_sec = edge_cell->timestamp_sec;
		edge->timestamp_usec = edge_cell->timestamp_usec;
		edge->cost = edge_cell->cost;
		
		Add_to_changed_edges(sender, edge, NEW_CHANGE);
		changed_route_flag = 1;
	    }
	    else if((edge->timestamp_sec == edge_cell->timestamp_sec)&&
		    (edge->timestamp_usec == edge_cell->timestamp_usec)) {
		Add_to_changed_edges(sender, edge, OLD_CHANGE);
	    }




	    processed_bytes += sizeof(edge_cell_packet);
	}	
    }
    if(changed_route_flag == 1)
	Set_Routes();
}


/***********************************************************/
/* void Resend_Link_States(int dummy_int, void* dummy)     */
/*                                                         */
/* Called by the event system                              */
/* Resends the valid links to all the neighbors            */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* Not used                                                */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Resend_Link_States (int dummy_int, void* dummy)
{
    int16 i;
    Node *nd;
    Edge *edge;
    sp_time now, diff, edge_time;

    
    now = E_get_time();

    for(i=0; i<Num_Neighbors; i++) {
        if(Neighbor_Nodes[i] != NULL) {
	    if(Neighbor_Nodes[i]->flags & CONNECTED_NODE) {
		
		nd = Neighbor_Nodes[i];
		edge = Find_Edge(&All_Edges, My_Address, nd->address, 0);
		
		Alarm(DEBUG, "edge_cost: %d\n", edge->cost);

		edge_time.sec  = edge->timestamp_sec;
		edge_time.usec = edge->timestamp_usec;
		diff = E_sub_time(now, edge_time);

		Alarm(DEBUG, "now : %d, %d\n", now.sec, now.usec);
		Alarm(DEBUG, "edge: %d, %d\n", edge_time.sec, edge_time.usec);
		Alarm(DEBUG, "diff: %d, %d\n", diff.sec, diff.usec);
	
		if(E_compare_time(diff, edge_resend_time) >= 0) {
		    Alarm(DEBUG, "Resend_Updating edge: %d.%d.%d.%d -> %d.%d.%d.%d\n",
			  IP1(edge->source->address), IP2(edge->source->address), 
			  IP3(edge->source->address), IP4(edge->source->address), 
			  IP1(edge->dest->address), IP2(edge->dest->address), 
			  IP3(edge->dest->address), IP4(edge->dest->address));

		    edge->timestamp_sec = now.sec;
		    edge->timestamp_usec = now.usec;
		    Add_to_changed_edges(My_Address, edge, NEW_CHANGE);
		}
	    }
	}
    }
    E_queue(Resend_Link_States, 0, NULL, resend_call_timeout);
}



/***********************************************************/
/* void Garbage_Collector(int dummy_int, void* dummy)      */
/*                                                         */
/* Called by the event system                              */
/* Remove the unnecessary edges/nodes from memory          */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* Not used                                                */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Garbage_Collector(int dummy_int, void* dummy)
{
    stdhash_it it;
    Edge *edge;
    sp_time now, diff, edge_time;
    int flag;

    Alarm(DEBUG, "Garbage_Collector()\n");

    now = E_get_time();

    /* All the edges */

    stdhash_begin(&All_Edges, &it); 
    while(!stdhash_it_is_end(&it)) {
        /* one by one... */
      	edge = *((Edge **)stdhash_it_val(&it));

	flag = 0;
	edge_time.sec  = edge->my_timestamp_sec;
	edge_time.usec = edge->my_timestamp_usec;
	
	Alarm(DEBUG, "now : %d, %d\n", now.sec, now.usec);
	Alarm(DEBUG, "edge: %d, %d\n", edge_time.sec, edge_time.usec);
	
	
	diff = E_sub_time(now, edge_time);
	
	Alarm(DEBUG, "diff: %d, %d\n", diff.sec, diff.usec);
	
	if(E_compare_time(diff, gb_collect_remove) >= 0) {
	    
	    Alarm(PRINT, "Garbage_Collector() -- delete edge\n");
	    Alarm(PRINT, "DELETING edge: %d.%d.%d.%d -> %d.%d.%d.%d\n",
		  IP1(edge->source->address), IP2(edge->source->address), 
		  IP3(edge->source->address), IP4(edge->source->address), 
		  IP1(edge->dest->address), IP2(edge->dest->address), 
		  IP3(edge->dest->address), IP4(edge->dest->address));

	    
	    
	    /* Ok, this is a very old edge. It should be removed... */
	    stdhash_erase(&it);
	    
	    /* See if we need to delete the Nodes of the edge also */
	    if(Try_Remove_Node(edge->source->address) < 0)
		Alarm(EXIT, "Garbage_Collector(): Error removing node\n");
	    if(Try_Remove_Node(edge->dest->address) < 0)
		Alarm(EXIT, "Garbage_Collector(): Error removing node\n");
	    
	    dispose(edge);
	    
	    continue;
	}
	stdhash_it_next(&it); 
    }
    E_queue(Garbage_Collector, 0, NULL, gb_collect_timeout);
}
