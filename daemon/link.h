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

#ifndef LINK_H
#define LINK_H

/* Window (for reliability) */
#define MAX_WINDOW       500
#define MAX_CG_WINDOW    200
#define CTRL_WINDOW      10 
#define MAX_HISTORY      1000

/* Packet (unreliable) window for detecting loss rate */
#define PACK_MAX_SEQ     20000

/* Loss rate calculation constants */
#define LOSS_RATE_SCALE  1000000  /* For conversion from float to int*/
#define UNKNOWN             (-1)
#define LOSS_HISTORY        50

/* Link types */
typedef enum 
{
  CONTROL_LINK,
  UDP_LINK,
  RELIABLE_UDP_LINK,
  REALTIME_UDP_LINK,

  RESERVED0_LINK,      /* MN */
  RESERVED1_LINK,      /* TCP */
  RESERVED2_LINK,      /* SC2 */

  MAX_LINKS_4_EDGE,

} Link_Type;

#define MAX_NEIGHBORS        256
#define MAX_LOCAL_INTERFACES 5
#define MAX_NETWORK_LEGS     (MAX_NEIGHBORS * MAX_LOCAL_INTERFACES)

/* TODO: examine all instances of MAX_LINKS / MAX_LINKS_4_EDGE; replace with MAX_NEIGHBORS? */
/* TODO: redefine MAX_LINKS to be (MAX_LOCAL_LEGS * (int) MAX_LINKS_4_EDGE) */
/* TODO: actually change MAX_LINKS_4_EDGE to be MAX_LINKS_4_LEG -> all over the code */

#define MAX_LINKS            (MAX_NEIGHBORS * (int) MAX_LINKS_4_EDGE)

#define MAX_DISCOVERY_ADDR  10

/* Ports to listen to for sessions */
#define SESS_PORT           ( MAX_LINKS_4_EDGE     )
#define SESS_UDP_PORT       ( MAX_LINKS_4_EDGE + 1 )
#define SESS_CTRL_PORT      ( MAX_LINKS_4_EDGE + 2 )

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

#define EMPTY_CELL       0
#define RECVD_CELL       1
#define NACK_CELL        2
#define SENT_CELL        3
#define RETRANS_CELL     4

#define MAX_BUFF_LINK    50
#define MAX_REORDER      10

#define MAX_BUCKET       500
#define RT_RETRANSM_TOK  5   /* 1/5 = 20% max retransmissions */

#define BWTH_BUCKET      536064 /* 64K + 1.472K for one packet*/

#include "stdutil/stddefines.h"
#include "stdutil/stdit.h"
#include "stdutil/stddll.h"
#include "stdutil/stdcarr.h"

#include "net_types.h"
#include "node.h"
#include "link_state.h"
#include "network.h"

#include "session.h"

struct Node_d;
struct Edge_d;
struct Interface_d;
struct Network_Leg_d;
struct Link_d;

typedef struct Lk_Param_d {
    int32 loss_rate;
    int32 burst_rate;
    int   was_loss;
    int32 bandwidth;
    int32 bucket;
    sp_time last_time_add;
    sp_time delay;
} Lk_Param;

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
    int32u total_len;
} UDP_Cell;

typedef struct Recv_Cell_d {
    char flag;                    /* Received, empty, not received yet, etc. */
    sp_time nack_sent;            /* Last time I sent a NACK */
    struct UDP_Cell_d data;       /* For FIFO ordering, it keeps the 
				     unordered data */
} Recv_Cell;

typedef struct History_Cell_d {
    char*     buff;
    int16u    len;
    sp_time   timestamp;
} History_Cell;

typedef struct History_Recv_Cell_d {
    int flags;
    sp_time timestamp;
} History_Recv_Cell;

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
    int32u adv_win;               /* advertised window set by the receiver */
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
    int16u cong_flag;
    int32u last_tail_resent;
    int16u unacked_msgs;
    int32u last_ack_sent;
    int32u last_seq_sent;
    int  padded;
} Reliable_Data;

typedef struct Loss_Event_d {
    int32 received_packets;
    int32 lost_packets;
} Loss_Event;

typedef struct Loss_Data_d {
    int16  my_seq_no;             /* My packet sequence number */
    int16  other_side_tail;       /* Last packet received in order from the other side */
    int16  other_side_head;       /* Highest packet received from the other side */
    int32  received_packets;      /* Received packets since it's been reset */
    char   recv_flags[MAX_REORDER];/* Window of flags for received packets */
    Loss_Event loss_interval[LOSS_HISTORY]; /* History of loss events */      
    int32  loss_event_idx;        /* Index in the loss event array */
    float  loss_rate;             /* Locally estimated loss rate */
} Loss_Data;

typedef struct Control_Data_d {
    int32u hello_seq;             /* My hello sequence */
    int32u other_side_hello_seq;  /* Remote hello sequence */
    int32  diff_time;             /* Used for computing round trip time */
    float  rtt;                   /* Round trip time of the link */
    Loss_Data l_data;             /* For determining loss_rate */
    float  est_loss_rate;         /* Estimated loss rate */
    float  est_tcp_rate;          /* Estimated available TCP rate */

    int32  reported_rtt;          /* RTT last reported in a link_state (if any) */
    float  reported_loss_rate;    /* Loss rate last reported in a link_state (if any) */
} Control_Data;

typedef struct Realtime_Data_d {
    int32u    head;
    int32u    tail;
    struct History_Cell_d window[MAX_HISTORY]; /* Sending window history
						(keeps actual pakets for a while) */    
    int32u    recv_head;
    int32u    recv_tail;
    struct History_Recv_Cell_d recv_window[MAX_HISTORY]; /* Receiving window history    
							    Only flags here, no packets */
    char nack_buff[MAX_PACKET_SIZE];
    int num_nacks;
    char *retransm_buff;
    int num_retransm;
    int bucket;
} Realtime_Data;

typedef struct Link_d {

  int16     link_id;               /* Index of the link in the global link array */ 
  Link_Type link_type;             /* Type of link this is */

  struct Network_Leg_d *leg;       /* Leg across which this link is running */
  
  struct Reliable_Data_d *r_data;  /* Reliablility specific data. 
				      * If the link does not need reliability,
				      * this is NULL */
  void *prot_data;                 /* Link Protocol specific data */

} Link;

int16   Create_Link(Network_Leg *leg, int16 mode);
void    Destroy_Link(int16 linkid);

Link   *Get_Best_Link(Node_ID node_id, int mode);
int     Link_Send(Link *lk, sys_scatter *scat);

int32   Relative_Position(int32 base, int32 seq);

void    Check_Link_Loss(struct Network_Leg_d *leg, int16u seq_no);
int32   Compute_Loss_Rate(struct Network_Leg_d *leg);
int16u  Set_Loss_SeqNo(struct Network_Leg_d *leg);

#endif
