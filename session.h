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


#ifndef SESSION_H
#define SESSION_H


#define READY_LEN           0
#define PARTIAL_LEN         1
#define READY_DATA          2
#define PARTIAL_DATA        3

#define READ_DESC           1
#define EXCEPT_DESC         2
#define WRITE_DESC          4

#define SOCK_ERR            0
#define PORT_IN_USE         1
#define SES_BUFF_FULL       2
#define SES_DISCONNECT      3
#define SES_DELAY_CLOSE     4

#define UDP_SES_TYPE        1
#define RELIABLE_SES_TYPE   2
#define LISTEN_SES_TYPE     3

#define BIND_TYPE_MSG       1
#define CONNECT_TYPE_MSG    2
#define LISTEN_TYPE_MSG     3
#define ACCEPT_TYPE_MSG     4

#define SES_CLIENT_ON       1
#define SES_CLIENT_OFF      2

#define MAX_BUFF_SESS      20

#include "stdutil/src/stdutil/stdcarr.h"
#include "link.h" /* For Reliable_Data */

typedef struct Session_d {
    int32u sess_id;
    channel sk;
    int16  type;
    byte   client_stat;
    int16u port;
    int16u total_len;
    int16u partial_len;
    int16  state;
    byte   fd_flags;    
    char   *data;
    stdcarr udp_deliver_buff;  /* Sending UDP buffer to be delivered */
    int16u sent_bytes;
    struct Reliable_Data_d *r_data;
    int32  rel_otherside_addr;
    int32  rel_otherside_port;
    int32  rel_otherside_id;
    int32  rel_orig_port;
    int    rel_hello_cnt;
    int    rel_blocked;
} Session;


void Init_Session(void);
void Session_Accept(int sk_local, int dummy, void *dummy_p);
void Session_Read(int sk, int dummy, void *dummy_p);
void Session_Close(int sk, int reason);
int  Process_Session_Packet(struct Session_d *ses);
int  Deliver_UDP_Data(char* buff, int16u buf_len, int32u type);
void Session_Write(int sk, int sess_id, void *dummy_p);
void Block_Session(struct Session_d *ses);
void Resume_Session(struct Session_d *ses);
void Block_All_Sessions(void);
void Resume_All_Sessions(void);


#endif
