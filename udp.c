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
#include "util/data_link.h"
#include "stdutil/src/stdutil/stdhash.h"
#include "stdutil/src/stdutil/stdcarr.h"

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "link.h"
#include "network.h"
#include "reliable_link.h"
#include "link_state.h"
#include "hello.h"
#include "udp.h"
#include "reliable_udp.h"
#include "protocol.h"
#include "route.h"
#include "session.h"

/* Global vriables */

extern int32     My_Address;
extern stdhash   All_Nodes;
extern Link*     Links[MAX_LINKS];
extern int       network_flag;
extern int       Reliable_Flag;
extern channel   Local_Send_Channels[MAX_LINKS_4_EDGE + 1]; 


/* Local constatnts */

static const sp_time zero_timeout  = {     0,    0};


void	Flip_udp_hdr( udp_header *udp_hdr )
{
    udp_hdr->source	  = Flip_int32( udp_hdr->source );
    udp_hdr->dest	  = Flip_int32( udp_hdr->dest );
    udp_hdr->source_port  = Flip_int16( udp_hdr->source_port );
    udp_hdr->dest_port	  = Flip_int16( udp_hdr->dest_port );
    udp_hdr->len	  = Flip_int16( udp_hdr->len );
    udp_hdr->seq_no	  = Flip_int16( udp_hdr->seq_no );
}



/***********************************************************/
/* void Process_udp_data_packet(int32 sender_id,           */
/*                              char *buff,                */
/*                              int16u data_len,           */
/*			        int32u type, int mode)     */
/*                                                         */
/* Processes a UDP data packet                             */
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

void Process_udp_data_packet(int32 sender_id, char *buff, int16u data_len, 
			     int32u type, int mode)
{
    udp_header *hdr;
    Node* next_hop;
    int ret;


    /* Process the packet */
    hdr = (udp_header*)buff;

    if(!Same_endian(type))
	Flip_udp_hdr(hdr);

    if(hdr->len + sizeof(udp_header) == data_len) {
	if(hdr->dest == My_Address) {
	    ret = Deliver_UDP_Data(buff, data_len, 0);
	}
	else {
	    next_hop = Get_Route(hdr->dest);
	    if(next_hop != NULL) {
		if(Reliable_Flag == 1) {
		    ret = Forward_Rel_UDP_Data(next_hop, buff, data_len, 0);
		}
		else {
		    ret = Forward_UDP_Data(next_hop, buff, data_len);
		}
	    }
	    else {
		return;
	    }
	}
    }
    else {
	Alarm(EXIT, "Process_udp_data: Packed data... not available yet\n");
    }
}



/***********************************************************/
/* int Forward_UDP_data((Node *next_hop, char *buff,       */
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

int Forward_UDP_Data(Node *next_hop, char *buff, int16u buf_len)
{
    Link *lk, *lk_ctr;
    UDP_Data *u_data;
    packet_header hdr;
    sys_scatter scat;
    int ret;

    
    lk = next_hop->link[UDP_LINK];
    if(lk == NULL) {
	return(BUFF_DROP);
    }
    
    u_data = (UDP_Data*)lk->prot_data;
    if(u_data == NULL) {
	Alarm(EXIT, "Forward_UDP_Data: UDP link w/o udp data\n");
    }

    lk_ctr = lk->other_side_node->link[CONTROL_LINK];
    if(lk_ctr == NULL) {
	Alarm(EXIT, "Forward_UDP_Data(): Non existing control link !");
    }  



#if 0
    /* This is for links having send buffer. NOT YET IMPLEMENTED  */
    if((u_data->block_flag != 0)||
       (!stdcarr_empty(&(u_data->udp_net_buff)))||
       (!stdcarr_empty(&(u_data->udp_ses_buff)))) {
	
	if(stdcarr_size(&u_data->udp_net_buff) >= MAX_BUFF) {
	    return(BUFF_DROP);
	}

	if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
	    Alarm(EXIT, "Forward_UDP_Data(): Cannot allocte udp cell\n");
	}
	u_cell->len = buf_len;
	u_cell->buff = buff;
	stdcarr_push_back(&u_data->udp_net_buff, &u_cell);
	inc_ref_cnt(buff);
	return(BUFF_OK);
    }

    /* If I got up to here, there is nothing in the buffer,
     * and I'm not blocked. So let's try to send the packet */


