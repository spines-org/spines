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


#ifndef NETWORK_H
#define NETWORK_H

#include "util/arch.h"
#include "util/scatter.h"

typedef struct Delayed_Packet_d {
    char*  header;
    char*  buff;
    int16u header_len;
    int16u buf_len;
    int32u type;
    sp_time schedule_time;
} Delayed_Packet;

void Init_Network(void);
void Init_My_Node(void);
void Init_Recv_Channel(int16 mode);
void Init_Send_Channel(int16 mode);
void Net_Recv(channel sk, int mode, void * dummy_p);
int  Read_UDP(channel sk, int mode, sys_scatter *scat);
void Up_Down_Net(int dummy_int, void *dummy_p);
void Graceful_Exit(int dummy_int, void *dummy_p);
void Proc_Delayed_Pkt(int idx, void *dummy_p);

#ifdef SPINES_SSL
/* openssl */
int DL_send_SSL(channel chan, int mode, int32 address, int16 port, sys_scatter *scat);
void Handshake_Timeout(int param, void* dummy);
void Resend_SSL(int dummy_int, void* p_data);
void print_SSL_Recv_Queue(int mode);
void print_All_Nodes();
#endif

#endif
