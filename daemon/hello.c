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


#ifndef	ARCH_PC_WIN95

#include <netdb.h>
#include <sys/socket.h>

#else

#include <winsock2.h>

#endif

#include <stdlib.h>
#include <math.h>

#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/data_link.h"

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "link.h"
#include "network.h"
#include "reliable_datagram.h"
#include "hello.h"
#include "link_state.h"
#include "protocol.h"
#include "udp.h"
#include "realtime_udp.h"
#include "route.h"
#include "state_flood.h"
#include "stdutil/src/stdutil/stdhash.h"


/* Global variables */

extern int32     My_Address;
extern int16     Port;
extern int16     Num_Neighbors;
extern int16     Num_Discovery_Addresses;
extern int32     Discovery_Address[MAX_DISCOVERY_ADDR];
extern stdhash   All_Nodes;
extern stdhash   All_Edges;
extern Node*     Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
extern Link*     Links[MAX_LINKS];
extern int       network_flag;
extern int       Route_Weight;
extern channel   Local_Recv_Channels[MAX_LINKS_4_EDGE];
extern Prot_Def  Edge_Prot_Def;
extern Prot_Def  Groups_Prot_Def;
extern int       Security;
extern int       Wireless;
extern int       Wireless_ts;
extern int       Wireless_monitor;

/* Local variables */

static const sp_time zero_timeout   = {     0,    0};
static       sp_time short_timeout  = {     0,    50000};  /* 50 milliseconds */
static       sp_time cnt_timeout    = {     0,    100000}; /* 100 milliseconds */
static       sp_time hello_timeout  = {     1,    0};      /* 1 seconds */
static       sp_time ad_timeout     = {     6,    0};      /* 6 seconds */

int          hello_cnt_start        = (int)(0.7*DEAD_LINK_CNT); 
int          stable_delay_flag      = 0;
int          stable_timeout         = 0;

void Flip_hello_pkt( hello_packet *hello_pkt )
{
    hello_pkt->seq_no          = Flip_int32(hello_pkt->seq_no);
    hello_pkt->my_time_sec     = Flip_int32(hello_pkt->my_time_sec);
    hello_pkt->my_time_usec    = Flip_int32(hello_pkt->my_time_usec);
    hello_pkt->response_seq_no = Flip_int32(hello_pkt->response_seq_no);
    hello_pkt->diff_time       = Flip_int32(hello_pkt->diff_time);
    hello_pkt->loss_rate       = Flip_int32(hello_pkt->loss_rate);
}



/***********************************************************/
/* Init_Connections(void)                                  */
/*                                                         */
/* Starts hello protocol on all the known links,           */
/* initializing the protocols on these links               */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Init_Connections(void)
{
    int16 linkid;
    sp_time now, rand_hello_timeout;

    now = E_get_time();
    srand(now.usec);

    if (Wireless) {
        hello_timeout.sec = 2;
        cnt_timeout.usec = 200000;
        hello_cnt_start = (int)(0.3*DEAD_LINK_CNT); 
        stable_delay_flag = 1;
    } 

    /* Start the hello protocol on all the known links */
    for(linkid=0; linkid<MAX_LINKS && Links[linkid] != NULL; linkid++)  {
        /* Try to avoid synchronized hello */
        rand_hello_timeout.sec = (int)(rand()%(hello_timeout.sec+1));
        rand_hello_timeout.usec = (int)(rand()%(1000000));
	E_queue(Send_Hello, linkid, NULL, rand_hello_timeout);
    }

    /* Start the link recover protocol (hello ping) */
    if (!Wireless || (Wireless && Num_Discovery_Addresses == 0) ) {
        Send_Hello_Ping(0, NULL); 
    }

    /* Start Discovery of neighbors if requested */
    if (Num_Discovery_Addresses > 0) {
        Send_Discovery_Hello_Ping(0, NULL);
    }

    if (stable_delay_flag) {
        stable_timeout = hello_timeout.sec*hello_cnt_start +
            (cnt_timeout.usec*(DEAD_LINK_CNT-hello_cnt_start+1))/1000000 + hello_timeout.sec;
    } 
}


/***********************************************************/
/* Send_Hello(int linkid, void *dummy)                     */
/*                                                         */
/* Called periodically by the event system.                */
/* Sends a hello message on a link and declares the link   */
/* dead if a number of hello msgs hav already been sent    */
/* without an ack                                          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid - the id of the link in the global array         */
/*                                                         */
/***********************************************************/

