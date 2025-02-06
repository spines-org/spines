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

#include <string.h>
#include <assert.h>

#ifdef ARCH_PC_WIN95
#  include <winsock2.h>
#endif

#include "arch.h"
#include "spu_alarm.h"
#include "spu_events.h"
#include "spu_memory.h"
#include "spu_data_link.h"
#include "stdutil/stdhash.h"

#include "objects.h"
#include "node.h"
#include "link.h"
#include "protocol.h"
#include "network.h"
#include "reliable_datagram.h"
#include "link_state.h"
#include "hello.h"
#include "route.h"
#include "udp.h"
#include "realtime_udp.h"
#include "session.h"
#include "state_flood.h"
#include "link_state.h"

#include "spines.h"

/***********************************************************/
/* Creates a link between the current node and some        */
/* neighbor                                                */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* leg: the leg across which this link should run          */
/* mode: mode of the link (CONTROL, UDP, etc.)             */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int16) the ID of the link in the global Link array     */
/*                                                         */
/***********************************************************/

int16 Create_Link(Network_Leg *leg,
		  int16        mode) 
{
  Edge           *edge;
  Node           *nd;
  int16           linkid;
  Link           *lk;
  Reliable_Data  *r_data;
  Control_Data   *c_data;
  Realtime_Data  *rt_data;
  int             i;

  /* validation */

  if (leg->local_interf->owner != This_Node) {
    Alarm(EXIT, "Create_Link: This_Node isn't owner of local interface?!\r\n");
  }

  edge = leg->edge;
  nd   = leg->remote_interf->owner;

  if (leg->links[mode] != NULL) {
    Alarm(EXIT, "Create_Link: This type of link already exists on this leg?!\r\n");
  }

  if (mode != CONTROL_LINK && (leg->links[CONTROL_LINK] == NULL || leg->status != CONNECTED_LEG || leg->cost == -1)) {
    Alarm(EXIT, "Create_Link: Creating a non-control link on a leg before connected?!\r\n");
  }

  /* Find an empty spot in the Links array */

  for (linkid = 0; linkid < MAX_LINKS && Links[linkid] != NULL; ++linkid);

  if (linkid == MAX_LINKS) {
    Alarm(EXIT, "Create_Link() No link IDs available; too many open links\r\n");
  }

  /* Create and initialize the link structure */

  if ((lk = (Link*) new(DIRECT_LINK)) == NULL) {
    Alarm(EXIT, "Create_Link: Cannot allocate link object\r\n");
  }

  memset(lk, 0, sizeof(*lk));

  Links[linkid]    = lk;
  leg->links[mode] = lk;

  lk->link_id      = linkid;
  lk->link_type    = mode;
  lk->r_data       = NULL;
  lk->prot_data    = NULL;
  lk->leg          = leg;

  /* Create the reliable_data structure */

  if (mode == CONTROL_LINK || mode == RELIABLE_UDP_LINK) {  

    if ((r_data = (Reliable_Data*) new(RELIABLE_DATA)) == NULL) {
      Alarm(EXIT, "Create_Link: Cannot allocte reliable_data object\r\n");
    }

    memset(r_data, 0, sizeof(*r_data));

    r_data->flags = UNAVAILABLE_LINK;
    r_data->seq_no = 0;
    stdcarr_construct(&r_data->msg_buff, sizeof(Buffer_Cell*), 0);

    for (i = 0; i < MAX_WINDOW; ++i) {
      r_data->window[i].buff      = NULL;
      r_data->window[i].data_len  = 0;
      r_data->recv_window[i].flag = EMPTY_CELL;
    }

    r_data->head = 0;
    r_data->tail = 0;
    r_data->recv_head = 0;
    r_data->recv_tail = 0;
    r_data->nack_buff = NULL;
    r_data->nack_len = 0;
    r_data->scheduled_ack = 0;
    r_data->scheduled_timeout = 0;
    r_data->timeout_multiply = 1;
    r_data->rtt = 0;
    r_data->congestion_flag = 0;
    r_data->connect_state = AVAILABLE_LINK;
    r_data->cong_flag = 1;
    r_data->last_tail_resent = 0;
    r_data->padded = 1;

    /* Congestion control */

    r_data->window_size = (float)Minimum_Window; 
    r_data->max_window = MAX_CG_WINDOW;
    r_data->ssthresh = MAX_CG_WINDOW;
      
    lk->r_data = r_data;
  }

  /* Create the control_data structure */

  if (mode == CONTROL_LINK) {

    /* update leg status */

    assert(leg->status == DISCONNECTED_LEG && leg->cost == -1);

    leg->status              = NOT_YET_CONNECTED_LEG;
    leg->cost                = -1;

    while ((leg->ctrl_link_id = rand()) == 0);  /* NOTE: 0 is a reserved ctrl link id indicating that no ctrl link has yet been created */
    leg->other_side_ctrl_link_id = 0;

    leg->hellos_out          = 0;
    leg->connect_cnter       = 0;

    if ((c_data = (Control_Data*) new(CONTROL_DATA)) == NULL) {
      Alarm(EXIT, "Create_Link: Cannot allocate ctrl_data object\r\n");
    }

    memset(c_data, 0, sizeof(*c_data));

    c_data->hello_seq            = 0;
    c_data->other_side_hello_seq = 0;
    c_data->diff_time            = 0;
    c_data->rtt                  = 1000;     /* NOTE: we initially assume a new link has bad characteristics for routing purposes */
    c_data->est_loss_rate        = UNKNOWN;
    c_data->est_tcp_rate         = UNKNOWN;
    c_data->reported_rtt         = UNKNOWN;
    c_data->reported_loss_rate   = UNKNOWN;

    /* For determining loss_rate */
    c_data->l_data.my_seq_no = PACK_MAX_SEQ;
    c_data->l_data.other_side_tail = 0;
    c_data->l_data.other_side_head = 0;
    c_data->l_data.received_packets = 0;

    for(i=0; i<MAX_REORDER; i++) {
      c_data->l_data.recv_flags[i] = 0;
    }

    for(i=0; i<LOSS_HISTORY; i++) {
      c_data->l_data.loss_interval[i].received_packets = 0;
      c_data->l_data.loss_interval[i].lost_packets = 0;
    }

    c_data->l_data.loss_event_idx = 0;	
    c_data->l_data.loss_rate = UNKNOWN;

    lk->prot_data = c_data;

    /* ensure this guy is in Neighbor_Nodes */

    if (nd->neighbor_id == -1) {

      for (nd->neighbor_id = 0; nd->neighbor_id != MAX_LINKS / MAX_LINKS_4_EDGE && Neighbor_Nodes[nd->neighbor_id] != NULL; ++nd->neighbor_id);

      if (nd->neighbor_id == MAX_LINKS / MAX_LINKS_4_EDGE) {
	Alarm(EXIT, "Network_Leg_Set_Cost: Too many neighbors!\r\n");
      }

      if (nd->neighbor_id + 1 > Num_Neighbors) {
	Num_Neighbors = nd->neighbor_id + 1;
      }

      Neighbor_Nodes[nd->neighbor_id] = nd;
    }

    /* set up hello's on this link at a random offset to avoid synchronizing all hello's */
    {
      sp_time rand_hello_TO;

      rand_hello_TO.sec  = 0;
      rand_hello_TO.usec = (long) ((1.0e6 * hello_timeout.sec + hello_timeout.usec) * (rand() / (RAND_MAX + 1.0)) + 0.5);

      if (rand_hello_TO.usec > 1000000L) {
	rand_hello_TO.sec  = rand_hello_TO.usec / 1000000L;
	rand_hello_TO.usec = rand_hello_TO.sec  % 1000000L;
      }

      E_queue(Send_Hello, lk->link_id, NULL, rand_hello_TO);
    }
  }

  /* Create the realtime_udp_data structure */

  if (mode == REALTIME_UDP_LINK) {

    if ((rt_data = (Realtime_Data*) new(REALTIME_DATA)) == NULL) {
      Alarm(EXIT, "Create_Link: Cannot allocte realtime_udp_data object\n");
    }

    memset(rt_data, 0, sizeof(*rt_data));

    rt_data->head = 0;
    rt_data->tail = 0;
    rt_data->recv_head = 0;
    rt_data->recv_tail = 0;
      
    for (i = 0; i < MAX_HISTORY; ++i) {
      rt_data->recv_window[i].flags = EMPTY_CELL;
    }
      
    rt_data->num_nacks = 0;
    rt_data->retransm_buff = NULL;
    rt_data->num_retransm = 0;
      
    rt_data->bucket = 0;
      
    lk->prot_data = rt_data;
  }

  {
    const char * str;

    switch (mode) {

    case CONTROL_LINK:
      str = "Control";
      break;

    case UDP_LINK:
      str = "UDP";
      break;

    case RELIABLE_UDP_LINK:
      str = "Reliable";
      break;

    case REALTIME_UDP_LINK:
      str = "RealTime";
      break;

    default:
      str = "Unknown";
      break;
    }

    Alarm(PRINT, "Create_Link: edge = (" IPF " -> " IPF "); leg = (" IPF " -> " IPF "); linkid = %d; type = %s\r\n",
	  IP(edge->src_id), IP(edge->dst_id), IP(leg->local_interf->iid), IP(leg->remote_interf->iid), 
	  (int) linkid, str);
  }

  return linkid;
}

