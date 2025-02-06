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
#include "util/data_link.h"
#include "util/memory.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "link.h"
#include "network.h"
#include "reliable_datagram.h"
#include "link_state.h"
#include "hello.h"
#include "protocol.h"
#include "route.h"
#include "udp.h"
#include "reliable_udp.h"
#include "realtime_udp.h"
#include "state_flood.h"


/* Statistics */
extern long64 total_udp_pkts;
extern long64 total_udp_bytes;
extern long64 total_rel_udp_pkts;
extern long64 total_rel_udp_bytes;
extern long64 total_link_ack_pkts;
extern long64 total_link_ack_bytes;
extern long64 total_hello_pkts;
extern long64 total_hello_bytes;
extern long64 total_link_state_pkts;
extern long64 total_link_state_bytes;
extern long64 total_group_state_pkts;
extern long64 total_group_state_bytes;






/***********************************************************/
/* int16 Prot_process_scat(sys_scatter *scat,              */
/*                         int total_bytes, int mode,      */
/*                         int32 type)                     */
/*                                                         */
/* Processes a scatter received from the network           */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* scat:        scatter to be processed                    */
/* total_bytes: number of bytes received in the scatter    */
/* mode:        mode of the link (CONTROL, UDP, etc.)      */
/* type:        the original (unflipped) type of the msg.  */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/



/* scat is the receiving scatter (Global !). 
   At the return of the function I should either:
       - finish processing of scat, so it can be reused for receiving
       - allocate a new scat header/body so I have smthg to receive in */
