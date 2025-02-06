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

#include <string.h>
#include <stdlib.h>

#ifdef ARCH_PC_WIN95
#include <winsock2.h>
#endif

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
#include "reliable_datagram.h"
#include "state_flood.h"
#include "link_state.h"
#include "hello.h"
#include "protocol.h"
#include "route.h"
#include "multicast.h"
#include "kernel_routing.h"


extern Node* Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
extern stdhash   All_Nodes;
extern int16 Num_Neighbors;
extern Link* Links[MAX_LINKS];
extern int32 My_Address;
extern Prot_Def Edge_Prot_Def;
extern Prot_Def Groups_Prot_Def;
extern int Schedule_Set_Route;
extern int Wireless;
extern int16 KR_Flags;


/* Local variables */
static const sp_time zero_timeout        = {     0,    0};
static const sp_time short_timeout       = {     0,    5000};  /* 5 milliseconds */
static const sp_time wireless_timeout    = {     0,    15000};  /* 15 milliseconds */
static const sp_time kr_timeout          = {     0,    20000};
static const sp_time state_resend_time   = { 30000,    0};     /* 8 h 20 mins */
static const sp_time resend_call_timeout = {  3000,    0}; 
static const sp_time resend_fast_timeout = {     1,    0};     /* one second */
static const sp_time gb_collect_remove   = { 90000,    0};     /* about one day */
static const sp_time gb_collect_timeout  = { 10000,    0}; 



void Flip_state_cell(State_Cell *s_cell)
{
    s_cell->dest           = Flip_int32(s_cell->dest);
    s_cell->timestamp_sec  = Flip_int32(s_cell->timestamp_sec);
    s_cell->timestamp_usec = Flip_int32(s_cell->timestamp_usec);
    s_cell->value          = Flip_int16(s_cell->value);
    s_cell->age            = Flip_int16(s_cell->age);
}




/***********************************************************/
/* void Net_Send_State_All(int lk_id, void *p_data)        */
/*                                                         */
/* Sends the entire state to a neighbor                    */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* lk_id:  link to the neighbor                            */
/* p_func: pointer to the structure of funct. pointers     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Net_Send_State_All(int lk_id, void *p_data) 
{
    int16 linkid;
    Prot_Def *p_def;
    char   *buff;
    State_Packet *pkt;
    State_Cell *state_cell;
    State_Chain *s_chain;
    stdit it, src_it;
    State_Data *s_data;
    int ret;
    int pack_bytes = 0;
    sp_time now;

    linkid = (int16)lk_id;
    p_def = (Prot_Def*)p_data;

    /* See if we have anything to send */
    stdhash_begin(p_def->All_States(), &src_it); 
    if(stdhash_is_end(p_def->All_States(), &src_it)) 
        return;

    now = E_get_time();

    /* Allocating packet_body for the update */
    if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
           Alarm(EXIT, "Net_Send_State_all(): Cannot allocte pack_body object\n");
    }


    /* Create the state packet */

    /* All the sources */
    while(!stdhash_is_end(p_def->All_States(), &src_it)) {
	/* source by source */
	s_chain = *((State_Chain **)stdhash_it_val(&src_it));
	stdhash_begin(&s_chain->states, &it); 
	while(!stdhash_is_end(&s_chain->states, &it)) {
	    /* one by one, the destinations... */
	    s_data = *((State_Data **)stdhash_it_val(&it));
	    
	    /* Do we still have room in the packet for a 
	     * state containing at least one cell ? */
	    
	    if(pack_bytes > (int)(sizeof(packet_body) - sizeof(reliable_tail) -
	       p_def->State_header_size() - p_def->Cell_packet_size())) {
	        
	        Alarm(DEBUG, "%s%s",
		      "Net_Send_State_All: not enough room in the packet (1)\n",
		      "\t...sending this one and starting a new one\n");

		/* Close the packet and send it... */
		ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, p_def->State_type());
		Alarm(DEBUG, "STATE sent %d bytes; header: %d; Pkt: %d\n", 
		      ret, sizeof(packet_header), pack_bytes);
		dec_ref_cnt(buff);

		/* Initiate a new packet and continue from there on */
		if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
		    Alarm(EXIT, "Net_Send_State_all(): Cannot allocte pack_body object\n");
		}
		pack_bytes = 0;
	    }

	    /* Initialize the data structure pointers */
	    pkt = (State_Packet*)(buff+pack_bytes);
	    pkt->source = s_data->source_addr;
	    pkt->num_cells = 1;
	    pack_bytes += sizeof(State_Packet);
	    /* Add anything specific to the protocol */
	    pack_bytes += p_def->Set_state_header((void*)s_data, buff+pack_bytes);

	    state_cell = (State_Cell*)(buff+pack_bytes);
	    state_cell->dest = s_data->dest_addr;
	    state_cell->timestamp_sec  = s_data->timestamp_sec;
	    state_cell->timestamp_usec = s_data->timestamp_usec;
	    state_cell->age = s_data->age + (now.sec - s_data->my_timestamp_sec)/10;
	    state_cell->value = s_data->value; 
	    pack_bytes += sizeof(State_Cell);
	    /* Add anything specific to the protocol */
	    pack_bytes += p_def->Set_state_cell((void*)s_data, buff+pack_bytes);     

	    Alarm(DEBUG, "All: Packing state: %d.%d.%d.%d -> %d.%d.%d.%d | %d:%d\n", 
		  IP1(pkt->source), IP2(pkt->source), 
		  IP3(pkt->source), IP4(pkt->source),
		  IP1(state_cell->dest), IP2(state_cell->dest), 
		  IP3(state_cell->dest), IP4(state_cell->dest),
		  state_cell->timestamp_sec, state_cell->timestamp_usec); 
	   

	    /* See if we still have other states for this source */
	    
	    stdhash_it_next(&it);
	    while(!stdhash_is_end(&s_chain->states, &it)) {
	        s_data = *((State_Data **)stdhash_it_val(&it));

		/* Do we still have room in the packet for a 
		 * at least a state cell ? */
		if(pack_bytes > (int)(sizeof(packet_body) - sizeof(reliable_tail) -
		   p_def->Cell_packet_size())) {
		    
		    Alarm(DEBUG, "%s%s",
			  "Net_Send_State_All: not enough room in the packet (2)\n",
			  "\t...sending this one and starting a new one\n");
		    
		    /* Close the packet and send it... */
		    ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, p_def->State_type());    
		    Alarm(DEBUG, "STATE sent %d bytes; header: %d; Pkt: %d\n", 
			  ret, sizeof(packet_header), pack_bytes);

		    dec_ref_cnt(buff);
		    
		    /* Initiate a new packet and continue from there on */
		    if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
			Alarm(EXIT, "Net_Send_State_all(): Cannot allocte pack_body object\n");
		    }


		    /* Initialize the data structure pointers */
		    pkt = (State_Packet*)buff;
		    pkt->source = s_data->source_addr;
		    pkt->num_cells = 0;		    
		    pack_bytes = sizeof(State_Packet);
		    /* Add anything specific to the protocol */
		    pack_bytes += p_def->Set_state_header(s_data, buff+pack_bytes);
		}

		pkt->num_cells++;
		state_cell = (State_Cell*)(buff+pack_bytes);
		state_cell->dest = s_data->dest_addr;
		state_cell->timestamp_sec  = s_data->timestamp_sec;
		state_cell->timestamp_usec = s_data->timestamp_usec;
		state_cell->age = s_data->age + (now.sec - s_data->my_timestamp_sec)/10;
		state_cell->value = s_data->value; 
		pack_bytes += sizeof(State_Cell);
		/* Add anything specific to the protocol */
		pack_bytes += p_def->Set_state_cell(s_data, buff+pack_bytes);  
	    
		stdhash_it_next(&it);	

		Alarm(DEBUG, "All: Packing another state: %d.%d.%d.%d -> %d.%d.%d.%d | %d:%d\n", 
		      IP1(pkt->source), IP2(pkt->source), 
		      IP3(pkt->source), IP4(pkt->source),
		      IP1(state_cell->dest), IP2(state_cell->dest), 
		      IP3(state_cell->dest), IP4(state_cell->dest),
		      state_cell->timestamp_sec, state_cell->timestamp_usec); 
	    }
	}

        stdhash_it_next(&src_it); 
    }

    ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, p_def->State_type());
    Alarm(DEBUG, "STATE sent %d bytes; header: %d; Pkt: %d\n", ret, 
	  sizeof(packet_header), pack_bytes);
    
    /* Dispose the allocated memory for the packet_body buffer */
    dec_ref_cnt(buff);
}




