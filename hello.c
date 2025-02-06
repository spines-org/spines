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

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "link.h"
#include "network.h"
#include "reliable_link.h"
#include "hello.h"
#include "link_state.h"
#include "protocol.h"
#include "stdutil/src/stdutil/stdhash.h"


/* Global variables */

extern int32     My_Address;
extern int16     Port;
extern int16     Num_Neighbors;
extern stdhash   All_Nodes;
extern stdhash   All_Edges;
extern Node*     Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
extern Link*     Links[MAX_LINKS];
extern int       network_flag;
extern channel   Local_Send_Channels[MAX_LINKS_4_EDGE];

/* Local variables */

static const sp_time zero_timeout  = {     0,    0};
static const sp_time short_timeout = {     0,    50000}; /* 50 milliseconds */
static       sp_time hello_timeout;



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

    hello_timeout.sec = 1;
    hello_timeout.usec = 0;

    /* Start the hello protocol on all the known links */
    for(linkid=0; linkid<MAX_LINKS && Links[linkid] != NULL; linkid++) 
        Send_Hello(linkid, NULL);

    /* Start the link recover protocol (hello ping) */
    Send_Hello_Ping(0, NULL);
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
	Alarm(PRINT, "Send Hello on a non existing link !\n");
	return;
    }
    cnt = Links[linkid]->other_side_node->counter++;
    /*
    Alarm(PRINT, "hello counter: %d\n", Links[linkid]->other_side_node->counter);
    */
    if(cnt > DEAD_LINK_CNT) {
        Alarm(PRINT, "Disconnecting node %d.%d.%d.%d\n", 
	      IP1(Links[linkid]->other_side_node->address),
	      IP2(Links[linkid]->other_side_node->address),
	      IP3(Links[linkid]->other_side_node->address),
	      IP4(Links[linkid]->other_side_node->address));

        Disconnect_Node(Links[linkid]->other_side_node->address);
    }
    else {
        Net_Send_Hello((int16u)linkid, 0);
	E_queue(Send_Hello, linkid, NULL, hello_timeout);
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
       Net_Send_Hello((int16u)linkid, 1);       
}