void Send_Hello(int linkid, void* dummy)
{
    int cnt;
    
    if(Links[linkid] == NULL) {
	Alarm(DEBUG, "Send Hello on a non existing link !\n");
	return;
    }
    cnt = Links[linkid]->other_side_node->counter++;
    
    /*
     *Alarm(PRINT, "hello counter: %d\n", Links[linkid]->other_side_node->counter);
     */
 
    if(cnt > DEAD_LINK_CNT) {
        Alarm(DEBUG, "Disconecting node %d.%d.%d.%d\n", 
	      IP1(Links[linkid]->other_side_node->address),
	      IP2(Links[linkid]->other_side_node->address),
	      IP3(Links[linkid]->other_side_node->address),
	      IP4(Links[linkid]->other_side_node->address));

        Disconnect_Node(Links[linkid]->other_side_node->address);
    }
    else {
        Net_Send_Hello((int16u)linkid, 0);
	Clean_RT_history(Links[linkid]->other_side_node);
	E_queue(Send_Hello, linkid, NULL, hello_timeout);
	
	if(cnt > hello_cnt_start) {
	    Alarm(DEBUG, "cnt: %d :: %d.%d.%d.%d\n", cnt,
		  IP1(Links[linkid]->other_side_node->address),
		  IP2(Links[linkid]->other_side_node->address),
		  IP3(Links[linkid]->other_side_node->address),
		  IP4(Links[linkid]->other_side_node->address));

    	    E_queue(Send_Hello_Request_Cnt, linkid, NULL, cnt_timeout);
        }
    }
}



/***********************************************************/
/* Send_Hello_Request(int linkid, void *dummy)             */
/*                                                         */
/* Called by the event system.                             */
/* Sends a hello message on a link and requests an         */
/* immediate response (another hello message) from the     */
/* receiver                                                */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid: the ID of the link in the global array          */
/*                                                         */
/***********************************************************/

void Send_Hello_Request(int linkid, void* dummy)
{ 
    //    Alarm(PRINT, "Send_Hello_Request\n");
    Net_Send_Hello((int16u)linkid, 1);
}



/***********************************************************/
/* Send_Hello_Request_Cnt(int linkid, void *dummy)         */
/*                                                         */
/* Called by the event system.                             */
/* Sends a hello message on a link and requests an         */
/* immediate response (another hello message) from the     */
/* receiver. Cuts the link after a while if there is no    */
/* response                                                */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid: the ID of the link in the global array          */
/*                                                         */
/***********************************************************/

void Send_Hello_Request_Cnt(int linkid, void* dummy)
{
    int cnt;
    
    if(Links[linkid] == NULL) {
	Alarm(DEBUG, "Send Hello on a non existing link !\n");
	return;
    }

    //    Alarm(PRINT, "Send_Hello_Request_Cnt\n");


    cnt = Links[linkid]->other_side_node->counter++;
    
    if(cnt > DEAD_LINK_CNT) {
        Alarm(PRINT, "Disconnecting node %d.%d.%d.%d\n", 
	      IP1(Links[linkid]->other_side_node->address),
	      IP2(Links[linkid]->other_side_node->address),
	      IP3(Links[linkid]->other_side_node->address),
	      IP4(Links[linkid]->other_side_node->address));

        Disconnect_Node(Links[linkid]->other_side_node->address);
    }
    else {
        Net_Send_Hello((int16u)linkid, 1);
	E_queue(Send_Hello_Request_Cnt, linkid, NULL, cnt_timeout);
    }
}




/***********************************************************/
/* Send_Discovery(int linkid, void *dummy)                 */
/*                                                         */
/* Called periodically by the event system.                */
/* Sends a hello message on autodiscovery link             */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Send_Discovery_Hello_Ping(int dummy_int, void* dummy)
{ 
    int i;
    for (i=0;i<Num_Discovery_Addresses;i++) {
        Net_Send_Hello_Ping(Discovery_Address[i]); 
    }
    E_queue(Send_Discovery_Hello_Ping, 0, NULL, ad_timeout);
}