/***********************************************************/
/* void Send_State_Updates(int dummy_int, void* p_data)    */
/*                                                         */
/* Called by the event system                              */
/* Sends the new state updates to all the neighbors        */
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

void Send_State_Updates (int dummy_int, void* p_data)
{
    int16 i;
    Prot_Def *p_def;
    int flag = 0;


    p_def = (Prot_Def*)p_data;
   
    Alarm(DEBUG, "\nSend_State_Updates()\n\n");

    for(i=0; i<Num_Neighbors; i++) {
        if(Neighbor_Nodes[i] != NULL) {
	    if(Neighbor_Nodes[i]->flags & CONNECTED_NODE) {
		if(Net_Send_State_Updates(p_def, i) < 0)
		    flag = 1;
	    }
	}
    }
    if(flag == 1) {
	E_queue(Send_State_Updates, 0, p_data, zero_timeout);
    } 
    else {
	Empty_Changed_States(p_def->Changed_States());
    }
}



/***********************************************************/
/* int Net_Send_State_Updates(Prot_Def *p_def,             */
/*                                  int16 node_id)         */
/*                                                         */
/* Sends the new state updates to a neighbor               */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* p_def: structure with the protocol function pointers    */
/* node_id:   id of the neighbour                          */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* int  1 if we sent everything                            */
/*     -1 if we could not send all the updates (too many)  */
/*                                                         */
/***********************************************************/