/***********************************************************/
/* void Destroy_Link(int16 linkid)                         */
/*                                                         */
/* Destroys a link between the current node and some       */
/* neighbor                                                */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid: the ID of the link in the global Link array     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Destroy_Link(int16 linkid)
{
    Link *lk;
    Network_Leg *leg;
    Edge *edge;
    int32u i;
    Control_Data  *c_data;
    Reliable_Data *r_data;
    Realtime_Data *rt_data;
    stdit it;
    Buffer_Cell *buf_cell;
    char* msg;

    if ((lk = Links[linkid]) == NULL) {
      Alarm(EXIT, "Destroy_Link: no link %hd!\r\n", linkid);
    }

    leg  = lk->leg;
    edge = leg->edge;
    
    /* Take care about the reliability stuff */
    if(lk->r_data != NULL) {
	r_data = lk->r_data;

	/* Remove data from the window */
	for(i=r_data->tail; i<=r_data->head; i++) {
	    if(r_data->window[i%MAX_WINDOW].buff != NULL) {
		dec_ref_cnt(r_data->window[i%MAX_WINDOW].buff);
	    }
	    r_data->window[i%MAX_WINDOW].buff = NULL;
	}

	for(i=r_data->recv_tail; i<=r_data->recv_head; i++) {
	    if(r_data->recv_window[i%MAX_WINDOW].data.buff != NULL) {
		dec_ref_cnt(r_data->recv_window[i%MAX_WINDOW].data.buff);
		r_data->recv_window[i%MAX_WINDOW].data.buff = NULL;
	    }
	}
	
	/* Remove data from the queue */
	while(!stdcarr_empty(&(r_data->msg_buff))) {
	    stdcarr_begin(&(r_data->msg_buff), &it);
	    
	    buf_cell = *((Buffer_Cell **)stdcarr_it_val(&it));
	    msg = buf_cell->buff;
	    dec_ref_cnt(msg);
	    dispose(buf_cell);
	    stdcarr_pop_front(&(r_data->msg_buff));
	}
	stdcarr_destruct(&(r_data->msg_buff));

	if(r_data->nack_buff != NULL) {
	    E_dequeue(Send_Nack_Retransm, (int)linkid, NULL);
	    dispose(r_data->nack_buff);
	    r_data->nack_len = 0;
	}

	if(r_data->scheduled_timeout == 1) {
	    E_dequeue(Reliable_timeout, (int)linkid, NULL);
	}
	r_data->scheduled_timeout = 0;

	if(r_data->scheduled_ack == 1) {
	    E_dequeue(Send_Ack, (int)linkid, NULL);
	}
	r_data->scheduled_ack = 0;

	E_dequeue(Try_to_Send, (int)linkid, NULL);

	if (lk->link_type == CONTROL_LINK) {
	    E_dequeue(Send_Hello, (int)linkid, NULL);
	    E_dequeue(Send_Hello_Request, (int)linkid, NULL);
	    E_dequeue(Send_Hello_Request_Cnt, (int) linkid, NULL);
	    E_dequeue(Net_Send_State_All, (int)linkid, &Edge_Prot_Def);
	    E_dequeue(Net_Send_State_All, (int)linkid, &Groups_Prot_Def);
	}
	dispose(r_data);

	if(Link_Sessions_Blocked_On == linkid) {
	    Resume_All_Sessions();
	    Link_Sessions_Blocked_On = -1;
	}
    }

    /* Protocol data */

    if(lk->prot_data != NULL) {
        if(lk->link_type == CONTROL_LINK) {
	    c_data = (Control_Data*)lk->prot_data;
	    dispose(c_data);
	}
        else if(lk->link_type == REALTIME_UDP_LINK) {
	    rt_data = (Realtime_Data*)lk->prot_data;
	    for(i=rt_data->tail; i<rt_data->head; i++) {
		if(rt_data->window[i%MAX_HISTORY].buff != NULL) {
		    dec_ref_cnt(rt_data->window[i%MAX_HISTORY].buff);
		    rt_data->window[i%MAX_HISTORY].buff = NULL;
		}
	    }	
	    E_dequeue(Send_RT_Nack, (int)linkid, NULL);
	    E_dequeue(Send_RT_Retransm, (int)linkid, NULL);

	    if(rt_data->retransm_buff != NULL) {
		dec_ref_cnt(rt_data->retransm_buff);
		rt_data->retransm_buff = NULL;
	    }

	    dispose(rt_data);
	    Alarm(DEBUG, "Destroyed Realtime UDP link\n");
	}
    }

    lk->leg->links[lk->link_type] = NULL;
    Links[linkid] = NULL;

  {
    const char * str;

    switch (lk->link_type) {

    case CONTROL_LINK:
      str = "Control";
      break;

    case UDP_LINK:
      str = "UDP";
      break;

    case RELIABLE_UDP_LINK:
      str = "Reliable";
      break;

    case REALTIME_UDP_LINK:
      str = "RealTime";
      break;

    default:
      str = "Unknown";
      break;
    }

    Alarm(PRINT, "Destroy_Link: edge = (" IPF " -> " IPF "); leg = (" IPF " -> " IPF "); linkid = %d; type = %s\r\n",
	  IP(edge->src_id), IP(edge->dst_id), IP(leg->local_interf->iid), IP(leg->remote_interf->iid), 
	  (int) linkid, str);
  }

    dispose(lk);
}

