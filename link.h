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


#ifndef LINK_H
#define LINK_H

#include "stdutil/src/stdutil/stdhash.h"
#include "stdutil/src/stdutil/stdcarr.h"

#include "session.h"


#define MAX_LINKS_4_EDGE 4
#define MAX_LINKS        1024 /* this allows for 256 neighbor nodes
			                     4 links for each node */

/* Window (for reliability) */
#define MAX_WINDOW       500
#define MAX_CG_WINDOW    200
#define CTRL_WINDOW      10 

/* Packet (unreliable) window for detecting loss rate */
#define PACK_MAX_SEQ     10000

/* Types */
#define CONTROL_LINK        0
#define UDP_LINK            1
#define RELIABLE_UDP_LINK   2
#define TCP_LINK            3

/* Ports to listen to, in addition to ON port */
#define CONTROL_PORT     0
#define UDP_PORT         1
#define SESS_PORT        MAX_LINKS_4_EDGE


/* Updates */
#define OLD_CHANGE       1
#define NEW_CHANGE       2

/* Update actions */
#define NEW_ACT          1
#define UPDATE_ACT       2
#define DELETE_ACT       3

/* Flags */
#define UNAVAILABLE_LINK   0x0001
#define AVAILABLE_LINK     0x0002
#define CONNECTED_LINK     0x0004
#define CONNECT_WAIT_LINK  0x0008
#define ACCEPT_WAIT_LINK   0x0010
#define DISCONNECT_LINK    0x0020


#define CONNECTED_EDGE   0x1
#define REMOTE_EDGE      0x2
#define DEAD_EDGE        0x4

#define EMPTY_CELL       0
#define RECVD_CELL       1
#define NACK_CELL        2

#define MAX_BUFF_LINK    30


typedef struct Buffer_Cell_d {
    int32u seq_no;
    char*  buff;
    int32u pack_type;
    int16u data_len;
    sp_time timestamp;
    int resent;
} Buffer_Cell;

typedef struct UDP_Cell_d {
    char*  buff;
    int16u len;
} UDP_Cell;


typedef struct Recv_Cell_d {
    char flag;                    /* Received, empty, not received yet, etc. */
    sp_time nack_sent;            /* Last time I sent a NACK */
    struct UDP_Cell_d data;       /* For FIFO ordering, it keeps the 
				     unordered data */
} Recv_Cell;


typedef struct Reliable_Data_d {
    int16 flags;                  /* Link status */
    int16 connect_state;          /* Connect state */
    int32u seq_no;                /* Sequence number */
    stdcarr msg_buff;             /* Sending buffer in front of the link */
    float window_size;            /* Congestion window. */
    int32u max_window;            /* Maximum congestion window */
    int32u ssthresh;              /* Slow-start threshold */

    struct Buffer_Cell_d window[MAX_WINDOW]; /* Sending window 
						(keeps actual pakets) */
    int32u head;                  /* 1 + highest message sent */
    int32u tail;                  /* Lowest message that is not acked */
    struct Recv_Cell_d  recv_window[MAX_WINDOW]; /* Receiving window */
    int32u recv_head;             /* 1 + highest packet received */
    int32u recv_tail;             /* 1 + highest received packet in order 
				     (first hole)*/    
    char *nack_buff;              /* Nacks to be parsed */
    int16u nack_len;              /* Length of the above buffer */ 
    int16 scheduled_ack;          /* Set to be 1 if I have an ack scheduled, 
				   * to send, 0 otherwise*/
    int16 scheduled_timeout;      /* Set to be 1 if I have a timeout scheduled
				   * for retransmission, 0 otherwise */
    int16 timeout_multiply;       /* Subsequent timeouts increase exponentially */
    int32 rtt;                    /* Round trip time of the link. */
    int32u congestion_flag;       /* ECN flag */
    int16u ack_window;
    int16u unacked_msgs;
    int32u last_ack_sent;
    int32u last_seq_sent;
} Reliable_Data;

typedef struct Control_Data_d {
    int32u hello_seq;             /* My hello sequence */
    int32u other_side_hello_seq;  /* Remote hello sequence */
    int32  diff_time;             /* Used for computing round trip time */
    int32  rtt;                   /* Round trip time of the link */
} Control_Data;

typedef struct UDP_Data_d {
    int16  my_seq_no;             /* My packet sequence number */
    int16  other_side_seq_no;     /* Last packet received from the other side */
    int16  last_seq_noloss;       /* There were no losses since this seqno */
    int16  no_of_rounds_wo_loss;  /* PACK_MAX_SEQ reached so many
				     times without a loss */

    stdcarr udp_ses_buff;         /* Sending UDP buffer due to sessions */
    stdcarr udp_net_buff;         /* Sending UDP buffer due to network */
    char block_flag;              /* 1 if the link is blocked, 0 otherwise */
} UDP_Data;

typedef struct Link_d {
    struct Node_d *other_side_node; /* The node at the other side */
    int16 link_node_id;     /* The link indices in the source node link struct*/
    int16 link_id;          /* Index of the link in the global link array */ 
    channel chan;           /* Socket channel */
    int16u port;            /* Sending port */
    struct Reliable_Data_d *r_data;  /* Reliablility specific data. 
				      * If the link does not need reliability,
				      * this is NULL */
    void *prot_data;        /* Protocol specific data */
} Link;


typedef struct Edge_d {
    struct Node_d *source;        /* Source node */
    struct Node_d *dest;          /* Destination node */
    int32  timestamp_sec;         /* Original timestamp of the last change (seconds) */
    int32  timestamp_usec;        /* ...microseconds */
    int32  my_timestamp_sec;      /* Local timestamp of the last upadte (seconds) */
    int32  my_timestamp_usec;     /* ...microseconds */
    int32  cost;                  /* Cost of the edge */
    int16  flags;                 /* Edge status */
} Edge;


/* Represent a link update to be sent. It refers to an edge
   and the mask is set to which nodes sould not hear about 
   this update (b/c they already know it) */
typedef struct Changed_Edge_d {
    struct Edge_d *edge;
	int32u mask[MAX_LINKS/(MAX_LINKS_4_EDGE*32)]; 
} Changed_Edge;


int16 Create_Link(int32 address, int16 mode);
void Destroy_Link(int16 linkid);
Edge* Create_Overlay_Edge(int32 source, int32 dest);
Edge* Find_Edge(stdhash *hash_struct, int32 source, int32 dest, int del);
Changed_Edge* Find_Changed_Edge(int32 source, int32 dest);
Edge* Destroy_Edge(int32 source, int32 dest, int local_call);
void Add_to_changed_edges(int32 sender, struct Edge_d *edge, int how);
void Empty_Changed_Updates(void);
void Print_Links(int dummy_int, void* dummy); 

#endif