int Net_Send_State_Updates(Prot_Def *p_def, int16 node_id) 
{
    char   *buff;
    State_Packet *pkt;
    State_Cell *state_cell;
    stdhash *hash_struct;
    int32u node_mask;
    stdit it, src_it, tmp_it;
    Changed_State *cg_state, *cg_state_tmp, *cg_state_src;
    int16 linkid;
    stdhash  sources;
    State_Data *s_data, *tmp_data;
    sp_time now;
    int ret;
    int pack_bytes = 0;
    int flag = 0;
    int pkt_cnt = 0;



    Alarm(DEBUG, "=== Net_Send_State_Updates()\n");

    Alarm(DEBUG, "Send_Updates to node: %d.%d.%d.%d\n",
	  IP1(Neighbor_Nodes[node_id]->address), 
	  IP2(Neighbor_Nodes[node_id]->address), 
	  IP3(Neighbor_Nodes[node_id]->address), 
	  IP4(Neighbor_Nodes[node_id]->address));


    hash_struct = p_def->Changed_States();
    node_mask = 1;
    node_mask = node_mask << node_id%32;

    /* See if we have anything to send */
    stdhash_begin(hash_struct, &it); 
    while(!stdhash_is_end(hash_struct, &it)) {
        cg_state = *((Changed_State **)stdhash_it_val(&it));
	Alarm(DEBUG, "state mask: %X node mask: %X\n", 
	      cg_state->mask[node_id/32], node_mask);
	if(!(cg_state->mask[node_id/32]&node_mask)) {
	    flag = 1;
	    break;
	}
        stdhash_it_next(&it);
    }
    if(flag == 0)
        return(1);

    now = E_get_time();

    /* See which control link connects me to this node */
    if(Neighbor_Nodes[node_id]->link[CONTROL_LINK] == NULL)
	return(1);
    linkid = Neighbor_Nodes[node_id]->link[CONTROL_LINK]->link_id;

    /* Allocating packet_body buffer */
    if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
           Alarm(EXIT, "Net_Send_State_Update(): Cannot allocte pack_body object\n");
    }

    /* 'sources' is a temporary hash where we keep the processed 
     * source nodes */
    stdhash_construct(&sources, sizeof(int32), 0, NULL, NULL, 0); 

    /* Create the state packet */

    /* All the states */
    stdhash_begin(hash_struct, &it); 
    while(!stdhash_is_end(hash_struct, &it)) {
        /* one by one... */
	cg_state_src = *((Changed_State **)stdhash_it_val(&it));
	tmp_data = (State_Data*)cg_state_src->state;

	Alarm(DEBUG, "!!!!!! : %d.%d.%d.%d\n", 
	  IP1(tmp_data->source_addr), 
	  IP2(tmp_data->source_addr), 
	  IP3(tmp_data->source_addr), 
	  IP4(tmp_data->source_addr));


	/* did we consider this source ? */
	stdhash_find(&sources, &src_it, &tmp_data->source_addr);
	if(stdhash_is_end(&sources, &src_it)) {
	    /* we didn't, so let's see the states originating from it */
	    stdhash_find(hash_struct, &tmp_it, &tmp_data->source_addr);
	    cg_state_tmp = *((Changed_State **)stdhash_it_val(&tmp_it));


	    Alarm(DEBUG, "%X\t%X\n", cg_state_tmp->mask[node_id/32], node_mask);

	    /* Are we supposed to send this state to this node ? */
	    if((cg_state_tmp->mask[node_id/32]&node_mask) != 0) {
	        /* No, we aren't */
		Alarm(DEBUG, "Identical mask\n");
	        stdhash_keyed_next(hash_struct, &tmp_it);
		while(!stdhash_is_end(hash_struct, &tmp_it)) {
		    cg_state_tmp = *((Changed_State **)stdhash_it_val(&tmp_it));
		    if((cg_state_tmp->mask[node_id/32]&node_mask) == 0) {
			break;
		    }
		    stdhash_keyed_next(hash_struct, &tmp_it);
		}
		if(stdhash_is_end(hash_struct, &tmp_it)) {
		    /* Ok, this source was useless, we didn't find 
		     * anything for it. Move to the next one. */
		    stdhash_insert(&sources, &src_it, &tmp_data->source_addr, NULL);
		    stdhash_it_next(&it); 
		    continue;
		}
	    }
	    
	    cg_state_tmp->mask[node_id/32] = cg_state_tmp->mask[node_id/32] | node_mask;

	    s_data = (State_Data*)cg_state_tmp->state;
	    if(s_data == NULL) {
		Alarm(EXIT, "BUG !!! Changed state with no state\n");
	    }

	    /* Do we still have room in the packet for a 
	     * state containing at least one cell ? */

	    if(pack_bytes > (int)(sizeof(packet_body)  - sizeof(reliable_tail) -
	       p_def->State_header_size() - p_def->Cell_packet_size())) {
	        
	        Alarm(DEBUG, "%s%s",
		      "Net_Send_State_Updates: not enough room in the packet (1)\n",
		      "\t...sending this one and starting a new one\n");
		
		/* Close the packet and send it... */
		ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, p_def->State_type());
		Alarm(DEBUG, "STATE sent %d bytes; header: %d; Pkt: %d\n", 
		      ret, sizeof(packet_header), pack_bytes);

		dec_ref_cnt(buff);
		
		pkt_cnt++;

		if(pkt_cnt > 5) {
		    stdhash_destruct(&sources);
		    return(-1);
		}

		/* Initiate a new packet and continue from there on */
		if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
		    Alarm(EXIT, "Net_Send_State_updates(): Cannot allocte pack_body object\n");
		}

		pack_bytes = 0;
	    }

	    /* Initialize the data structure pointers */
	    pkt = (State_Packet*)(buff+pack_bytes);
	    pkt->source = s_data->source_addr;
	    pkt->num_cells = 1;
	    pack_bytes += sizeof(State_Packet);
	    /* Add anything specific to the protocol */
	    pack_bytes += p_def->Set_state_header((void*)s_data, buff+pack_bytes);

	    state_cell = (State_Cell*)(buff+pack_bytes);
	    state_cell->dest = s_data->dest_addr;
	    state_cell->timestamp_sec  = s_data->timestamp_sec;
	    state_cell->timestamp_usec = s_data->timestamp_usec;
	    state_cell->age = s_data->age + (now.sec - s_data->my_timestamp_sec)/10;
	    state_cell->value = s_data->value; 

	    pack_bytes += sizeof(State_Cell);
	    /* Add anything specific to the protocol */
	    pack_bytes += p_def->Set_state_cell((void*)s_data, buff+pack_bytes);     

	    Alarm(DEBUG, "Upd: Packing state: %d.%d.%d.%d -> %d.%d.%d.%d | %d:%d\n", 
		  IP1(pkt->source), IP2(pkt->source), 
		  IP3(pkt->source), IP4(pkt->source),
		  IP1(state_cell->dest), IP2(state_cell->dest), 
		  IP3(state_cell->dest), IP4(state_cell->dest),
		  state_cell->timestamp_sec, state_cell->timestamp_usec); 
	   
	    /* See if we still have other states for this source */
	    
	    stdhash_keyed_next(hash_struct, &tmp_it);
	    while(!stdhash_is_end(hash_struct, &tmp_it)) {
	        cg_state_tmp = *((Changed_State **)stdhash_it_val(&tmp_it));

		Alarm(DEBUG, "%X\t%X\n", cg_state_tmp->mask[node_id/32], node_mask);
	    
		/* Are we supposed to send this state to this node ? */
		if((cg_state_tmp->mask[node_id/32]&node_mask) != 0) {
		    /* No, we aren't */
		    Alarm(DEBUG, "Identical mask\n");
		    stdhash_keyed_next(hash_struct, &tmp_it);
		    continue;
		}
		
		cg_state_tmp->mask[node_id/32] = cg_state_tmp->mask[node_id/32] | node_mask;

		s_data = (State_Data*)cg_state_tmp->state;
		if(s_data == NULL) {
		    Alarm(EXIT, "BUG !!! Changed state with no state\n");
		}
		/* Do we still have room in the packet for a 
		 * at least a state cell ? */
		if(pack_bytes > (int)(sizeof(packet_body)  - sizeof(reliable_tail) 
		   - p_def->Cell_packet_size())) {
		  
		    Alarm(DEBUG, "%s%s",
			  "Net_Send_State_Updates: not enough room in the packet (2)\n",
			  "\t...sending this one and starting a new one\n");
		    
		    /* Close the packet and send it... */
		    ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, p_def->State_type());
		    Alarm(DEBUG, "STATE sent %d bytes; header: %d; Pkt: %d\n", 
			  ret, sizeof(packet_header), pack_bytes);

		    dec_ref_cnt(buff);

		    pkt_cnt++;

		    if(pkt_cnt > 5) {
			stdhash_destruct(&sources);
			return(-1);
		    }

		    /* Initialize the data structure pointers */
		    if((buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
			Alarm(EXIT, "Net_Send_State_updates(): Cannot allocte pack_body object\n");
		    }


		    /* Initialize the data structure pointers */
		    pkt = (State_Packet*)buff;
		    pkt->source = s_data->source_addr;
		    pkt->num_cells = 0;		    
		    pack_bytes = sizeof(State_Packet);
		    /* Add anything specific to the protocol */
		    pack_bytes += p_def->Set_state_header(s_data, buff+pack_bytes);
		}

		pkt->num_cells++;
		state_cell = (State_Cell*)(buff+pack_bytes);
		state_cell->dest = s_data->dest_addr;
		state_cell->timestamp_sec  = s_data->timestamp_sec;
		state_cell->timestamp_usec = s_data->timestamp_usec;
		state_cell->age = s_data->age + (now.sec - s_data->my_timestamp_sec)/10;
		state_cell->value = s_data->value; 

		pack_bytes += sizeof(State_Cell);
		/* Add anything specific to the protocol */
		pack_bytes += p_def->Set_state_cell(s_data, buff+pack_bytes);  
	    
		stdhash_keyed_next(hash_struct, &tmp_it);	

		Alarm(DEBUG, "Upd: Packing another state: %d.%d.%d.%d -> %d.%d.%d.%d | %d:%d\n", 
		      IP1(pkt->source), IP2(pkt->source), 
		      IP3(pkt->source), IP4(pkt->source),
		      IP1(state_cell->dest), IP2(state_cell->dest), 
		      IP3(state_cell->dest), IP4(state_cell->dest),
		      state_cell->timestamp_sec, state_cell->timestamp_usec); 
	    }

	    /* record this source, so we don't consider it again */
	    stdhash_insert(&sources, &src_it, &tmp_data->source_addr, NULL);
	}

        stdhash_it_next(&it); 
    }  
 
    ret = Reliable_Send_Msg(linkid, buff, (int16u)pack_bytes, p_def->State_type());
    Alarm(DEBUG, "STATE sent %d bytes; header: %d; Pkt: %d\n", ret, 
	  sizeof(packet_header), pack_bytes);

    /* Dispose the allocated memory for the packet_body buffer and 'sources' hash */
    dec_ref_cnt(buff);

    stdhash_destruct(&sources);
    
    return(1);
}