Link *Get_Best_Link(Node_ID node_id, int mode)
{
  Link *link = NULL;
  Edge *edge;

  if ((edge = Get_Edge(My_Address, node_id)) != NULL && edge->cost != -1) {
    link = edge->leg->links[mode];
  }

  return link;
}

int Link_Send(Link *lk, sys_scatter *scat)
{
  return DL_send(lk->leg->local_interf->channels[lk->link_type], 
		 lk->leg->remote_interf->net_addr,
		 Port + lk->link_type,
		 scat);
}

/***********************************************************/
/* int32 Relative_Position(int32 base, int32 seq)          */
/*                                                         */
/* Computes the relative position between two sequence     */
/* numbers in a circular array.                            */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* base: base sequence number                              */
/* seq_no: sequence number of offset                       */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int32) offset                                          */
/*                                                         */
/***********************************************************/

int32 Relative_Position(int32 base, int32 seq) {
    if(seq >= base) {
	if(seq - base < PACK_MAX_SEQ/2) {
            return(seq - base);
	}
	else {
            /* this is an old packet */
            return(seq - base - PACK_MAX_SEQ);
	}
    }
    else {
	if(base - seq > PACK_MAX_SEQ/2) {
            return(seq + PACK_MAX_SEQ - base);
	}
	else {
            /* this is an old packet */
            return(seq - base);
	}
    }
}