#endif


    scat.num_elements = 2;
    scat.elements[0].len = sizeof(packet_header);
    scat.elements[0].buf = (char *) &hdr;
    scat.elements[1].len = buf_len;
    scat.elements[1].buf = buff;
    
    hdr.type    = UDP_DATA_TYPE;
    hdr.type    = Set_endian(hdr.type);

    u_data->my_seq_no++;
    if(u_data->my_seq_no >= PACK_MAX_SEQ)
	u_data->my_seq_no = 0;

    /*    hdr.seq_no    = u_data->my_seq_no;*/
    hdr.sender_id = My_Address;
    hdr.data_len  = buf_len;
    hdr.ack_len   = 0;
	
    if(network_flag == 1) {
	ret = DL_send(lk->chan, 
		      next_hop->address,
		      lk->port, 
		      &scat );
	
	if(ret < 0) {
	    return(BUFF_DROP);
	}
    }
    return(BUFF_EMPTY);
}


/***********************************************************/
/* void UDP_Check_Link_Loss(int32 sender, int16u seq_no)   */
/*                                                         */
/* Checks the link loss probability on a UDP channel       */
/* between two neighbours                                  */
/* THIS FUNCTION IS FOR FUTURE DEVELOPMENT                 */
/* IT IS NOT USED YET !!!                                  */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender: IP of the neighbour                             */
/* seq_no: sequence number of the UDP packet received      */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/



void UDP_Check_Link_Loss(int32 sender, int16u seq_no) {
    Link *lk;
    stdhash_it it;
    Node *sender_node;
    UDP_Data *u_data;
    int last_interval;
    

    if(seq_no != 0xffff) {
	/* Get the link to the sender */
	stdhash_find(&All_Nodes, &it, &sender);
	if(stdhash_it_is_end(&it)) { /* I had no idea about the sender node */
	    return;
	}
	sender_node = *((Node **)stdhash_it_val(&it));
	lk = sender_node->link[UDP_LINK];
	if(lk == NULL)
	    return;
	if(lk->prot_data == NULL)
	    return;
	u_data = (UDP_Data*)lk->prot_data;

	if(seq_no == u_data->other_side_seq_no) {
	    /* this is an old packet */
	    return;
	}
	else if(seq_no > u_data->other_side_seq_no) {
	    if(seq_no - u_data->other_side_seq_no > PACK_MAX_SEQ/2) {
		/* this is an old packet */
		Alarm(PRINT, "old seq: %d\n", seq_no);
		return;
	    }
	    
	    if(seq_no == u_data->other_side_seq_no + 1) {
		u_data->other_side_seq_no++;
	    }
	    else {
		/* Ok, there was a loss here */
		last_interval = PACK_MAX_SEQ;
		last_interval *= u_data->no_of_rounds_wo_loss;
		last_interval += u_data->other_side_seq_no - 
		    u_data->last_seq_noloss;
		
		Alarm(PRINT, "\nLOSS! new: %d; old: %d; last interval: %d\n\n", 
		      seq_no, u_data->other_side_seq_no, last_interval);

		u_data->last_seq_noloss = seq_no;
		u_data->other_side_seq_no = seq_no;
		u_data->no_of_rounds_wo_loss = 0;
	    }
	}
	else {
	    if(u_data->other_side_seq_no - seq_no < PACK_MAX_SEQ/2) {
		/* this is an old packet */
		Alarm(PRINT, "old seq: %d; last:%d\n", 
		      seq_no, u_data->other_side_seq_no);
		return;
	    }

	    Alarm(PRINT, "End of sequence: seq_no: %d; last_seq: %d\n",
		  seq_no, u_data->other_side_seq_no);

	    if((u_data->other_side_seq_no == PACK_MAX_SEQ - 1)&&
	       (seq_no == 0)) {
		u_data->other_side_seq_no = 0;
		u_data->no_of_rounds_wo_loss++;
	    }
	    else {
		/* Ok, there was a loss here */
		last_interval = PACK_MAX_SEQ;
		last_interval *= u_data->no_of_rounds_wo_loss;
		last_interval += u_data->other_side_seq_no - 
		    u_data->last_seq_noloss;
		
		Alarm(PRINT, "\nLOSS! last interval: %d\n\n", last_interval);

		u_data->last_seq_noloss = seq_no;
		u_data->other_side_seq_no = seq_no;
		u_data->no_of_rounds_wo_loss = 0;
	    }	    
	}
    }
    else {
	printf("\n\n^^^^^ seq_no special !\n\n");
    }
}