/***********************************************************/
/* void Process_state_packet(int32 sender, char *buf,      */
/*                           int16u data_len,              */
/*                           int16u ack_len,               */
/*                           int32u type,                  */
/*                           int mode)                     */
/*                                                         */
/* Process a state flood packet                            */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender:   neighbor that gave me the packet              */
/* buf:      pointer to the message                        */
/* data_len: data length in the packet                     */
/* ack_len:  ack length in the packet                      */
/* type:     the first four bytes of the message           */
/* mode:     type of the link the message was received on  */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Process_state_packet(int32 sender, char *buf, 
			  int16u data_len, int16u ack_len, 
			  int32u type, int mode)
{
    Prot_Def *p_def;
    State_Packet *pkt;
    State_Cell *state_cell;
    Node *sender_node;
    stdit it;
    Link *lk;
    State_Data *s_data;
    int32 sender_ip = sender;
    Reliable_Data *r_data;
    reliable_tail *r_tail;
    int processed_bytes = 0;
    int i, flag;
    int changed_route_flag = 0;
    int my_endianess_type;
   

    /* Check if we knew about the sender of this message */
    stdhash_find(&All_Nodes, &it, &sender_ip);
    if(stdhash_is_end(&All_Nodes, &it)) { /* I had no idea about the sender node */
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

	lk = sender_node->link[CONTROL_LINK];
	if(lk->r_data == NULL) {
	    Alarm(PRINT, "Process_state_packet(): CONTROL link not reliable\n");
	    return;
	}
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
            if (!E_in_queue(Send_Ack, (int)lk->link_id, NULL)) {
	        r_data->scheduled_ack = 1;
	        E_queue(Send_Ack, (int)lk->link_id, NULL, short_timeout);
            }
            /* old version
	    r_data->scheduled_ack = 1;
	    E_queue(Send_Ack, (int)lk->link_id, NULL, zero_timeout);
            */
	
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
	    return;
	}
    }
    else {
	/* Got a reliable message from a node that is not my neighbor. 
	 * Can not do anything, so just ignore the packet */
	return;
    }
    
    if(!Same_endian(type)) {
	my_endianess_type = Flip_int32(type);
    }
    else {
	my_endianess_type = type;
    }

    /* Get the protocol function pointers */
    p_def = Get_Prot_Def(my_endianess_type);
    
    /* Process the data in the packet */

    while(processed_bytes < data_len) {
	pkt = (State_Packet*)(buf+processed_bytes);
	p_def->Process_state_header(buf+processed_bytes, type);

	processed_bytes += p_def->State_header_size();
	for(i=0; i< pkt->num_cells; i++) {
	    state_cell = (State_Cell*)(buf+processed_bytes);
	    
	    if(!Same_endian(type)) {
		Flip_state_cell(state_cell);
	    }
	    /* Did I know about this state ? */
	    if((s_data = Find_State(p_def->All_States(), pkt->source, 
				 state_cell->dest)) == NULL) {
		/* This is about a state that I didn't know about until 
		 * now... */
		s_data = p_def->Process_state_cell(pkt->source, buf+processed_bytes);
		Add_to_changed_states(p_def, sender_ip, s_data, NEW_CHANGE);
		changed_route_flag = 1;
	    }
	    else {
                /* Lets make sure that my clock is not back in time, 
                   with respect to others in the network. */
                if (pkt->source == My_Address) 
                {
                    if (s_data->timestamp_sec < state_cell->timestamp_sec ||
                        (s_data->timestamp_sec == state_cell->timestamp_sec &&
                         s_data->timestamp_usec < state_cell->timestamp_usec)) {
                        /* My clock was out-of-sync!  Need to resend all of my states 
                           with an update clock value */
                            Alarm(EXIT, "Clock is out-of-sync!!!\n");
                            /*
                            if (state_sync_up_val.sec <= state_cell->timestamp_sec) {
                                state_sync_up_val.sec = state_cell->timestamp_sec+1;
                            }
                            E_dequeue(Resend_States, 0, &Edge_Prot_Def);
                            E_dequeue(Resend_States, 0, &Groups_Prot_Def);
                            E_queue(Resend_States, 1, &Edge_Prot_Def, resend_fast_timeout);
                            E_queue(Resend_States, 1, &Groups_Prot_Def, resend_fast_timeout);
                            */
                    }
                }
		if((s_data->timestamp_sec < state_cell->timestamp_sec)||
		   ((s_data->timestamp_sec == state_cell->timestamp_sec)&&
		    (s_data->timestamp_usec < state_cell->timestamp_usec))) {
		    /* This is a new update */
		    p_def->Process_state_cell(pkt->source, buf+processed_bytes);
		    Add_to_changed_states(p_def, sender_ip, s_data, NEW_CHANGE);
		    changed_route_flag = 1;
		}
		else if((s_data->timestamp_sec == state_cell->timestamp_sec)&&
			(s_data->timestamp_usec == state_cell->timestamp_usec)) {
		    Add_to_changed_states(p_def, sender_ip, s_data, OLD_CHANGE);
		}
	    }
	    processed_bytes += p_def->Cell_packet_size();
	}
    }
    if((changed_route_flag == 1)&&(p_def->Is_route_change())) {
	if(Schedule_Set_Route == 0) {
	    Schedule_Set_Route = 1;
	    E_queue(Set_Routes, 0, NULL, short_timeout);
	}
    }
}



