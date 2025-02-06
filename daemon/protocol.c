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
#include <assert.h>

#ifdef ARCH_PC_WIN95
#include <winsock2.h>
#endif

#include "arch.h"
#include "spu_alarm.h"
#include "spu_events.h"
#include "spu_data_link.h"
#include "spu_memory.h"
#include "stdutil/stdhash.h"

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

extern int16u Port;

/* Statistics */

extern int64_t total_udp_pkts;
extern int64_t total_udp_bytes;
extern int64_t total_rel_udp_pkts;
extern int64_t total_rel_udp_bytes;
extern int64_t total_link_ack_pkts;
extern int64_t total_link_ack_bytes;
extern int64_t total_hello_pkts;
extern int64_t total_hello_bytes;
extern int64_t total_link_state_pkts;
extern int64_t total_link_state_bytes;
extern int64_t total_group_state_pkts;
extern int64_t total_group_state_bytes;

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

/* scat is the receiving scatter (Global!). 
   At the return of the function I should either:
       - finish processing of scat, so it can be reused for receiving
       - allocate a new scat header/body so I have smthg to receive in 
*/

void Prot_process_scat(sys_scatter *scat, int total_bytes, Interface *local_interf, int mode, int32 type, Network_Address from_addr, int16u from_port)
{
  packet_header *pack_hdr        = (packet_header*) scat->elements[0].buf;
  int            remaining_bytes = total_bytes - scat->elements[0].len;
  Interface     *remote_interf   = NULL;
  Network_Leg   *leg             = NULL;
  Link          *link            = NULL;
  
  if (remaining_bytes != pack_hdr->data_len + pack_hdr->ack_len) {
    Alarm(PRINT, "Prot_process_scat: Got a corrupted message; wrong sizes: %d != %d + %d!\r\n", remaining_bytes, pack_hdr->data_len, pack_hdr->ack_len);
    return;
  }

  if (from_port != Port + mode) {
    Alarm(PRINT, "Prot_process_scat: Recvd a msg on port %d from an unequal remote port (%d)!\r\n", Port + mode, (int) from_port);
    return;
  }

  /* look up any existing remote interface, network leg and link on which this msg arrived */

  if ((remote_interf = Get_Interface_by_Addr(from_addr)) != NULL) {

    if (remote_interf->owner->nid != pack_hdr->sender_id) {
      Alarm(PRINT, "Prot_process_scat: Sender ID (" IPF ") doesn't match known interface information (" IPF ") based on src address (" IPF ")! Dropping!\r\n",
	    IP(pack_hdr->sender_id), IP(remote_interf->owner->nid), IP(from_addr));
      return;
    }

    if ((leg = Get_Network_Leg(local_interf->iid, remote_interf->iid)) != NULL) {
      link = leg->links[mode];
    }
  }

  /* process hello's specially, regardless of lookup results */

  if (Is_hello_type(pack_hdr->type)) {

    if (mode == CONTROL_LINK) {

      Alarm(DEBUG, "\n\nprocess_hello*: size: %d\n", total_bytes);

      total_hello_pkts++;
      total_hello_bytes += total_bytes;

      Process_hello_ping(pack_hdr, from_addr, local_interf, &remote_interf, &leg, &link);  /* try to create remote_interf, leg + ctrl link on demand */

      if (link != NULL) {

	if (Is_hello(pack_hdr->type) || Is_hello_req(pack_hdr->type)) {
	  Process_hello_packet(link, pack_hdr, scat->elements[1].buf, remaining_bytes, type);
	  /* NOTE: Check_Link_Loss() called in Process_hello_packet if appropriate */
	}
      }

    } else {
      Alarm(PRINT, "Prot_process_scat: Bad mode %d for hello msg type! Dropping!\r\n", mode);
      return;
    }

  } else if (link != NULL) {  /* call the appropriate processing function for the packet if it was received on a link */

    assert(leg->links[CONTROL_LINK] != NULL && link->link_type == mode);

    if (pack_hdr->ctrl_link_id != leg->other_side_ctrl_link_id) {
      Alarm(PRINT, "Prot_process_scat: Other side ctrl id 0x%x doesn't match current one 0x%x! Dropping!\n", pack_hdr->ctrl_link_id, leg->other_side_ctrl_link_id);
      return;
    }

    Check_Link_Loss(leg, pack_hdr->seq_no);

    switch (mode) {

    case CONTROL_LINK:

      if (Is_link_state(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_state_packet: (link) size: %d\n", total_bytes);

	total_link_state_pkts++;
	total_link_state_bytes += total_bytes;

	Process_state_packet(link, scat->elements[1].buf, pack_hdr->data_len, pack_hdr->ack_len, type, mode);

      } else if (Is_group_state(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_state_packet: (group) size: %d\n", total_bytes);

	total_group_state_pkts++;
	total_group_state_bytes += total_bytes;   	    

	Process_state_packet(link, scat->elements[1].buf, pack_hdr->data_len, pack_hdr->ack_len, type, mode);

      } else if (Is_link_ack(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_ack: size: %d\n", total_bytes);

	total_link_ack_pkts++;
	total_link_ack_bytes += total_bytes;
 
	Process_ack_packet(link, scat->elements[1].buf, pack_hdr->ack_len, type, mode);

      } else {
	Alarm(PRINT, "Prot_process_scat: Unexpected msg type 0x%x for mode %d! Dropping!\r\n", pack_hdr->type, mode);
      }

      break;

    case UDP_LINK:

      if (Is_udp_data(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_udp_data: size: %d\n", total_bytes);   

	total_udp_pkts++;
	total_udp_bytes += total_bytes;

	Process_udp_data_packet(link, scat->elements[1].buf, pack_hdr->data_len, type, mode);

      } else {
	Alarm(PRINT, "Prot_process_scat: Unexpected msg type 0x%x for mode %d! Dropping!\r\n", pack_hdr->type, mode);
      }
      
      break;

    case RELIABLE_UDP_LINK:

      if (Is_rel_udp_data(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_rel_udp_data: size: %d\n", total_bytes);    

	total_rel_udp_pkts++;
	total_rel_udp_bytes += total_bytes;

	Process_rel_udp_data_packet(link, scat->elements[1].buf, pack_hdr->data_len, pack_hdr->ack_len, type, mode);

      } else if (Is_link_ack(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_ack: size: %d\n", total_bytes);
	
	total_link_ack_pkts++;
	total_link_ack_bytes += total_bytes;
	
	Process_ack_packet(link, scat->elements[1].buf, pack_hdr->ack_len, type, mode);

      } else {
	Alarm(PRINT, "Prot_process_scat: Unexpected msg type 0x%x for mode %d! Dropping!\r\n", pack_hdr->type, mode);
      }
     
      break;

    case REALTIME_UDP_LINK:

      if (Is_realtime_data(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_realtime_udp_data: size: %d\n", total_bytes); 
	
	total_udp_pkts++;
	total_udp_bytes += total_bytes;
	
	Process_RT_UDP_data_packet(link, scat->elements[1].buf, pack_hdr->data_len, pack_hdr->ack_len, type, mode);
	
      } else if(Is_realtime_nack(pack_hdr->type)) {
	Alarm(DEBUG, "\n\nprocess_realtime_nack: size: %d\n", total_bytes);
	
	total_link_ack_pkts++;
	total_link_ack_bytes += total_bytes;
 
	Process_RT_nack_packet(link, scat->elements[1].buf, pack_hdr->ack_len, type, mode);

      } else {
	Alarm(PRINT, "Prot_process_scat: Unexpected msg type 0x%x for mode %d! Dropping!\r\n", pack_hdr->type, mode);
      }
      
      break;

    case RESERVED0_LINK:
    case RESERVED1_LINK:
    case RESERVED2_LINK:
    case MAX_LINKS_4_EDGE:
    default:
      Alarm(EXIT, "Prot_process_scat: Unrecognized mode %d! BUG!!\r\n", mode);
      break;
    }

  } else {
    Alarm(DEBUG, "Prot_process_scat: Dropping message of type 0x%x in mode %d from " IPF " due to lack of link!\r\n", pack_hdr->type, mode, IP(from_addr));
  }

  /* check if the buffer is still needed */

  if (get_ref_cnt(scat->elements[1].buf) > 1) {

    dec_ref_cnt(scat->elements[1].buf);

    if ((scat->elements[1].buf = (char*) new_ref_cnt(PACK_BODY_OBJ)) == NULL) {
      Alarm(EXIT, "Prot_process_scat: Could not allocate packet body obj\r\n");
    } 	    
  }
}