/***********************************************************/
/* Checks the link loss probability on link                */
/* between two neighbours                                  */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender: IP of the neighbour                             */
/* seq_no: sequence number of the packet received          */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Check_Link_Loss(Network_Leg *leg, int16u seq_no) 
{
  Loss_Data *l_data;
  int32      head;
  int32      tail;
  int32      sum;
  int32      i;

  assert(leg->links[CONTROL_LINK] != NULL);

  l_data = &((Control_Data*) leg->links[CONTROL_LINK]->prot_data)->l_data;

  if (seq_no == 0xffff) {  /* special sequence # used by hello pings (hello sent on dead legs) */
    goto END;              /* ignore */
  }

  tail = l_data->other_side_tail;
  head = l_data->other_side_head;

  if (Relative_Position(head, seq_no % PACK_MAX_SEQ) >= 0) {
    l_data->other_side_head = (seq_no + 1) % PACK_MAX_SEQ;

  } else if (Relative_Position(tail, seq_no % PACK_MAX_SEQ) <= 0) {
    return;                /* ignore; older than our reorder/loss window */
  }

  /* if seq is more than 4 positions ahead of our "ARU" declare a loss event (implies at least one hole at "ARU" + 1) */

  /* TODO: logic isn't great: 
     1) we can give a TON of weight to a long history of no losses until it is pushed out of LOSS_HISTORY
     2) once we declare a loss we run all the way up to seq counting any hole as loss (i.e. - no chance for later reorders to fill)
     3) history can hang around a LONG time
  */
 
  if (Relative_Position(tail, seq_no % PACK_MAX_SEQ) > 4) {  /* LOSS EVENT!!!! */

    /* calculate and record loss stats */

    l_data->loss_interval[l_data->loss_event_idx % LOSS_HISTORY].received_packets = l_data->received_packets;

    for (sum = 0, i = l_data->other_side_tail + 1; i != seq_no % PACK_MAX_SEQ; i = (i + 1) % PACK_MAX_SEQ) {

      if (l_data->recv_flags[i % MAX_REORDER] == 0) {
	++sum;

      } else {
	l_data->recv_flags[i % MAX_REORDER] = 0;
      }
    }
	    
    l_data->loss_interval[l_data->loss_event_idx % LOSS_HISTORY].lost_packets = sum;
	    
    Alarm(DEBUG, "LOSS!!! event: %d; received: %d; lost: %d\n", l_data->loss_event_idx,
	  l_data->loss_interval[l_data->loss_event_idx % LOSS_HISTORY].received_packets,
	  l_data->loss_interval[l_data->loss_event_idx % LOSS_HISTORY].lost_packets);

    /* reset records for next loss event */
		
    l_data->other_side_tail = l_data->other_side_head;
    ++l_data->loss_event_idx;
    l_data->received_packets = 0;

    for (i = 0; i < MAX_REORDER; ++i) {
      l_data->recv_flags[i % MAX_REORDER] = 0;	    
    }
  }

  /* record reciept */

  if (l_data->recv_flags[seq_no % MAX_REORDER] == 0) {
    l_data->recv_flags[seq_no % MAX_REORDER] = 1;

    if (l_data->received_packets < PACK_MAX_SEQ / 2) {  /* NOTE: don't let received_packets grow w/o bound -> avoids rollover and keeps weight of clean history more reasonable */
      ++l_data->received_packets;
    }
  }

  /* advance tail if possible */

  while(l_data->recv_flags[(l_data->other_side_tail + 1) % MAX_REORDER] == 1 &&
	(l_data->other_side_tail + 1) % PACK_MAX_SEQ != l_data->other_side_head) {
    l_data->recv_flags[l_data->other_side_tail % MAX_REORDER] = 0;
    l_data->other_side_tail = (l_data->other_side_tail + 1) % PACK_MAX_SEQ;
  }

 END:
  return;
}