/***********************************************************/
/* Send_Hello_Ping(int linkid, void *dummy)                */
/*                                                         */
/* Called periodically by the event system.                */
/* Sends a hello message on a dead link and requests an    */
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
    stdhash_it it;
    Edge *edge;

    stdhash_find(&All_Edges, &it, &My_Address);
    while(!stdhash_it_is_end(&it)) {
      	edge = *((Edge **)stdhash_it_val(&it));
	if(edge->cost < 0)
	    Net_Send_Hello_Ping(edge->dest->address);       
	stdhash_it_keyed_next(&it);
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
	
    hdr.type    = Set_endian(hdr.type);

    hdr.sender_id = My_Address;
    hdr.data_len  = sizeof(hello_packet);
    hdr.ack_len   = 0;

    if(Links[linkid]->prot_data == NULL)
	Alarm(EXIT, "Net_Send_Hello: control_data unavailable\n");
	
    c_data = Links[linkid]->prot_data;
    pkt.seq_no       = c_data->hello_seq++;
    pkt.my_time_sec  = (int32)now.sec;
    pkt.my_time_usec = (int32)now.usec;
    pkt.diff_time    = c_data->diff_time;

    if(network_flag == 1) {
	ret = DL_send(Links[linkid]->chan, 
		      Links[linkid]->other_side_node->address,
		      Links[linkid]->port, 
		      &scat );
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

    pkt.seq_no       = 0;
    pkt.my_time_sec  = (int32)now.sec;
    pkt.my_time_usec = (int32)now.usec;
    pkt.diff_time    = 0;

    if(network_flag == 1) {
	ret = DL_send(Local_Send_Channels[CONTROL_LINK], 
		      address,
		      Port, 
		      &scat );
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
    sp_time now, diff, my_diff, remote, rtt;
    hello_packet *pkt;
    stdhash_it it;
    Node *nd;
    Edge *edge;
    int16 linkid, udp_linkid, rel_udp_linkid; 
    int16 nodeid;
    int32u remote_seq;
    Control_Data *c_data;
    int32 sender_id = sender;

    now = E_get_time();

    if(remaining_bytes != sizeof(hello_packet)) {
        Alarm(EXIT, "Process_hello_packet: Wrong no. of bytes for hello: %d\n",
	      remaining_bytes);
    }

    pkt = (hello_packet*) buf;

    if(!Same_endian(type)) { 
        pkt->seq_no       = Flip_int32(pkt->seq_no);	
        pkt->my_time_sec  = Flip_int32(pkt->my_time_sec);	
        pkt->my_time_usec = Flip_int32(pkt->my_time_usec);	
        pkt->diff_time    = Flip_int32(pkt->diff_time);
    }


    /* Compute the round trip time */

    remote.sec  = pkt->my_time_sec;
    remote.usec = pkt->my_time_usec;
    diff.sec    = pkt->diff_time/1000000;
    diff.usec   = pkt->diff_time%1000000;
    remote_seq  = pkt->seq_no;
    
    my_diff = E_sub_time(now, remote);
    rtt     = E_add_time(my_diff, diff);


    /* See who sent the message */
    stdhash_find(&All_Nodes, &it, &sender_id);
    if(!stdhash_it_is_end(&it)) {
      
        nd = *((Node **)stdhash_it_val(&it));

	/* Maybe the node crashed and went back up before I detected it */
	/* This stays here, and not in the following one b/c it may
	 * be possible that after it the node is no longer neighbor*/
	if(nd->flags & NEIGHBOR_NODE) {
	    if(nd->link[CONTROL_LINK] == NULL)
	        Alarm(EXIT, "Process_hello_packet(): No control link !\n");

	    c_data = (Control_Data*)nd->link[CONTROL_LINK]->prot_data;

	    if(pkt->seq_no < c_data->other_side_hello_seq) {
	        Alarm(PRINT, "\n\nMy neighbor crashed and I didn't know...\n\n");
	        Disconnect_Node(sender_id);
	    }
	}

	/* If this is a neighbor... */
	if(nd->flags & NEIGHBOR_NODE) {
	    if(nd->link[CONTROL_LINK] == NULL)
	        Alarm(EXIT, "Process_hello_packet(): No control link !\n");

	    c_data = (Control_Data*)nd->link[CONTROL_LINK]->prot_data;
	    c_data->diff_time = my_diff.sec*1000000 + my_diff.usec;
	    c_data->other_side_hello_seq = remote_seq;

	    edge = Find_Edge(&All_Edges, My_Address, sender_id, 0);
	    if(edge == NULL)
	        Alarm(EXIT, "Process_hello_packet(): No overlay edge !\n");

	    /* Update round trip time */
	    c_data->rtt = rtt.sec*1000000 + rtt.usec;
	    if(c_data->rtt < 0)
	      c_data->rtt = 0;
	    
	    if(nd->link[CONTROL_LINK]->r_data == NULL)
		Alarm(EXIT, "Process_hello_packet: control link not reliable\n");
	    nd->link[CONTROL_LINK]->r_data->rtt = c_data->rtt;

	    /* Update the last time heard */
	    nd->last_time_heard = now;
	    nd->counter = 0;
		
	    /* If the node is not connected yet */
	    if(!(nd->flags & CONNECTED_NODE)) {
		if(nd->flags & NOT_YET_CONNECTED_NODE)
		    nd->flags = nd->flags ^ NOT_YET_CONNECTED_NODE;
	        nd->flags = nd->flags | CONNECTED_NODE;

		edge->flags = CONNECTED_EDGE;
		nd->link[CONTROL_LINK]->r_data->flags = CONNECTED_LINK;

		udp_linkid = Create_Link(sender_id, UDP_LINK);
		rel_udp_linkid = Create_Link(sender_id, RELIABLE_UDP_LINK);
		if(Links[rel_udp_linkid] == NULL)
		    Alarm(EXIT, "Process_hello_packet: could not create rel_udp\n");
		Links[rel_udp_linkid]->r_data->flags = CONNECTED_LINK;


		E_queue(Send_Hello, nd->link[CONTROL_LINK]->link_id, NULL, zero_timeout);

		Alarm(DEBUG, "!!! Sending all links\n");
	        E_queue(Net_Send_Link_State_All, nd->link[CONTROL_LINK]->link_id, NULL, short_timeout);
	    }
	    else if(Is_hello_req(type)) {
		E_queue(Send_Hello, nd->link[CONTROL_LINK]->link_id, NULL, zero_timeout);
	    }
	}
	else {
	    nd->flags = NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE;	      	  
	    nd->last_time_heard = now;
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
	    c_data->diff_time = my_diff.sec*1000000 + my_diff.usec;
	    c_data->other_side_hello_seq = remote_seq;
	    
	    E_queue(Send_Hello, linkid, NULL, zero_timeout);
	}
    }
    else {
        Create_Node(sender_id, NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE);

	stdhash_find(&All_Nodes, &it, &sender_id);
	if(stdhash_it_is_end(&it)) 
	    Alarm(EXIT, "Process_hello_packet; could not create node\n");
        nd = *((Node **)stdhash_it_val(&it));
	nd->last_time_heard = now;
	nd->counter = 0;
	
	linkid = Create_Link(sender_id, CONTROL_LINK);
	c_data = (Control_Data*)Links[linkid]->prot_data;
	c_data->diff_time = my_diff.sec*1000000 + my_diff.usec;
	c_data->other_side_hello_seq = remote_seq;
	
	E_queue(Send_Hello, linkid, NULL, zero_timeout);
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
    stdhash_it it;
    int16 linkid, nodeid;
    Node *nd;


    stdhash_find(&All_Nodes, &it, &sender);

    if(stdhash_it_is_end(&it)) {
        Create_Node(sender, REMOTE_NODE);
	stdhash_find(&All_Nodes, &it, &sender);
    }
      
    nd = *((Node **)stdhash_it_val(&it));


    /* If this is a neighbor... */
    if(nd->flags & NEIGHBOR_NODE) {
	/* Hey, I know about this guy ! */
	return;
    }
    else {
	if(nd->link[CONTROL_LINK] != NULL) {
	    Alarm(EXIT, "Got a non neighbor with a control link !\n");
	}
	
	nd->flags = NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE;	      	  
	nd->last_time_heard = E_get_time();;
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
