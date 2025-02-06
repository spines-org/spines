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


#ifndef RELIABLE_LINK_H
#define RELIABLE_LINK_H


int  Reliable_Send_Msg(int16 linkid, char *buff, int16u buff_len, int32u pack_type);
void Send_Much(int16 linkid);
void Try_to_Send(int linkid, void* dummy);
void Send_Ack(int linkid, void* dummy);
void Reliable_timeout(int linkid, void *dummy); 
void Send_Nack_Retransm(int linkid, void *dummy); 
void Process_ack_packet(int32 sender, char *buf, int16u ack_len, int32u type, int mode);
int Process_Ack(int16 linkid, char *buff, int16u ack_len, int32u type);

#endif