/***********************************************************/
/* Send_Hello_Ping(int linkid, void *dummy)                */
/*                                                         */
/* Called periodically by the event system.                */
/* Sends a hello message on dead links and requests a      */
/* response (another hello message) from the reciver       */
/* It is used to determine if a crashed/partitioned        */
/* node recovered                                          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Send_Hello_Ping(int dummy_int, void* dummy)
{
    stdit it, src_it;
    Edge *edge;
    State_Chain *s_chain;

    //    Alarm(PRINT, "Send_Hello_Ping\n");

    stdhash_find(&All_Edges, &src_it, &My_Address);
    if(!stdhash_is_end(&All_Edges, &src_it)) {
	s_chain = *((State_Chain **)stdhash_it_val(&src_it));
	stdhash_begin(&s_chain->states, &it);
	while(!stdhash_is_end(&s_chain->states, &it)) {
	    edge = *((Edge **)stdhash_it_val(&it));
	    if(edge->cost < 0)
		Net_Send_Hello_Ping(edge->dest->address);       
	    stdhash_it_next(&it);
	}
    }
    E_queue(Send_Hello_Ping, 0, NULL, hello_timeout);
}



/***********************************************************/
/* Net_Send_Hello(int16 linkid, int mode)                  */
/*                                                         */
/* Sends a hello message on a given link.                  */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* linkid: the ID of the link in the global array          */
/*                                                         */
/* mode: 1 if an immediate response is requested,          */
/*       0 otherwise                                       */
/*                                                         */
/***********************************************************/

void Net_Send_Hello(int16 linkid, int mode) 
{
    hello_packet pkt;
    packet_header hdr;
    sys_scatter scat;
    Control_Data *c_data;
    sp_time now;
    int ret;

    now = E_get_time();

    scat.num_elements = 2;
    scat.elements[0].len = sizeof(packet_header);
    scat.elements[0].buf = (char *) &hdr;
    scat.elements[1].len = sizeof(hello_packet);
    scat.elements[1].buf = (char *) &pkt;

    if(Links[linkid]->prot_data == NULL)
	Alarm(EXIT, "Net_Send_Hello: control_data unavailable\n");
	
    if(mode == 0)
	hdr.type = HELLO_TYPE;
    else
	hdr.type = HELLO_REQ_TYPE;
	
    hdr.type      = Set_endian(hdr.type);
    hdr.sender_id = My_Address;
    hdr.data_len  = sizeof(hello_packet);
    hdr.ack_len   = 0;
    hdr.seq_no = Set_Loss_SeqNo(Links[linkid]->other_side_node);

    if(Links[linkid]->prot_data == NULL)
	Alarm(EXIT, "Net_Send_Hello: control_data unavailable\n");
	
    c_data = Links[linkid]->prot_data;
    pkt.seq_no       = c_data->hello_seq++;
    pkt.my_time_sec  = (int32)now.sec;
    pkt.my_time_usec = (int32)now.usec;
    pkt.response_seq_no = c_data->other_side_hello_seq;
    pkt.diff_time    = c_data->diff_time;
    pkt.loss_rate    = Compute_Loss_Rate(Links[linkid]->other_side_node);
    
    if(network_flag == 1) {
#ifdef SPINES_SSL
        if (!Security) {
#endif
	    ret = DL_send(Links[linkid]->chan, 
			  Links[linkid]->other_side_node->address,
			  Links[linkid]->port, 
			  &scat );
#ifdef SPINES_SSL
	} else {
	    ret = DL_send_SSL(Links[linkid]->chan, 
			      Links[linkid]->link_node_id,
			      Links[linkid]->other_side_node->address,
			      Links[linkid]->port, 
			      &scat );
	}
#endif
    }
}


/***********************************************************/
/* Net_Send_Hello_Ping(int32 address)                      */
/*                                                         */
/* Sends a hello message on a dead link.                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* address: the IP address of the destination              */
/*                                                         */
/***********************************************************/