/***********************************************************/
/* void Resend_States(int sync_up, void* p_data)           */
/*                                                         */
/* Called by the event system                              */
/* Resends the valid states to all the neighbors for       */
/* garbage collection purposes                             */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* p_data: structure with the protocol function pointers   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Resend_States (int sync_up, void* p_data)
{
    Prot_Def *p_def;
    State_Data *s_data;
    State_Chain *s_chain;
    sp_time now, diff, state_time;
    stdhash *hash_struct;
    stdit it, src_it;
    int cnt = 0;

    
    p_def = (Prot_Def*)p_data;
    hash_struct = p_def->All_States();
    now = E_get_time();

    stdhash_find(hash_struct, &src_it, &My_Address);
    if(!stdhash_is_end(hash_struct, &src_it)) {
	s_chain = *((State_Chain **)stdhash_it_val(&src_it));
	stdhash_begin(&s_chain->states, &it); 
	while(!stdhash_is_end(&s_chain->states, &it)) {
	    s_data = *((State_Data **)stdhash_it_val(&it));
	    
	    state_time.sec  = s_data->my_timestamp_sec;
	    state_time.usec = s_data->my_timestamp_usec;
	    diff = E_sub_time(now, state_time);
	    
	    Alarm(DEBUG, "resend_now :  %d, %d\n", now.sec, now.usec);
	    Alarm(DEBUG, "resend_state: %d, %d\n", state_time.sec, state_time.usec);
	    Alarm(DEBUG, "resend_diff:  %d, %d\n", diff.sec, diff.usec);
	    
	    if((E_compare_time(diff, state_resend_time) >= 0)&&
	       (p_def->Is_state_relevant((void*)s_data))) {
		Alarm(DEBUG, "Resend_Updating state: %d.%d.%d.%d -> %d.%d.%d.%d\n",
		      IP1(s_data->source_addr), IP2(s_data->source_addr), 
		      IP3(s_data->source_addr), IP4(s_data->source_addr), 
		      IP1(s_data->dest_addr), IP2(s_data->dest_addr), 
		      IP3(s_data->dest_addr), IP4(s_data->dest_addr));

		s_data->timestamp_usec++;
		if(s_data->timestamp_usec >= 1000000) {
		    s_data->timestamp_usec = 0;
		    s_data->timestamp_sec++;
		}
		Add_to_changed_states(p_def, My_Address, s_data, NEW_CHANGE);

		cnt++;
		if(cnt > 500) {
		    E_queue(Resend_States, 0, p_data, resend_fast_timeout);
		    return;
		}
	    }	
	    stdhash_it_next(&it);
	} 
    }   
    E_queue(Resend_States, 0, p_data, resend_call_timeout);
}





