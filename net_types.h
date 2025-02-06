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


#ifndef	NET_TYPES
#define	NET_TYPES


/*      Dont forget that 0x80000080 is kept for endians */

/* First byte */

#define         RELIABLE_TYPE           0x40000000
/* Unreliable msgs will not have 1 on the second bit */   

#define		HELLO_TYPE		0x10000000
#define		HELLO_REQ_TYPE		0x20000000
#define         HELLO_DISCNCT_TYPE      0x20000000 /* Same value as req */
#define		HELLO_PING_TYPE		0x30000000
#define         HELLO_CLOSE_TYPE        0x30000000 /* Same value as ping */
#define         HELLO_MASK              0x30000000

#define		LINK_STATE_TYPE		0x01000000
#define		GROUP_STATE_TYPE	0x02000000
#define         ROUTE_MASK              0x0f000000


/* Second byte */
/* Nothing here yet... */


/* Third byte */
#define         ECN_DATA_T1             0x00000100
#define         ECN_DATA_T2             0x00000200
#define         ECN_DATA_T3             0x00000300
#define         ECN_DATA_MASK           0x00000300

#define         ECN_ACK_T1              0x00000400
#define         ECN_ACK_T2              0x00000800
#define         ECN_ACK_T3              0x00000c00
#define         ECN_ACK_MASK            0x00000c00

#define         ACK_INTERVAL_MASK       0x0000f000 


/* Fourth byte */

#define         LINK_ACK_TYPE           0x00000001
#define         UDP_DATA_TYPE           0x00000002
#define         REL_UDP_DATA_TYPE       0x00000003
#define         DATA_MASK               0x0000007f


/* Other types */
#define         REL_MCAST_ACK_TYPE      0x00000001



/* Type macros */
#define		Is_reliable(type)	(type & RELIABLE_TYPE)
#define		Is_hello(type)	        ((type & HELLO_MASK)==HELLO_TYPE)
#define		Is_hello_req(type)	((type & HELLO_MASK)==HELLO_REQ_TYPE)
#define		Is_hello_ping(type)	((type & HELLO_MASK)==HELLO_PING_TYPE)
#define		Is_hello_discnct(type)	((type & HELLO_MASK)==HELLO_DISCNCT_TYPE)
#define		Is_hello_close(type)	((type & HELLO_MASK)==HELLO_CLOSE_TYPE)

#define		Is_link_state(type)	((type & ROUTE_MASK) == LINK_STATE_TYPE)
#define		Is_group_state(type)	((type & ROUTE_MASK) == GROUP_STATE_TYPE)

#define		Is_udp_data(type)	((type & DATA_MASK) == UDP_DATA_TYPE)
#define		Is_rel_udp_data(type)	((type & DATA_MASK) == REL_UDP_DATA_TYPE)
#define		Is_link_ack(type)	((type & DATA_MASK) == LINK_ACK_TYPE)



/*This goes in front of each packet (any kind), as it is sent on the network */


typedef	struct	dummy_packet_header {
    int32u          type;      /* type of the message */
    int32           sender_id; /* Sender of this network packet, and NOT
			          the originator of the message */
    int16u          data_len;  /* Length of the data */
    int16u          ack_len;   /* Length of the acknowledgement tail */
    int16u          seq_no;    /* Sequence number of the packet for link loss_rate */
    int16u          dummy;     /* Nothing yet... */
} packet_header;

typedef	char       packet_body[MAX_PACKET_SIZE-sizeof(packet_header)];

typedef	struct	dummy_udp_pkt_header {
    int32           source;
    int32           dest;
    int16u          source_port;
    int16u          dest_port;
    int16u          len;
    int16u          seq_no;    /* sequence number for end-to-end loss_rate */
} udp_header;

typedef struct dummy_rel_udp_pkt_add {
    int32u type;
    int16u data_len;
    int16u ack_len;
} rel_udp_pkt_add;

typedef	struct	dummy_ses_hello_packet {
    int32u          type;
    int32u          seq_no;
    int32           my_sess_id;
    int16u          my_port;
    int16u          orig_port;
} ses_hello_packet;

typedef	struct	dummy_hello_packet {
    int32u          seq_no;
    int32           my_time_sec;
    int32           my_time_usec;
    int32           diff_time;
    int32           loss_rate;   /* estimated loss rate of data */
                                 /* (from 0 to LOSS_RATE_SCALE for 0% to 100%) */
} hello_packet;


typedef	struct	dummy_link_state_packet {
    int32 	    source;
    int16u	    num_edges;
    int16           src_data; /* Data about the source itself. 
				 Not used yet */
} link_state_packet;


typedef	struct	dummy_edge_cell_packet {
    int32           dest;
    int32           timestamp_sec;
    int32           timestamp_usec;
    int16           cost;
    int16 	    age;
} edge_cell_packet;

typedef	struct	dummy_group_state_packet {
    int32 	    source;
    int16u	    num_cells;
    int16           src_data; /* Data about the source itself. 
				 Not used yet */
} group_state_packet;


typedef	struct	dummy_group_cell_packet {
    int32           dest; /* This is actually the multicast address */
    int32           timestamp_sec;
    int32           timestamp_usec;
    int16           flags;
    int16 	    age;
} group_cell_packet;


typedef struct dummy_reliable_tail {
    int32u          seq_no;            /* seq no of this reliable message */ 
    int32u          cummulative_ack;   /* cummulative in order ack */
} reliable_tail;


typedef struct dummy_reliable_mcast_ack {
    int32           type;
    int32           mcast_address;
    int32           seq_no;
    int32           timestamp_sec;
    int32           timestamp_usec;
} reliable_mcast_ack;


typedef struct dummy_rel_mcast_tail {
    int32u          seq_no;            /* seq no of this reliable message */
    int32           acker;             /* who will ack this message*/
} rel_mcast_tail;


#endif	/* NET_TYPES */
