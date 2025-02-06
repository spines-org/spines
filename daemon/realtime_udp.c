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
#include <winsock2.h>
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
#include "realtime_udp.h"
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
extern int       Security;
extern int       Unicast_Only;

/* Local constatnts */

static const sp_time zero_timeout  = {     0,    0};

/***********************************************************/
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

void Process_RT_UDP_data_packet(Link *lk, char *buff, int16u data_len, 
				int16u ack_len, int32u type, int mode)
{
    udp_header    *hdr = (udp_header*) buff;
    Realtime_Data *rt_data;
    int32          seq_no;
    int32          diff;
    sp_time        now;
    int            i;

    if (!Same_endian(type)) {
      Flip_udp_hdr(hdr);
    }
    
    if (hdr->len + sizeof(udp_header) != data_len) {
      Alarm(PRINT, "Process_RT_UDP_data_packet: Packed data not available yet!\r\n");
      return;
    }
  
    rt_data = (Realtime_Data*) lk->prot_data;
    seq_no  = *(int*)(buff+data_len);

    if(!Same_endian(type)) {
	seq_no = Flip_int32(seq_no);
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
    
    Deliver_and_Forward_Data(buff, data_len, mode, lk);
}

/***********************************************************/
/* int Forward_RT_UDP_data(Node *next_hop, char *buff,     */
/*                         int16u buf_len)                 */
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
    
    if (next_hop == This_Node) {
	Process_udp_data_packet(NULL, buff, buf_len, UDP_DATA_TYPE, UDP_LINK);
	return BUFF_EMPTY;
    }

    if ((lk = Get_Best_Link(next_hop->nid, REALTIME_UDP_LINK)) == NULL) {
	return BUFF_DROP;
    }

    rt_data = (Realtime_Data*) lk->prot_data;
    now     = E_get_time();

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


    scat.num_elements    = 2;
    scat.elements[0].len = sizeof(packet_header);
    scat.elements[0].buf = (char *) &hdr;
    scat.elements[1].len = buf_len+sizeof(int32);
    scat.elements[1].buf = buff;
    
    hdr.type             = REALTIME_DATA_TYPE;
    hdr.type             = Set_endian(hdr.type);

    hdr.sender_id        = My_Address;
    hdr.ctrl_link_id     = lk->leg->ctrl_link_id;
    hdr.data_len         = buf_len;
    hdr.ack_len          = sizeof(int32);
    hdr.seq_no           = Set_Loss_SeqNo(lk->leg);

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
      ret = Link_Send(lk, &scat);

      if(ret < 0) {
	return BUFF_DROP;
      }
    }
    return BUFF_EMPTY;
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

void Clean_RT_history(Link *lk)
{
    Realtime_Data *rt_data;
    sp_time        now;
    History_Cell  *h_cell;
    int            diff;

    if (lk == NULL) {
	return;
    }
    
    rt_data = lk->prot_data;
    now     = E_get_time();
    
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
    Link          *lk;
    Realtime_Data *rt_data;
    sys_scatter    scat;
    packet_header  hdr;
    int            ret;

    if ((lk = Links[linkid]) == NULL || lk->link_type != REALTIME_UDP_LINK || (rt_data = (Realtime_Data*) lk->prot_data) == NULL) {
	Alarm(EXIT, "Send_RT_nack(): link not valid\n");
	return;
    }

    scat.num_elements    = 2;			  
    scat.elements[0].len = sizeof(packet_header);
    scat.elements[0].buf = (char *)(&hdr);
    scat.elements[1].len = rt_data->num_nacks*sizeof(int32);
    scat.elements[1].buf = rt_data->nack_buff;
	
    hdr.type             = REALTIME_NACK_TYPE;
    hdr.type             = Set_endian(hdr.type);

    hdr.sender_id        = My_Address;
    hdr.ctrl_link_id     = lk->leg->ctrl_link_id;
    hdr.data_len         = 0; 
    hdr.ack_len          = rt_data->num_nacks*sizeof(int32);
    hdr.seq_no           = Set_Loss_SeqNo(lk->leg);
    
    rt_data->num_nacks = 0;
    
    /* Sending the ack*/
    if(network_flag == 1) {
      ret = Link_Send(lk, &scat);
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
   
    if ((lk = Links[linkid]) == NULL || lk->link_type != REALTIME_UDP_LINK || (rt_data = (Realtime_Data*) lk->prot_data) == NULL) {
	Alarm(EXIT, "Send_RT_nack(): link not valid\n");
	return;
    }

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

	    hdr.sender_id    = My_Address;
	    hdr.ctrl_link_id = lk->leg->ctrl_link_id;
	    hdr.data_len     = buf_len;
	    hdr.ack_len      = sizeof(int32);
	    hdr.seq_no       = Set_Loss_SeqNo(lk->leg);

	    /* Set the sequence number of the packet */
	    *(int*)(buff+buf_len) = seq_no;
    
	    if(network_flag == 1) {
	      ret = Link_Send(lk, &scat);

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
/* void Process_RT_nack_packet(Node_ID sender, char *buff,   */
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

void Process_RT_nack_packet(Link *lk, char *buf, int16u ack_len, int32u type, int mode)
{
    Realtime_Data *rt_data;
    int32 *tmp;
    int i;

    rt_data = lk->prot_data;

    if (rt_data->retransm_buff != NULL) {
      dec_ref_cnt(rt_data->retransm_buff);
    }

    tmp = (int32*)buf;
	
    if (!Same_endian(type)) {

      for (i = 0; i < ack_len / sizeof(int32); ++i) {
	*tmp = Flip_int32(*tmp);
	tmp++;
      }
    }

    rt_data->retransm_buff = buf;
    inc_ref_cnt(buf);
    rt_data->num_retransm = ack_len / sizeof(int32);

    E_queue(Send_RT_Retransm, (int)lk->link_id, NULL, zero_timeout);
}   
