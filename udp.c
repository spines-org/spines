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
 * Copyright (c) 2003 - 2007 The Johns Hopkins University.
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
extern int       Unicast_Only;
extern int       Security;

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
    udp_hdr->sess_id	  = Flip_int16( udp_hdr->sess_id );
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
    Node *next_hop, *nd;
    stdit ngb_it, grp_it, st_it, src_it;
    Group_State *g_state;
    State_Chain *s_chain_grp;
    int ret;
    stdhash *neighbors;
    
    /* Process the packet */
    hdr = (udp_header*)buff;

    if(!Same_endian(type))
	Flip_udp_hdr(hdr);

    if(hdr->len + sizeof(udp_header) == data_len) {	
	if(!Is_mcast_addr(hdr->dest) && !Is_acast_addr(hdr->dest)) {
	    /* This is unicast */
	   
	    /* This could be a resent multicast message. TODO Should we do
	     * something differently, or is it adequate to use port numbers
	     * to identify a resent multicast message? */
	   
	    /* Is this for me? */
	    if(hdr->dest == My_Address) {
		ret = Deliver_UDP_Data(buff, data_len, 0);
		return;
	    }
	    
	    /* Nope, it's for smbd else. See where we should forward it */
	    next_hop = Get_Route(My_Address, hdr->dest);
	    if(next_hop != NULL) {
		ret = Forward_UDP_Data(next_hop, buff, data_len);
		return;
	    }
	    else {
		return;
	    }
	}
	else if (Unicast_Only != 1) { 
	    /* This is multicast or anycast */
	    if(Find_State(&All_Groups_by_Node, My_Address, hdr->dest) != NULL) {
		/* Hey, I joined this group !*/
		Deliver_UDP_Data(buff, data_len, 0);
	    }
	    stdhash_find(&All_Groups_by_Name, &grp_it, &hdr->dest);
	    if(!stdhash_is_end(&All_Groups_by_Name, &grp_it)) {
		/* Send Ack -- TEMPORARY */
		s_chain_grp = *((State_Chain **)stdhash_it_val(&grp_it));
		stdhash_begin(&s_chain_grp->states, &st_it);
		if(!stdhash_is_end(&s_chain_grp->states, &st_it)) {
		    g_state = *((Group_State **)stdhash_it_val(&st_it));
		    if ((g_state->flags & SENDRECV_GROUP) != 0) {
			stdhash_find(&All_Nodes, &src_it, &hdr->source);
			if(!stdhash_is_end(&All_Nodes, &src_it)) {
			    nd = *((Node **)stdhash_it_val(&src_it));
#if 0
			    stdhash_begin(&nd->routes, &rt_it);
			    while(!stdhash_is_end(&nd->routes, &rt_it)) {
				route = *((Route **)stdhash_it_val(&rt_it));
				if (route->forwarder != 0) {
				    stdhash_find(&Neighbors, &ngb_it, &route->forwarder->address);
				    if(stdhash_is_end(&Neighbors, &ngb_it)) {
					stdhash_insert(&Neighbors, &ngb_it, &route->forwarder->address, &route->forwarder);
				    }
				}
			        stdhash_it_next(&rt_it);
			    }
#endif
		        }
		    }
		}
	    }
	    neighbors = Get_Mcast_Neighbors(hdr->source, hdr->dest);
	    
	    if( neighbors != NULL) {
		stdhash_begin(neighbors, &ngb_it);
		while(!stdhash_is_end(neighbors, &ngb_it)) {
		    next_hop = *((Node **)stdhash_it_val(&ngb_it));
		    ret = Forward_UDP_Data(next_hop, buff, data_len);
		    stdhash_it_next(&ngb_it);
		}
	    }
	    return;
	}
    }
    else {
	Alarm(PRINT, "Process_udp_data: Packed data... not available yet %d :: %d\n",
	      hdr->len + sizeof(udp_header), data_len);
	return;
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
    Link *lk;
    packet_header hdr;
    sys_scatter scat;
    int ret;

    
    if(next_hop->address == My_Address) {
	Process_udp_data_packet(My_Address, buff, buf_len, 
				 Set_endian(UDP_DATA_TYPE), UDP_LINK);
	
	return(BUFF_EMPTY);
    }

    lk = next_hop->link[UDP_LINK];
    if(lk == NULL) {
	return(BUFF_DROP);
    }
    
    scat.num_elements = 2;
    scat.elements[0].len = sizeof(packet_header);
    scat.elements[0].buf = (char *) &hdr;
    scat.elements[1].len = buf_len;
    scat.elements[1].buf = buff;
    
    hdr.type    = UDP_DATA_TYPE;
    hdr.type    = Set_endian(hdr.type);

    hdr.sender_id = My_Address;
    hdr.data_len  = buf_len;
    hdr.ack_len   = 0;
    hdr.seq_no    = Set_Loss_SeqNo(lk->other_side_node);
	
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