void Prot_process_scat(sys_scatter *scat, int total_bytes, int mode, int32 type)
{
    int remaining_bytes;
    packet_header *pack_hdr;
 

    pack_hdr = (packet_header *)scat->elements[0].buf;

    remaining_bytes = total_bytes - scat->elements[0].len;
    if(remaining_bytes != pack_hdr->data_len+pack_hdr->ack_len) {
	/* Ignore the message */
	Alarm(PRINT, "Prot_process_scat(): Got a corrupted message: %d; %d; %d\n",
	      remaining_bytes, pack_hdr->data_len, pack_hdr->ack_len);
	return;
    }

    /* Check the loss rate from the sender */
    Check_Link_Loss(pack_hdr->sender_id, pack_hdr->seq_no);

    /* Call the appropriate processing function for the packet */
    if(Is_udp_data(pack_hdr->type)) {
	Alarm(DEBUG, "Udp_data: %d\n", total_bytes);   

	total_udp_pkts++;
	total_udp_bytes += total_bytes;

        Process_udp_data_packet(pack_hdr->sender_id, scat->elements[1].buf, 
			   pack_hdr->data_len, type, mode);

	/* Here it's not ok anymore, Process_udp_data_packet() 
	   might still  need to keep the buffer after returning
	   Therefore, the function should create a new buffer if needed
	   and replace the original one. */
	
	if(get_ref_cnt(scat->elements[1].buf) > 1) {
	    dec_ref_cnt(scat->elements[1].buf);
	    if((scat->elements[1].buf = 
		(char *) new_ref_cnt(PACK_BODY_OBJ)) == NULL) {
		Alarm(EXIT, "Prot_process_scat: Could not allocate packet body obj\n");
	    } 	    
	}
    }
    else if(Is_realtime_data(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_realtime_udp_data: size: %d\n", total_bytes); 

	total_udp_pkts++;
	total_udp_bytes += total_bytes;

        Process_RT_UDP_data_packet(pack_hdr->sender_id, scat->elements[1].buf, 
			   pack_hdr->data_len, pack_hdr->ack_len, type, mode);

	/* Here it's not ok anymore, Process_udp_data_packet() 
	   might still  need to keep the buffer after returning
	   Therefore, the function should create a new buffer if needed
	   and replace the original one. */
	
	if(get_ref_cnt(scat->elements[1].buf) > 1) {
	    dec_ref_cnt(scat->elements[1].buf);
	    if((scat->elements[1].buf = 
		(char *) new_ref_cnt(PACK_BODY_OBJ)) == NULL) {
		Alarm(EXIT, "Prot_process_scat: Could not allocate packet body obj\n");
	    } 	    
	}
    }
    else if(Is_realtime_nack(pack_hdr->type)) {
        Alarm(DEBUG, "\n\nprocess_realtime_nack: size: %d\n", total_bytes);

	total_link_ack_pkts++;
	total_link_ack_bytes += total_bytes;
 
        Process_RT_nack_packet(pack_hdr->sender_id, scat->elements[1].buf, 
			       pack_hdr->ack_len, type, mode);
	 

	/* Here it's not ok anymore, Process_RT_nack_packet() 
	   might still  need to keep the buffer after returning
	   Therefore, the function should create a new buffer if needed
	   and replace the original one. */
	if(get_ref_cnt(scat->elements[1].buf) > 1) {
	    dec_ref_cnt(scat->elements[1].buf);
	    if((scat->elements[1].buf = 
		(char *) new_ref_cnt(PACK_BODY_OBJ)) == NULL) {
		Alarm(EXIT, "Prot_process_scat: Could not allocate packet body obj\n");
	    } 	    
	}

    }
    else if(Is_rel_udp_data(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_rel_udp_data: size: %d\n", total_bytes);    

	total_rel_udp_pkts++;
	total_rel_udp_bytes += total_bytes;

        Process_rel_udp_data_packet(pack_hdr->sender_id, 
				    scat->elements[1].buf, 
				    pack_hdr->data_len, pack_hdr->ack_len,
				    type, mode);
	/* Here it's not ok anymore, Process_rel_udp_data_packet() 
	   might still  need to keep the buffer after returning
	   Therefore, the function should create a new buffer if needed
	   and replace the original one. */
	if(get_ref_cnt(scat->elements[1].buf) > 1) {
	    dec_ref_cnt(scat->elements[1].buf);
	    if((scat->elements[1].buf = 
		(char *) new_ref_cnt(PACK_BODY_OBJ)) == NULL) {
		Alarm(EXIT, "Prot_process_scat: Could not allocate packet body obj\n");
	    } 	    
	}
    }
    else if(Is_link_ack(pack_hdr->type)) {
        /*Alarm(DEBUG, "\n\nprocess_ack: size: %d\n", total_bytes);*/

	total_link_ack_pkts++;
	total_link_ack_bytes += total_bytes;
 
        Process_ack_packet(pack_hdr->sender_id, scat->elements[1].buf, 
			   pack_hdr->ack_len, type, mode);
	/* It's ok here, Process_ack_packet() does not need the buffer 
	   after returning */
    }
    else if(Is_hello(pack_hdr->type)||Is_hello_req(pack_hdr->type)) {
	/*Alarm(PRINT, "\n\nprocess_hello: size: %d\n", total_bytes);*/ 

	total_hello_pkts++;
	total_hello_bytes += total_bytes;

        Process_hello_packet(scat->elements[1].buf, remaining_bytes, 
			     pack_hdr->sender_id, type);
	/* It's ok here, Process_hello_packet() does not need the buffer 
	   after returning */
    }
    else if(Is_hello_ping(pack_hdr->type)) {
	/*Alarm(PRINT, "\n\nprocess_hello_ping: size: %d\n", total_bytes);*/  

	total_hello_pkts++;
	total_hello_bytes += total_bytes;

        Process_hello_ping_packet(pack_hdr->sender_id, mode);
	/* It's ok here, Process_hello_ping_packet() does not need the buffer 
	   after returning */
    }
    else if((Is_link_state(pack_hdr->type)||(Is_group_state(pack_hdr->type)))) {
        /*Alarm(PRINT, "\n\nprocess_state_packet: size: %d\n", total_bytes);*/ 

	if(Is_link_state(pack_hdr->type)) {
	    total_link_state_pkts++;
	    total_link_state_bytes += total_bytes;   
	}
	else {
	    total_group_state_pkts++;
	    total_group_state_bytes += total_bytes;   	    
	}

        Process_state_packet(pack_hdr->sender_id, scat->elements[1].buf, 
				  pack_hdr->data_len, pack_hdr->ack_len, 
				  type, mode);
	/* It's ok here, Process_state_packet() does not need the buffer 
	   after returning */
    }
    else {
	/* Ignore the message */
        Alarm(DEBUG, "Prot_process_scat(): Unknown message type: %X\n",
	      pack_hdr->type);
	return;
    }
}