/***********************************************************/
/* void State_Garbage_Collect(int dummy_int, void* p_data) */
/*                                                         */
/* Called by the event system                              */
/* Remove the unnecessary (expired) states from memory     */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* p_data: structure with the protocol function pointers   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void State_Garbage_Collect(int dummy_int, void* p_data)
{
    Prot_Def *p_def;
    stdhash *hash_struct, *hash_struct_by_dest;
    stdit it, dst_it, src_it, st_it;
    State_Data *s_data;
    State_Chain *s_chain_src, *s_chain_dst;
    sp_time now, diff, state_time;
    int flag;

    Alarm(DEBUG, "Garbage_Collector()\n");

    p_def = (Prot_Def*)p_data;
    hash_struct = p_def->All_States();
    hash_struct_by_dest = p_def->All_States_by_Dest();
    now = E_get_time();

    /* All the states */

    stdhash_begin(hash_struct, &src_it); 
    while(!stdhash_is_end(hash_struct, &src_it)) {
        /* sources one by one... */
	s_chain_src = *((State_Chain **)stdhash_it_val(&src_it));
	stdhash_begin(&s_chain_src->states, &it);
	while(!stdhash_is_end(&s_chain_src->states, &it)) {
	    s_data = *((State_Data **)stdhash_it_val(&it));
	    flag = 0;
	    state_time.sec  = s_data->my_timestamp_sec - (s_data->age*10);
	    state_time.usec = s_data->my_timestamp_usec;
	    
	    Alarm(DEBUG, "now : %d, %d\n", now.sec, now.usec);
	    Alarm(DEBUG, "state: %d, %d; age: %d\n", state_time.sec, state_time.usec, s_data->age);
	    
	    
	    diff = E_sub_time(now, state_time);
	    
	    Alarm(DEBUG, "diff: %d, %d\n", diff.sec, diff.usec);
	    
	    if(E_compare_time(diff, gb_collect_remove) >= 0) {
		
		Alarm(PRINT, "Garbage_Collector() -- delete state\n");
		Alarm(PRINT, "DELETING state: %d.%d.%d.%d -> %d.%d.%d.%d\n",
		      IP1(s_data->source_addr), IP2(s_data->source_addr), 
		      IP3(s_data->source_addr), IP4(s_data->source_addr), 
		      IP1(s_data->dest_addr), IP2(s_data->dest_addr), 
		      IP3(s_data->dest_addr), IP4(s_data->dest_addr));
		
		/* Ok, this is a very old state. It should be removed... */
		stdhash_erase(&s_chain_src->states, &it);

		/* Remove it from the "by_Dest" structure */
		if(hash_struct_by_dest != NULL) {
		    stdhash_find(hash_struct_by_dest, &dst_it, &s_data->dest_addr);
		    if(stdhash_is_end(hash_struct_by_dest, &dst_it)) {
			Alarm(EXIT, "Garbage collect: no entry in the by_dest hash\n");
		    }

		    s_chain_dst = *((State_Chain **)stdhash_it_val(&dst_it));
		    stdhash_find(&s_chain_dst->states, &st_it, &s_data->source_addr);
		    if(stdhash_is_end(&s_chain_dst->states, &st_it)) {
			Alarm(EXIT, "Garbage collect: no entry in the state_chain hash\n");
		    }
		    stdhash_erase(&s_chain_dst->states, &st_it);
		    if(stdhash_empty(&s_chain_dst->states)) {
			stdhash_destruct(&s_chain_dst->states);
			stdhash_erase(hash_struct_by_dest, &dst_it);
			dispose(s_chain_dst);
		    }
		}


		if(p_def->State_type() & LINK_STATE_TYPE) {
		    /* See if we need to delete the Nodes of the edge also */
		    if(Try_Remove_Node(s_data->source_addr) < 0)
			Alarm(EXIT, "Garbage_Collector(): Error removing node\n");
		    if(Try_Remove_Node(s_data->dest_addr) < 0)
			Alarm(EXIT, "Garbage_Collector(): Error removing node\n");
		}
		p_def->Destroy_State_Data(s_data);
		dispose(s_data);
		continue;
	    }
	    stdhash_it_next(&it); 
	}
	if(stdhash_empty(&s_chain_src->states)) {
	    stdhash_destruct(&s_chain_src->states);
	    stdhash_erase(hash_struct, &src_it);
	    dispose(s_chain_src);
	    continue;
	}
	stdhash_it_next(&src_it); 
    }
    E_queue(State_Garbage_Collect, 0, p_data, gb_collect_timeout);
}


