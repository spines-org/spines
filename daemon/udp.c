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
 *  Yair Amir, Claudiu Danilov and John Schultz.
 *
 * Copyright (c) 2003 - 2013 The Johns Hopkins University.
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

#ifdef ARCH_PC_WIN95
#  include <winsock2.h>
#endif

#include "arch.h"
#include "spu_alarm.h"
#include "spu_events.h"
#include "spu_memory.h"
#include "spu_data_link.h"
#include "stdutil/stdhash.h"
#include "stdutil/stdcarr.h"

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

extern Node     *This_Node;
extern Node_ID   My_Address;
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

void Flip_udp_hdr(udp_header *udp_hdr)
{
    udp_hdr->source	  = Flip_int32( udp_hdr->source );
    udp_hdr->dest	  = Flip_int32( udp_hdr->dest );
    udp_hdr->source_port  = Flip_int16( udp_hdr->source_port );
    udp_hdr->dest_port	  = Flip_int16( udp_hdr->dest_port );
    udp_hdr->len	  = Flip_int16( udp_hdr->len );
    udp_hdr->seq_no	  = Flip_int16( udp_hdr->seq_no );
    udp_hdr->sess_id	  = Flip_int16( udp_hdr->sess_id );
}

void Copy_udp_header(udp_header *from_udp_hdr, udp_header *to_udp_hdr)
{
    to_udp_hdr->source	    = from_udp_hdr->source;
    to_udp_hdr->dest	    = from_udp_hdr->dest;
    to_udp_hdr->source_port = from_udp_hdr->source_port;
    to_udp_hdr->dest_port   = from_udp_hdr->dest_port;
    to_udp_hdr->len	    = from_udp_hdr->len;
    to_udp_hdr->seq_no	    = from_udp_hdr->seq_no;
    to_udp_hdr->sess_id	    = from_udp_hdr->sess_id;
    to_udp_hdr->frag_num    = from_udp_hdr->frag_num;
    to_udp_hdr->frag_idx    = from_udp_hdr->frag_idx;
    to_udp_hdr->ttl         = from_udp_hdr->ttl;
    to_udp_hdr->routing     = from_udp_hdr->routing;
}

/***********************************************************/
/* Processes a UDP data packet                             */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* lk:        link upon which this packet was recvd        */
/* buff:      a buffer containing the message              */
/* data_len:  length of the data in the packet             */
/* type:      type of the packet                           */
/* mode:      mode of the link the packet arrived on       */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Process_udp_data_packet(Link *lk, char *buff, int16u data_len, int32u type, int mode)
{
  /* NOTE: lk can be NULL -> self delivery */

  udp_header *hdr = (udp_header*) buff;
    
  if (!Same_endian(type)) {
    Flip_udp_hdr(hdr);
  }

  if (hdr->len + sizeof(udp_header) != data_len) {
    Alarm(PRINT, "Process_udp_data_packet: Packed data not available yet!\r\n");
    return;
  }
  
  Deliver_and_Forward_Data(buff, data_len, mode, lk);
}

/***********************************************************/
/* Forward a UDP data packet                               */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* next_hop:  the next node on the path                    */
/* buff:      buffer containing the message                */
/* buf_len:   length of the packet                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the status of the packet (see udp.h)              */
/*                                                         */
/***********************************************************/

int Forward_UDP_Data(Node *next_hop, char *buff, int16u buf_len)
{
  Link         *lk;
  packet_header hdr;
  sys_scatter   scat;
  int           ret;

  if (next_hop == This_Node) {
    Process_udp_data_packet(NULL, buff, buf_len, UDP_DATA_TYPE, UDP_LINK);
    return BUFF_EMPTY;
  }

  if ((lk = Get_Best_Link(next_hop->nid, UDP_LINK)) == NULL) {
    return BUFF_DROP;
  }
    
  scat.num_elements    = 2;
  scat.elements[0].len = sizeof(packet_header);
  scat.elements[0].buf = (char*) &hdr;
  scat.elements[1].len = buf_len;
  scat.elements[1].buf = buff;
    
  hdr.type             = UDP_DATA_TYPE;
  hdr.type             = Set_endian(hdr.type);

  hdr.sender_id        = My_Address;
  hdr.ctrl_link_id     = lk->leg->ctrl_link_id;
  hdr.data_len         = buf_len;
  hdr.ack_len          = 0;
  hdr.seq_no           = Set_Loss_SeqNo(lk->leg);
	
  if(network_flag == 1) {
    ret = Link_Send(lk, &scat);

    if (ret < 0) {
      return BUFF_DROP;
    }
  }
    
  return BUFF_EMPTY;
}