void Net_Send_Hello_Ping(int32 address) 
{
    hello_packet pkt;
    packet_header hdr;
    sys_scatter scat;
    sp_time now;
    int ret;

    now = E_get_time();

    //    Alarm(PRINT, "Net_Send_Hello_Ping\n");

    scat.num_elements = 2;
    scat.elements[0].len = sizeof(packet_header);
    scat.elements[0].buf = (char *) &hdr;
    scat.elements[1].len = sizeof(hello_packet);
    scat.elements[1].buf = (char *) &pkt;

    hdr.type = HELLO_PING_TYPE;
	
    hdr.type    = Set_endian(hdr.type);
    hdr.sender_id = My_Address;
    hdr.data_len  = sizeof(hello_packet);
    hdr.ack_len   = 0;
    hdr.seq_no = 0xffff;

    pkt.seq_no       = 2;
    pkt.my_time_sec  = (int32)now.sec;
    pkt.my_time_usec = (int32)now.usec;
    pkt.diff_time    = 0;

    if(network_flag == 1) {
#ifdef SPINES_SSL
	if (!Security) {
#endif
		ret = DL_send(Local_Recv_Channels[CONTROL_LINK], 
			  address,
                          Port+CONTROL_LINK, 
			  &scat );
#ifdef SPINES_SSL
	} else {
	    ret = DL_send_SSL(Local_Recv_Channels[CONTROL_LINK], 
			      CONTROL_LINK,
			      address,
			      Port + CONTROL_LINK, 
			      &scat );
	}
#endif
    }
}



/***********************************************************/
/* Process_hello_packet(char *buf, int remaining_bytes,    */
/*                      int32 sender, int32u type)         */
/*                                                         */
/* Processes a hello message.                              */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* buf: a pointer to the message itself                    */
/* remaining bytes: length of the message in the buffer    */
/* sender: IP address of the sender                        */
/* type: first byt in the actual packet received, giving   */
/*       the type of the message, endianess, etc.          */
/*                                                         */
/***********************************************************/