/***********************************************************/
/* State_Data* Find_State(stdhash *hash_struct,            */
/*                       int32 source, int32 dest)         */
/*                                                         */
/* Finds a state in a hash structure                       */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* hash_struct:   The hash structure to look into          */
/*                (should be indexed by source)            */
/* source: IP address of the source                        */
/* dest:   IP address of the destination                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (State_Data*) a pointer to the State structure, or NULL */
/* if the state does not exist                             */
/*                                                         */
/***********************************************************/

State_Data* Find_State(stdhash *hash_struct, int32 source, int32 dest) 
{
    stdit it, src_it;
    State_Data *s_data;
    State_Chain *s_chain;

    stdhash_find(hash_struct, &src_it, &source);
    if(!stdhash_is_end(hash_struct, &src_it)) {
	s_chain = *((State_Chain **)stdhash_it_val(&src_it));
	stdhash_find(&s_chain->states, &it, &dest);
	if(!stdhash_is_end(&s_chain->states, &it)) {
	    s_data = *((State_Data **)stdhash_it_val(&it));
	    return(s_data);
	}
    }
    return(NULL);
}



/***********************************************************/
/* void Add_to_changed_states(Prot_Def *p_def, int32 sender*/
/*                            State_Data *s_data, int how) */
/*                                                         */
/* Adds a state to the buffer of changed states to be sent */
/* to neighbors                                            */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* p_def: structure with the protocol function pointers    */
/* sender: IP address of the node that informed me about   */
/*         the chenge                                      */
/* s_data: state that has changed parameters               */
/* how:    NEW_CHANGE: this is first time I see the change */
/*         OLD_CHANGE: I already know about this           */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Add_to_changed_states(Prot_Def *p_def, int32 sender, 
			   State_Data *s_data, int how)
{

    stdit it;
    Node *nd;
    int32 ip_address = sender;
    Changed_State *cg_state;
    sp_time now;
    int remote_flag = 0;
    int temp;
    char how_str[5];  


    if(s_data == NULL)
	return;

    if(how == NEW_CHANGE)
	strcpy(how_str, "NEW");
    else if(how == OLD_CHANGE)
	strcpy(how_str, "OLD");

    Alarm(DEBUG, "Add_to_changed_states(): sender: %d.%d.%d.%d; how: %s\n",
	  IP1(sender), IP2(sender), IP3(sender), IP4(sender), how_str);
    Alarm(DEBUG, "\t\t%d.%d.%d.%d -> %d.%d.%d.%d\n", 
	  IP1(s_data->source_addr), IP2(s_data->source_addr), 
	  IP3(s_data->source_addr), IP4(s_data->source_addr), 
	  IP1(s_data->dest_addr), IP2(s_data->dest_addr), 
	  IP3(s_data->dest_addr), IP4(s_data->dest_addr)); 

    /* Find sender node in the datastructure */
    stdhash_find(&All_Nodes, &it, &ip_address);
    if(stdhash_is_end(&All_Nodes, &it)) {
        Alarm(EXIT, "Add_to_changed_states(): non existing sender node\n");
    }
    nd = *((Node **)stdhash_it_val(&it));
    if((!(nd->flags&NEIGHBOR_NODE))&&(nd->address != My_Address)) {
        /* Sender node is not a neighbor !!! 
	   (probably it's not yet, Hello protocol will take care of it later)*/
	remote_flag = 1;
    }

    now = E_get_time();
   
    if(how == NEW_CHANGE) {
    	s_data->my_timestamp_sec = (int32)now.sec;
    	s_data->my_timestamp_usec = (int32)now.usec;	
    }
    
    if(stdhash_empty(p_def->Changed_States())) {
        if (Wireless) {
            /* The more I wait, the more I can pack, but the more I will 
               delay the state update, or handoff */
            E_queue(Send_State_Updates, 0, (void*)p_def, wireless_timeout);
        } else {
            E_queue(Send_State_Updates, 0, (void*)p_def, short_timeout);
        }
    }

    if(nd->address == My_Address)
        Alarm(DEBUG, "nodeid: local, it's my update\n");
    else
        Alarm(DEBUG, "nodeid: %d\n", nd->node_id);

#ifndef ARCH_PC_WIN95
    /* If multicast change, schedule kernel route change if necessary
       Always schedule after Send_State_Updates */
    if(p_def == &Groups_Prot_Def) {
	/* Multicast change */
	Discard_Mcast_Neighbors(s_data->dest_addr); 
        if(Is_valid_kr_group(s_data->dest_addr)) {
            if (!E_in_queue(KR_Set_Group_Route, s_data->dest_addr, NULL)) {
                /* TODO: If I am a member, I should update right away. Should
                   use eucl distance to determine delay, to decrease unstable interval,
                   say my_eucl_dist*wireless_timeout */
                E_queue(KR_Set_Group_Route, s_data->dest_addr, NULL, kr_timeout);
            }
        } 
    }
    else {
	/*Topology change. Discard in route.c*/
	/*Discard_All_Mcast_Neighbors();*/
    }
#endif

    /* Find the state in the to_be_sent buffer (if it exists and 
       it wasn't sent already) */
    if((cg_state = 
	Find_Changed_State(p_def->Changed_States(), s_data->source_addr, 
			    s_data->dest_addr)) != NULL) {
        if(how == NEW_CHANGE) {
	    /* Update the mask to be for this guy only */
	    if(sender == My_Address || remote_flag == 1) {
	        temp = 0;
	    }
	    else {
	        temp = 1;
		temp = temp << nd->node_id%32;
	    }
	    cg_state->mask[nd->node_id/32] = temp;
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
	    cg_state->mask[nd->node_id/32] |= temp;
	}
    }
    else if(how == NEW_CHANGE) {
        /* Add this state to the list of to_be_sent */
        if((cg_state = (Changed_State*)new(CHANGED_STATE))==NULL)
	    Alarm(EXIT, "Add_to_changed_states(): Cannot allocte state object\n");  
    
	/* Initialize the changed_state structure */
	if(sender == My_Address || remote_flag == 1) {
	    temp = 0;
	}
	else {
	    temp = 1;
	    temp = temp << nd->node_id%32;
	}
	cg_state->mask[nd->node_id/32] = temp;
	cg_state->state = s_data;
   
	/* Insert the cg_state in the global data structures */
	stdhash_insert(p_def->Changed_States(), &it, &s_data->source_addr, &cg_state);
    }
}



