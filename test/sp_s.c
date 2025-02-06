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
 *  Yair Amir, Michal Miskin-Amir, Jonathan Stanton.
 *
 *  Copyright (C) 1993-2003 Spread Concepts LLC <spread@spreadconcepts.com>
 *
 *  All Rights Reserved.
 *
 * Major Contributor(s):
 * ---------------
 *    Cristina Nita-Rotaru crisn@cnds.jhu.edu - group communication security.
 *    Theo Schlossnagle    jesus@omniti.com - Perl, skiplists, autoconf.
 *    Dan Schoenblum       dansch@cnds.jhu.edu - Java interface.
 *    John Schultz         jschultz@cnds.jhu.edu - contribution to process group membership.
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


/********************************************************************/
/*                                                                  */
/* sp_s.c                                                           */
/*                                                                  */
/* This is a modified version from the s.c file provided in the     */
/* Spread Toolkit. It is basically the same file but instead of     */
/* using DL_send / receive / init calls, it uses Spines functions.  */
/*                                                                  */
/* The only use of sp_s.c file is as an application to test the     */
/* performance of Spines using a generic, external program.         */
/*                                                                  */
/********************************************************************/



#ifndef ARCH_PC_WIN95
#include <unistd.h>
#endif

#include <string.h>
#include <stdlib.h>
#include "../util/arch.h"
#include "../util/alarm.h"
#include "../util/sp_events.h"
#include "../util/data_link.h"

#include "../spines_lib.h"

#ifdef	ARCH_PC_WIN95

#include	<winsock.h>

WSADATA		WSAData;

#endif	/* ARCH_PC_WIN95 */

static  int     Num_bytes;
static  int     Num_packets;
static	char	IP[16];
static	int16	Port;
static	int16	destPort;
static	int16	recvPort;
static	sp_time	Delay;
static	int	Burst;
static  int32   Max_Count;

static	void 	Usage( int argc, char *argv[] );

int main( int argc, char *argv[] )
{

	channel chan;
	char	buf[100000];
	int	ret,i,i1,i2,i3,i4;
	int	address;
	int32	*type;
	int32	*count;
	int32	*size;
	sp_time	start, end, total_time;
	int	total_problems=0;
	int     localhost_ip;


	Alarm_set( NONE ); 

	Usage( argc, argv );

	localhost_ip = 127 << 24;
	localhost_ip += 1;


#ifdef	ARCH_PC_WIN95

	ret = WSAStartup( MAKEWORD(1,1), &WSAData );
	if( ret != 0 )
		Alarm( EXIT, "s: winsock initialization error %d\n", ret );

#endif	/* ARCH_PC_WIN95 */


	/*	
	 * chan = DL_init_channel( SEND_CHANNEL, Port, 0, 0 );
	 */


	chan = spines_socket(Port, localhost_ip, NULL);

	if(chan <= 0) {
	    printf("disconnected by spines...\n");
	    exit(0);
	}
	    

	/*
	 * scat.num_elements = 1;
	 * scat.elements[0].buf = buf;
	 * scat.elements[0].len = Num_bytes;	 
	 */

	sscanf( IP ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	address = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );

	printf("Checking (%d.%d.%d.%d, %d). Each burst has %d packets, %d bytes each with %d msec delay in between, for a total of %d packets\n",i1,i2,i3,i4, Port, Burst, Num_bytes, (int)(Delay.usec/1000+Delay.sec*1000), (int)Num_packets );

	type  = (int32 *)buf;
	count = (int32 *)&buf[4];
	size  = (int32 *)&buf[8];

	*type = Set_endian( 0 );
	*size = Num_bytes;


	start = E_get_time();

	for(i=1; i<= Num_packets; i++ )
	{
		*count = i;
		
		/*
		 * ret = DL_send( chan, address, Port, &scat );
		 */

		ret = spines_sendto(chan, address, destPort, buf, Num_bytes);

		if(ret <= 0) {
		    printf("disconnected by spines...\n");
		    exit(0);
		}

		if( ret != Num_bytes) 
		{
			total_problems++;
			i--;
		}
		if( i%Burst == 0 ) {
		    if((Delay.sec > 0) || (Delay.usec > 0)) {
			E_delay( Delay );
		    }
		} 
		if( i%1000  == 0) printf("sent %d packets of %d bytes\n",i, Num_bytes);
	}
	end = E_get_time();
	total_time = E_sub_time( end, start );
	Delay.usec = 10000;
	*type = Set_endian( 1 );
	E_delay( Delay );

	/*
	 * ret = DL_send( chan, address, Port, &scat );
	 */

	ret = spines_sendto(chan, address, destPort, buf, Num_bytes);

	printf("total time is (%d,%d), with %d problems \n",(int)total_time.sec, (int)total_time.usec, total_problems );

	return(1);
}

static  void    Usage(int argc, char *argv[])
{
        /* Setting defaults */
	Num_bytes = 1024;
	Num_packets = 10000;
	strcpy( IP, "127.0.0.1" );
	Port = 8100;
	recvPort = 8200;
	destPort = 8200;
	Delay.sec = 0;
	Delay.usec = 10000;
	Burst	  = 100;
	Max_Count = 0;

	while( --argc > 0 )
	{
		argv++;

                if( !strncmp( *argv, "-t", 2 ) )
		{
			sscanf(argv[1], "%d", (int*)&Delay.usec );
			Delay.sec = 0;
			Delay.usec *= 1000;
			if( Delay.usec > 1000000 )
			{
				Delay.sec  = Delay.usec / 1000000;
				Delay.usec = Delay.usec % 1000000;
			}
			argc--; argv++;
		}else if( !strncmp( *argv, "-p", 2 ) ){
			sscanf(argv[1], "%d", (int*)&Port );
			argc--; argv++;
		}else if( !strncmp( *argv, "-r", 2 ) ){
			sscanf(argv[1], "%d", (int*)&recvPort );
			argc--; argv++;
		}else if( !strncmp( *argv, "-d", 2 ) ){
			sscanf(argv[1], "%d", (int*)&destPort );
			argc--; argv++;
		}else if( !strncmp( *argv, "-b", 2 ) ){
			sscanf(argv[1], "%d", &Burst );
			argc--; argv++;
		}else if( !strncmp( *argv, "-n", 2 ) ){
			sscanf(argv[1], "%d", &Num_packets );
			argc--; argv++;
		}else if( !strncmp( *argv, "-s", 2 ) ){
			sscanf(argv[1], "%d", &Num_bytes );
			argc--; argv++;
		}else if( !strncmp( *argv, "-a", 2 ) ){
			sscanf(argv[1], "%s", IP );
			argc--; argv++;
                }else{
			printf( "Usage: \n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
				"\t[-p <port number>] : of on daemon, default is 8100",
				"\t[-r <port number>] : to receive on, default is 8200",
				"\t[-d <port number>] : to send on, default is 8200",
				"\t[-b <burst>]       : number of packets in each burst, default is 100",
				"\t[-t <delay>]       : time (mili-secs) to wait between bursts, default 10",
				"\t[-n <num packets>] : total number of packets to send, default is 10000",
				"\t[-s <num bytes>]   : size of each packet, default is 1024",
				"\t[-a <IP address>]  : default is 127.0.0.1" );
			exit( 0 );
		}
	}
}