/***********************************************************/
/* Computes the average loss rate from a neighbor          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* nd: The neighbor node                                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int32) average loss rate multiplied by LOSS_RATE_SCALE */
/*                                                         */
/***********************************************************/

int32 Compute_Loss_Rate(Network_Leg *leg) 
{
  Loss_Data *l_data;
  int        total_recvd;
  int        total_lost;
  int        i;
  int        j;

  assert(leg->links[CONTROL_LINK] != NULL);

  l_data = &((Control_Data*) leg->links[CONTROL_LINK]->prot_data)->l_data;

  total_recvd = l_data->received_packets;
  total_lost  = 0;

  for (i = 0, j = MIN(LOSS_HISTORY, l_data->loss_event_idx); i < j; ++i) {
    total_recvd += l_data->loss_interval[i].received_packets;
    total_lost  += l_data->loss_interval[i].lost_packets;
  }

  if (total_recvd + total_lost <= 10) {
    return UNKNOWN;
  }

  l_data->loss_rate = (float) total_lost / (total_recvd + total_lost);

  return (int32) (LOSS_RATE_SCALE * l_data->loss_rate);
}

/***********************************************************/
/* Sets the sequence number of a packet                    */
/*             (for detecting loss rate)                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* nd: The neighbor node                                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int16) Sequence number                                 */
/*                                                         */
/***********************************************************/

int16u Set_Loss_SeqNo(Network_Leg *leg) 
{
  Loss_Data *l_data;

  assert(leg->links[CONTROL_LINK] != NULL);

  l_data = &((Control_Data*) leg->links[CONTROL_LINK]->prot_data)->l_data;

  if (++l_data->my_seq_no >= PACK_MAX_SEQ) {
    l_data->my_seq_no = 0;
  }

  return l_data->my_seq_no;
}
