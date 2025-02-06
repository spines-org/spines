/*
 * The Spread Toolkit.
 *     
 * The contents of this file are subject to the Spread Open-Source
 * License, Version 1.0 (the ``License''); you may not use
 * this file except in compliance with the License.  You may obtain a
 * copy of the License at:
 *
 * http://www.spread.org/license/
 *
 * or in the file ``license.txt'' found in this distribution.
 *
 * Software distributed under the License is distributed on an AS IS basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License.
 *
 * The Creators of Spread are:
 *  Yair Amir, Michal Miskin-Amir, Jonathan Stanton, John Schultz.
 *
 *  Copyright (C) 1993-2006 Spread Concepts LLC <info@spreadconcepts.com>
 *
 *  All Rights Reserved.
 *
 * Major Contributor(s):
 * ---------------
 *    Ryan Caudy           rcaudy@gmail.com - contributions to process groups.
 *    Cristina Nita-Rotaru crisn@cs.purdue.edu - group communication security.
 *    Theo Schlossnagle    jesus@omniti.com - Perl, autoconf, old skiplist.
 *    Dan Schoenblum       dansch@cnds.jhu.edu - Java interface.
 *
 *
 * This file is also licensed by Spread Concepts LLC under the Spines 
 * Open-Source License, version 1.0. You may obtain a  copy of the 
 * Spines Open-Source License, version 1.0  at:
 *
 * http://www.spines.org/LICENSE.txt
 *
 * or in the file ``LICENSE.txt'' found in this distribution.
 *
 */


#ifndef INC_DATA_LINK
#define INC_DATA_LINK

#include "arch.h"
#include "scatter.h"

#define		MAX_PACKET_SIZE		1472    /*1472 = 1536-64 (of udp)*/

#define		SEND_CHANNEL	0x00000001
#define		RECV_CHANNEL    0x00000002
#define         NO_LOOP         0x00000004
#define         REUSE_ADDR      0x00000008


channel	DL_init_channel( int32 channel_type, int16 port, int32 mcast_address, int32 interface_address );
void    DL_close_channel(channel chan);
int	DL_send( channel chan, int32 address, int16 port, sys_scatter *scat );
int	DL_recv( channel chan, sys_scatter *scat );

#ifdef SPINES_SSL
int		DL_recv_enh(channel chan, sys_scatter *scat, struct sockaddr_in *source_addr);
#endif

#endif  /* INC_DATA_LINK */
