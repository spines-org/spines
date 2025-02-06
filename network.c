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


#ifndef ARCH_PC_WIN95

#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>

#else

#include <winsock.h>

#endif


#include <stdlib.h>
#include <string.h>


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
#include "session.h"
#include "multicast.h"


/* Global variables */

extern int16 Port;
extern int16 Num_Initial_Nodes;
extern int32 Address[256];
extern int32 My_Address;
extern channel Local_Recv_Channels[MAX_LINKS_4_EDGE];
extern channel Local_Send_Channels[MAX_LINKS_4_EDGE];
extern sys_scatter Recv_Pack[MAX_LINKS_4_EDGE];
extern sp_time Up_Down_Interval;
extern int network_flag;
extern Prot_Def Edge_Prot_Def;
extern Prot_Def Groups_Prot_Def;
extern stdhash  Monitor_Loss;


/* Statistics */
extern long long int total_received_bytes;
extern long long int total_received_pkts;
extern long long int total_udp_pkts;
extern long long int total_udp_bytes;
extern long long int total_rel_udp_pkts;
extern long long int total_rel_udp_bytes;
extern long long int total_link_ack_pkts;
extern long long int total_link_ack_bytes;
extern long long int total_hello_pkts;
extern long long int total_hello_bytes;
extern long long int total_link_state_pkts;
extern long long int total_link_state_bytes;
extern long long int total_group_state_pkts;
extern long long int total_group_state_bytes;


/***********************************************************/
/* void Init_Network(void)                                 */
/*                                                         */
/* First thing that gets called. Initializes the network   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Init_Network(void) 
{
    network_flag = 1;
    total_received_bytes = 0;
    total_received_pkts = 0;
    total_udp_pkts = 0;
    total_udp_bytes = 0;
    total_rel_udp_pkts = 0;
    total_rel_udp_bytes = 0;
    total_link_ack_pkts = 0;
    total_link_ack_bytes = 0;
    total_hello_pkts = 0;
    total_hello_bytes = 0;
    total_link_state_pkts = 0;
    total_link_state_bytes = 0;
    total_group_state_pkts = 0;
    total_group_state_bytes = 0;

    Init_My_Node();
    Init_Recv_Channel(CONTROL_LINK);
    Init_Recv_Channel(UDP_LINK);
    Init_Recv_Channel(RELIABLE_UDP_LINK);

    Init_Send_Channel(CONTROL_LINK);
    Init_Send_Channel(UDP_LINK);
    Init_Send_Channel(RELIABLE_UDP_LINK);

    Init_Nodes();
    Init_Connections();
    Init_Session();
    Resend_States(0, &Edge_Prot_Def);
    State_Garbage_Collect(0, &Edge_Prot_Def);
    Resend_States(0, &Groups_Prot_Def);
    State_Garbage_Collect(0, &Groups_Prot_Def);
    /* Uncomment next line to print periodical route updates */
    /* Print_Edges(0, NULL); */
}


/***********************************************************/
/* void Init_Recv_Channel(int16 mode)                      */
/*                                                         */
/* Initializes a receiving socket for a link type          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* mode: type of the link                                  */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Init_Recv_Channel(int16 mode) 
{
    if(mode == CONTROL_LINK) {
        Local_Recv_Channels[mode] = DL_init_channel( RECV_CHANNEL, (int16)(Port+mode), 0, 0 );
	E_attach_fd(Local_Recv_Channels[mode], READ_FD, Net_Recv, mode, 
		    NULL, HIGH_PRIORITY );
    }
    if((mode == UDP_LINK)||(mode == RELIABLE_UDP_LINK)) {
        Local_Recv_Channels[mode] = DL_init_channel( RECV_CHANNEL, (int16)(Port+mode), 0, 0 );
	E_attach_fd(Local_Recv_Channels[mode], READ_FD, Net_Recv, mode, 
		    NULL, MEDIUM_PRIORITY );
    }
    Recv_Pack[mode].num_elements = 2;
    Recv_Pack[mode].elements[0].len = sizeof(packet_header);
    Recv_Pack[mode].elements[0].buf = (char *) new(PACK_HEAD_OBJ);
    Recv_Pack[mode].elements[1].len = sizeof(packet_body);

    /* The packet body should have reference count */
    Recv_Pack[mode].elements[1].buf = (char *) new_ref_cnt(PACK_BODY_OBJ);
}


                                                         
/***********************************************************/
/* void Init_Send_Channel(int16 mode)                      */
/*                                                         */
/* Initializes a sending socket for a link type            */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* mode: type of the link                                  */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Init_Send_Channel(int16 mode) 
{
    Local_Send_Channels[mode] = DL_init_channel(SEND_CHANNEL, (int16)(Port+mode), 0, 0);
}


/***********************************************************/
/* void Net_Recv(channel sk, int mode, void * dummy_p)     */
/*                                                         */
/* Called by the event system to receive data from socket  */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      socket                                         */
/* mode:    type of the link                               */
/* dummy_p: not used                                       */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void    Net_Recv(channel sk, int mode, void * dummy_p) 
{
    Read_UDP(sk, mode, &Recv_Pack[mode]);
}