void Process_hello_packet(char *buf, int remaining_bytes, int32 sender, int32u type)
{
    sp_time now, my_diff, remote, rtt, rand_hello_timeout;
    hello_packet *pkt;
    stdit it;
    Node *nd;
    Edge *edge;
    int16 linkid, udp_linkid, rel_udp_linkid, rt_udp_linkid; 
    int16 nodeid;
    int32u remote_seq, tmp;
    int32 rtt_int;
    Control_Data *c_data;
    int32 sender_id = sender;
    int time_rnd;
    float loss_rate;

    now = E_get_time();
    srand(now.usec);
    time_rnd = (int)(30.0*rand()/(RAND_MAX+1.0));

    /* If creating a new link, try to avoid synchronized hello */
    rand_hello_timeout.sec = (int)(rand()%(hello_timeout.sec+1));
    rand_hello_timeout.usec = (int)(rand()%(1000000));

    if(remaining_bytes != sizeof(hello_packet)) {
        Alarm(EXIT, "Process_hello_packet: Wrong no. of bytes for hello: %d\n",
	      remaining_bytes);
    }

    pkt = (hello_packet*) buf;
    if(!Same_endian(type)) {
	Flip_hello_pkt(pkt);
    }


    /* Compute the round trip time */

    remote.sec  = pkt->my_time_sec;
    remote.usec = pkt->my_time_usec;
    remote_seq  = pkt->seq_no;
    
    my_diff = E_sub_time(now, remote);
    rtt_int = my_diff.sec*1000000 + my_diff.usec + pkt->diff_time*100;
    if(rtt_int < 0) {
	/*This can be because of the precisopn of 0.1ms in pkt->diff_time */
	rtt_int = 100; /* The precision value */
    }
    rtt.sec  = rtt_int/1000000;
    rtt.usec = rtt_int%1000000;
    

    Alarm(DEBUG, "REM: %d.%06d; NOW: %d.%06d; MY_DIF: %d.%06d; RTT %d.%06d\n", 
	  remote.sec, remote.usec, now.sec, now.usec, my_diff.sec, my_diff.usec, rtt.sec, rtt.usec);


    /* See who sent the message */
    stdhash_find(&All_Nodes, &it, &sender_id);
    if(!stdhash_is_end(&All_Nodes, &it)) {
      
        nd = *((Node **)stdhash_it_val(&it));

	/* Maybe the node crashed and went back up before I detected it */
	/* This stays here, and not in the following one b/c it may
	 * be possible that after it the node is no longer neighbor */

        /* TODO: Not entirely ok if there is more than one crash in less
           than the time it takes to declare a link dead, or other situations
           that occur in unstable networks. Should instead agree on a view number on
           each edge or create one-way connections with identifiers. Otherwise,
           link may send reliable packets indefinetly. Somewhat reprodusable with
           high loss in hello packets. */

	if(nd->flags & NEIGHBOR_NODE) {
	    if(nd->link[CONTROL_LINK] == NULL)
	        Alarm(EXIT, "Process_hello_packet(): No control link !\n");

	    c_data = (Control_Data*)nd->link[CONTROL_LINK]->prot_data;

            /* See if my neighbor crashed and came back without me knowing it */
            if(pkt->seq_no < DEAD_LINK_CNT && c_data->other_side_hello_seq > (pkt->seq_no + DEAD_LINK_CNT)) { 
                Alarm(DEBUG, "\n\nMy neighbor crashed and I didn't know: %d :: %d; %d.%d.%d.%d\n\n",
                      pkt->seq_no, c_data->other_side_hello_seq,
                      IP1(sender_id), IP2(sender_id), IP3(sender_id), IP4(sender_id));
                Disconnect_Node(sender_id);
	        return;
            }

            /* A new neighbor?  Make sure he has a new link with me */
            if((nd->flags & NOT_YET_CONNECTED_NODE) && pkt->seq_no > DEAD_LINK_CNT) {
                /* This is from another movie ...  He should notice from my 
                   hello packets seq number that this is a new connection */
                Alarm(DEBUG, "\n\nI crashed and my neighbor does not know yet: %d :: %d; %d.%d.%d.%d\n\n",
                      pkt->seq_no, c_data->other_side_hello_seq,
                      IP1(sender_id), IP2(sender_id), IP3(sender_id), IP4(sender_id));
                return;
            }

            /* Make sure is not a very old hello that took too long to get here */
            if(c_data->other_side_hello_seq > pkt->seq_no + 3) { 
		return;
            }
	} 

        /* Stability Flag: Prevent node from comming back until enough time has
           passed since last instantiation. Prevents reliable-link situation 
           described above. */
        if (stable_delay_flag && !(nd->flags & CONNECTED_NODE)) {
            if ((now.sec - nd->last_time_neighbor.sec) < stable_timeout) {
                if (nd->flags & NEIGHBOR_NODE) {
                    Disconnect_Node(sender_id);
                }
                return;
            }
        }


	/* If this is a neighbor... */
	if(nd->flags & NEIGHBOR_NODE) {
	    if(nd->link[CONTROL_LINK] == NULL)
	        Alarm(EXIT, "Process_hello_packet(): No control link !\n");

	    c_data = (Control_Data*)nd->link[CONTROL_LINK]->prot_data;
	    c_data->diff_time = my_diff.sec*10000 + my_diff.usec/100;
	    c_data->other_side_hello_seq = remote_seq;

	    edge = (Edge*)Find_State(&All_Edges, My_Address, sender_id);
	    if(edge == NULL)
	        Alarm(EXIT, "Process_hello_packet(): No overlay edge !\n");




	    /* Update round trip time */
	    if((c_data->hello_seq <= pkt->response_seq_no + 2)) {
		tmp = rtt.sec*1000 + rtt.usec/1000;
		if((tmp > (int32u)(c_data->rtt + 20))&&(c_data->rtt > 1)) {
		    tmp = c_data->rtt + 20; /* RTT cannot grow by more than 20 milliseconds per second */
		}
		c_data->rtt = 0.99*c_data->rtt + 0.01*(((float)tmp)+0.5);
		if(c_data->rtt < 1)
		    c_data->rtt = 1;
		Alarm(DEBUG, "@\trtt\t%d\t%d\t%d\n", now.sec, tmp, (int)c_data->rtt);
	    
	    
		if(now.sec > edge->my_timestamp_sec + 30 + time_rnd) {
		    if((Route_Weight == LATENCY_ROUTE)||(Route_Weight == AVERAGE_ROUTE)) {
			if((c_data->reported_rtt == UNKNOWN)||
			   ((abs(c_data->reported_rtt - c_data->rtt) > 0.15*c_data->reported_rtt)&&
			    (abs(c_data->reported_rtt - c_data->rtt) >= 2))) { /* 1 ms accuracy one-way*/
			    if(Edge_Update_Cost(nd->link[CONTROL_LINK]->link_id, Route_Weight) > 0) {
				c_data->reported_rtt = c_data->rtt;		    
			    }
			}
		    }
		}
	    }
	    else {
		Alarm(DEBUG, "My Seq_no: %d; Response Seq_no: %d\n", 
		      c_data->hello_seq, pkt->response_seq_no);
	    }

	    if(nd->link[CONTROL_LINK]->r_data == NULL)
		Alarm(EXIT, "Process_hello_packet: control link not reliable\n");
	    nd->link[CONTROL_LINK]->r_data->rtt = c_data->rtt;

	    /* Update the last time heard */
	    nd->counter = 0;

	    E_dequeue(Send_Hello_Request_Cnt, nd->link[CONTROL_LINK]->link_id, NULL); 
		
	    /* If the node is not connected yet */
	    if(!(nd->flags & CONNECTED_NODE)) {
		if(nd->flags & NOT_YET_CONNECTED_NODE)
		    nd->flags = nd->flags ^ NOT_YET_CONNECTED_NODE;
	        nd->flags = nd->flags | CONNECTED_NODE;
	        nd->last_time_neighbor = E_get_time();

		edge->flags = CONNECTED_EDGE;
		nd->link[CONTROL_LINK]->r_data->flags = CONNECTED_LINK;

		udp_linkid = Create_Link(sender_id, UDP_LINK);
		rt_udp_linkid = Create_Link(sender_id, REALTIME_UDP_LINK);
		rel_udp_linkid = Create_Link(sender_id, RELIABLE_UDP_LINK);
		if(Links[rel_udp_linkid] == NULL)
		    Alarm(EXIT, "Process_hello_packet: could not create rel_udp\n");
		Links[rel_udp_linkid]->r_data->flags = CONNECTED_LINK;


		E_queue(Send_Hello, nd->link[CONTROL_LINK]->link_id, NULL, zero_timeout);
		E_queue(Try_to_Send, nd->link[CONTROL_LINK]->link_id, NULL, zero_timeout);
		E_queue(Try_to_Send, nd->link[RELIABLE_UDP_LINK]->link_id, NULL, zero_timeout);

		Alarm(PRINT, "Connecting node %d.%d.%d.%d\n", 
		      IP1(nd->address),
		      IP2(nd->address),
		      IP3(nd->address),
		      IP4(nd->address));
				
		Alarm(DEBUG, "!!! Sending all links\n");
	        E_queue(Net_Send_State_All, nd->link[CONTROL_LINK]->link_id, 
			&Edge_Prot_Def, short_timeout);

		Alarm(DEBUG, "!!! Sending all groups\n");
	        E_queue(Net_Send_State_All, nd->link[CONTROL_LINK]->link_id, 
			&Groups_Prot_Def, short_timeout);
	    
	    }
	    else if(Is_hello_req(type)) {
		E_queue(Send_Hello, nd->link[CONTROL_LINK]->link_id, NULL, zero_timeout);
	    }
	    else {
		/* This is a connected node */
		if(pkt->loss_rate != UNKNOWN) {
		    loss_rate = (float)pkt->loss_rate;
		    loss_rate /= LOSS_RATE_SCALE;
		     
		    /*c_data->est_loss_rate = (float)(0.9*c_data->est_loss_rate + 0.1*loss_rate);*/
		    c_data->est_loss_rate = loss_rate;
		    
		    Alarm(DEBUG, "@\tloss\t%d\t%d\t%5.3f\n", 
			  nd->link[CONTROL_LINK]->link_id, now.sec, c_data->est_loss_rate);

		    if(now.sec > edge->my_timestamp_sec + 30 + time_rnd) {		    
			if((Route_Weight == LOSSRATE_ROUTE)||(Route_Weight == AVERAGE_ROUTE)) {
			    if(c_data->reported_loss_rate == UNKNOWN) {
				if(Edge_Update_Cost(nd->link[CONTROL_LINK]->link_id, Route_Weight) > 0) {
				    c_data->reported_loss_rate = c_data->est_loss_rate;			
				}
			    }
			    else {
				if(c_data->reported_loss_rate > c_data->est_loss_rate) {
				    if(c_data->reported_loss_rate - c_data->est_loss_rate >
				       0.15*c_data->reported_loss_rate) {
					//BUG? 
                    c_data->reported_loss_rate = c_data->est_loss_rate;
					if( Edge_Update_Cost(nd->link[CONTROL_LINK]->link_id, Route_Weight) > 0) {
					    c_data->reported_loss_rate = c_data->est_loss_rate;	    
					}
				    }
				}
				else if(c_data->est_loss_rate - c_data->reported_loss_rate >
					0.15*c_data->reported_loss_rate) {
				    if(Edge_Update_Cost(nd->link[CONTROL_LINK]->link_id, Route_Weight) > 0) {
					c_data->reported_loss_rate = c_data->est_loss_rate;
				    }
				}
			    }
			}
		    }
		    Alarm(DEBUG, "Loss rate to %d.%d.%d.%d: %5.3f(pct); rtt: %5.3f\n", 
			  IP1(nd->address), IP2(nd->address),
			  IP3(nd->address), IP4(nd->address), 
			  c_data->est_loss_rate*100, c_data->rtt);
		}
	    }
	}
	else {
	    nd->flags = NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE;	      	  
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
	    
	    linkid = Create_Link(sender_id, CONTROL_LINK);
	    c_data = (Control_Data*)Links[linkid]->prot_data;
	    c_data->diff_time = my_diff.sec*10000 + my_diff.usec/100;
	    
            E_queue(Send_Hello, linkid, NULL, rand_hello_timeout);
	}
    }
    else {
        Create_Node(sender_id, NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE);

	stdhash_find(&All_Nodes, &it, &sender_id);
	if(stdhash_is_end(&All_Nodes, &it)) 
	    Alarm(EXIT, "Process_hello_packet; could not create node\n");
        nd = *((Node **)stdhash_it_val(&it));
	nd->counter = 0;

	linkid = Create_Link(sender_id, CONTROL_LINK);
	c_data = (Control_Data*)Links[linkid]->prot_data;
	c_data->diff_time = my_diff.sec*10000 + my_diff.usec/100;
	
        E_queue(Send_Hello, linkid, NULL, rand_hello_timeout);
    }
    
}