/***********************************************************/
/* Changed_State* Find_Changed_State(stdhash *hash_struct, */
/*                       int32 source, int32 dest)         */
/*                                                         */
/* Finds a state in the buffer of changed states           */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* hash:   The hash structure to look into                 */
/* source: IP address of the source                        */
/* dest:   IP address of the destination                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Changed_State*) a pointer to the chg. state structure, */
/*                  or NULL if the chg. state doesn't exist*/
/*                                                         */
/***********************************************************/

Changed_State* Find_Changed_State(stdhash *hash_struct, int32 source, int32 dest) 
{
    stdit it;
    int32 src;
    int32 dst;
    Changed_State *cg_state;
    State_Data *s_data;
    
    src = source; dst = dest;
    
    stdhash_find(hash_struct, &it, &src);
    if(stdhash_is_end(hash_struct, &it))
        return NULL;
    
    cg_state = *((Changed_State **)stdhash_it_val(&it));
    s_data = (State_Data*)cg_state->state;
    if(s_data->dest_addr == dst) {
        return cg_state;
    }
    stdhash_keyed_next(hash_struct, &it); 
    while(!stdhash_is_end(hash_struct, &it)) {
	cg_state = *((Changed_State **)stdhash_it_val(&it));
	s_data = (State_Data*)cg_state->state;
	if(s_data->dest_addr == dst) {
	    return cg_state;
	}
	stdhash_keyed_next(hash_struct, &it); 
    }
    return NULL;
}



/***********************************************************/
/* void Empty_Changed_States(stdhash *hash_struct)         */
/*                                                         */
/* Empties the buffer of changed states                    */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* *hash_struct: the hash with these change updates        */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Empty_Changed_States(stdhash *hash_struct)
{
    stdit it;
    Changed_State *cg_state;

    stdhash_begin(hash_struct, &it); 
    while(!stdhash_is_end(hash_struct, &it)) {
        cg_state = *((Changed_State **)stdhash_it_val(&it));
	dispose(cg_state);
	stdhash_it_next(&it);
    } 
    stdhash_clear(hash_struct);
}


/***********************************************************/
/* Prot_Def* Get_Prot_Def(int32u type)                     */
/*                                                         */
/* Get the definition of potocol functions based on the    */
/* type of the message                                     */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* type: The type of the message                           */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Prot_Def*) The protocol functions structure            */
/*                                                         */
/***********************************************************/

Prot_Def* Get_Prot_Def(int32u type)
{
    if(Is_link_state(type)) {
	return(&Edge_Prot_Def);
    }
    else if(Is_group_state(type)) {
	return(&Groups_Prot_Def);
    }
    return(NULL);
}