/***********************************************************/
/* void Init_My_Node(void)                                 */
/*                                                         */
/* Initializes the IP address of the daemon                */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/


void Init_My_Node(void) 
{
    struct hostent  *host_ptr;
    char machine_name[256];
    int i;

    if(My_Address == -1) { /* No local address was given in the command line */
	gethostname(machine_name,sizeof(machine_name)); 
	host_ptr = gethostbyname(machine_name);
	
	if(host_ptr == NULL)
	    Alarm( EXIT, "Init_My_Node: could not get my ip address (my name is %s)\n",
		   machine_name );
	if (host_ptr->h_addrtype != AF_INET)
	    Alarm(EXIT, "Init_My_Node: Sorry, cannot handle addr types other than IPv4\n");
	if (host_ptr->h_length != 4)
	    Alarm(EXIT, "Conf_init: Bad IPv4 address length\n");
	
        memcpy(&My_Address, host_ptr->h_addr, sizeof(struct in_addr));
	My_Address = ntohl(My_Address);
    }
    for(i=0;i<Num_Initial_Nodes;i++) {
        if(Address[i] == My_Address)
	    Alarm(EXIT, "Oops ! I cannot conect to myself...\n");
    }
  
}


/***********************************************************/
/* void Read_UDP(channel sk, int mode, sys_scatter *scat)  */
/*                                                         */
/* Receives data from a socket                             */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      socket                                         */
/* mode:    type of the link                               */
/* scat:    scatter to receive data into                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

int    Read_UDP(channel sk, int mode, sys_scatter *scat)
{
    int	received_bytes;
    packet_header *pack_hdr;
    sp_time dummy_time;
    int32 sender;
    stdhash_it it;
    double chance = 100.0;
    double test = 0.0;
    double p1, p01, p11;
    Loss *loss = NULL;
    
    received_bytes = DL_recv(sk, scat);  
    if( received_bytes < sizeof( packet_header )) {
        Alarm(DEBUG, "Read_UDP: ignoring packet of size %d, smaller than packet header size %d\n", 
	      received_bytes, sizeof(packet_header));
	return -1;
    }

    
    Alarm(DEBUG, "Read_UDP: got %d bytes\n", received_bytes); 
    
    pack_hdr = (packet_header *)scat->elements[0].buf;

    /* Check for monitor injected losses */
    sender = pack_hdr->sender_id;
    stdhash_find(&Monitor_Loss, &it, &sender);
    if (!stdhash_it_is_end(&it)) {

	loss = (Loss*)stdhash_it_val(&it); 
	dummy_time = E_get_time();
	srand(dummy_time.usec);

	chance = rand();
	chance *= 100.0;
	chance /= RAND_MAX;

	p1  = loss->loss_rate;
	p1  = p1/1000000.0;

	if(loss->burst_rate == 0) {
	    p01 = p11 = p1;
	}
	else if((loss->burst_rate == 1000000)|| /* burst rate of 100% or */
		(loss->loss_rate == 1000000)) { /* loss rate of 100% */
	    p01 = p11 = 1.0;
	}
	else {
	    p11 = loss->burst_rate;
	    p11 = p11/1000000.0;
	    p01 = p1*(1-p11)/(1-p1);	      
	}

        if(loss->was_loss > 0) {
	    test = 100.0 * p11;
	}
	else {
	    test = 100.0 * p01;
	}
    } 

    if(loss != NULL) {
	Alarm(DEBUG, "loss: %d; burst: %d; was_loss %d; test: %5.3f; chance: %5.3f\n",
	      loss->loss_rate, loss->burst_rate, loss->was_loss, test, chance);
    }



    if((network_flag == 1)&&(chance >= test)) {
	Prot_process_scat(scat, received_bytes, mode);

	total_received_bytes += received_bytes;
	total_received_pkts++;
	if(loss != NULL) {
	    loss->was_loss = 0;
	}
    }
    else {
	received_bytes = 0;
	if(loss != NULL) {
	    loss->was_loss = 1;
	}
    }
    
    return received_bytes;
}


void Up_Down_Net(int dummy_int, void *dummy_p)
{
    network_flag = 1 - network_flag;
    E_queue(Up_Down_Net, 0, NULL, Up_Down_Interval);
}

void Graceful_Exit(int dummy_int, void *dummy_p)
{
    Alarm(PRINT, "\n\n\nUDP\t%9lld\t%9lld\n", total_udp_pkts, total_udp_bytes);
    Alarm(PRINT, "REL_UDP\t%9lld\t%9lld\n", total_rel_udp_pkts, total_rel_udp_bytes);
    Alarm(PRINT, "ACK\t%9lld\t%9lld\n", total_link_ack_pkts, total_link_ack_bytes);
    Alarm(PRINT, "HELLO\t%9lld\t%9lld\n", total_hello_pkts, total_hello_bytes);
    Alarm(PRINT, "LINK_ST\t%9lld\t%9lld\n", total_link_state_pkts, total_link_state_bytes);
    Alarm(PRINT, "GRP_ST\t%9lld\t%9lld\n", total_group_state_pkts, total_group_state_bytes);
    Alarm(EXIT,  "TOTAL\t%9lld\t%9lld\n", total_received_pkts, total_received_bytes);
}

