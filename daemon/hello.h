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


#ifndef HELLO_H
#define HELLO_H

#define DEAD_LINK_CNT     10 /* Number of hellos unacked until 
			       declaring a dead link */

void Init_Connections(void);
void Send_Hello(int linkid, void* dummy);
void Send_Hello_Request(int linkid, void* dummy);
void Send_Hello_Request_Cnt(int linkid, void* dummy);
void Send_Hello_Ping(int dummy_int, void* dummy);
void Send_Discovery_Hello_Ping(int dummy_int, void* dummy);
void Net_Send_Hello(int16 linkid, int mode);
void Net_Send_Hello_Ping(int32 address);
void Process_hello_packet(char *buf , int remaining_bytes, int32 sender, 
			  int32u type);
void Process_hello_ping_packet(int32 sender, int mode);

#endif