/***********************************************************/
/* Process_hello_ping_packet(int32 sender, int mode)       */
/*                                                         */
/* Processes a hello_ping message.                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sender: IP address of the sender                        */
/* mode: the link mode (of the edge) the packet arrived    */
/*       as for now, mode is not used                      */
/*                                                         */
/***********************************************************/

void Process_hello_ping_packet(int32 sender, int mode)
{
    stdit it;
    int16 linkid, nodeid;
    Node *nd;
    sp_time now, rand_hello_timeout;

    stdhash_find(&All_Nodes, &it, &sender);
    now = E_get_time();

    if(stdhash_is_end(&All_Nodes, &it)) {
        Create_Node(sender, REMOTE_NODE);
	stdhash_find(&All_Nodes, &it, &sender);
    }
      
    nd = *((Node **)stdhash_it_val(&it));


    /* If this is a neighbor... */
    if(nd->flags & NEIGHBOR_NODE) {
      /* Hey, I know about this guy ! */
#ifdef SPINES_WIRELESS
      if (Wireless_monitor) {
	/* If wireless RSSI fell too much, disconnect.  Will
	 * reconnect only if RSSI comes up to threshold. */
	if (Wireless_ts >= 20 && nd->w_data.rssi < Wireless_ts*0.4) {
	  Disconnect_Node(sender);
	}
      }
#endif
      return;
    }
    else {
	if(nd->link[CONTROL_LINK] != NULL) {
	    Alarm(EXIT, "Got a non neighbor with a control link !\n");
	}

        if (stable_delay_flag) {
            if ((now.sec - nd->last_time_neighbor.sec) < (stable_timeout*2)) {
                return;
            } 
        }

#ifdef SPINES_WIRELESS
        if (Wireless_monitor) {
            /* Create neighbor in wireless networks only when 
               signal strenght is above threshold */
            if (nd->w_data.rssi < Wireless_ts) {
                /* Not good enough */
                return;
            }
        }
#endif

        /* Create neighbor */

	nd->flags = NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE;	      	  
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
	
        /* If creating a new link, try to avoid synchronized hello */
        srand(now.usec);
        rand_hello_timeout.sec = (int)(rand()%(hello_timeout.sec+1));
        rand_hello_timeout.usec = (int)(rand()%(1000000));
        E_queue(Send_Hello, linkid, NULL, rand_hello_timeout);
    }
}


