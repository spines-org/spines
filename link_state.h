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


#ifndef LINK_STATE_H
#define LINK_STATE_H

#include "stdutil/src/stdutil/stdhash.h"

void Send_Link_Updates(int dummy_int, void* dummy);
void Net_Send_Link_State_All(int lk_id, void *dummy);
void Net_Send_Link_State_Updates(stdhash *hash_struct, int16 node_id);
void Process_link_state_packet(int32 sender, char *buf, int16u data_len, 
			       int16u ack_len, int32u type, int mode);
void Resend_Link_States (int dummy_int, void* dummy);
void Garbage_Collector (int dummy_int, void* dummy);

#endif
