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
 * Open-Source License, version 1.0
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
/* sp_r.c                                                           */
/*                                                                  */
/* This is a modified version from the r.c file provided in the     */
/* Spread Toolkit. It is basically the same file but instead of     */
/* using DL_send / receive / init calls, it uses Spines functions.  */
/*                                                                  */
/* The only use of sp_r.c file is as an application to test the     */
/* performance of Spines using a generic, external program.         */
/*                                                                  */
/********************************************************************/


#include <string.h>
#include <stdlib.h>

#include "../util/arch.h"
#include "../util/alarm.h"
#include "../util/data_link.h"

#include "../spines_lib.h"


#ifdef	ARCH_PC_WIN95

#include	<winsock.h>

WSADATA		WSAData;

#endif	/* ARCH_PC_WIN95 */

static	char	IP[16];
static	int16	Port;
static	int16	recvPort;
static	int32	Address;
static	int	Detailed_report;

static  void    Usage( int argc, char *argv[] );

int main( int argc, char *argv[] )
{

	channel chan;
	int32 addr;
	int32 port;
	char	buf[100000];
	int	ret,i;
	int	*type;
	int	*count;
	int	*size;
	int	missed;
	int	corrupt;
	int	total_missed;
	int     localhost_ip;

	Usage( argc, argv );

	localhost_ip = 127 << 24;
	localhost_ip += 1;

	Alarm_set( NONE ); 

#ifdef	ARCH_PC_WIN95

	ret = WSAStartup( MAKEWORD(1,1), &WSAData );
	if( ret != 0 )
		Alarm( EXIT, "r: winsock initialization error %d\n", ret );

#endif	/* ARCH_PC_WIN95 */

	/*
	 * chan = DL_init_channel( RECV_CHANNEL, Port, Address, 0 );
	 */

	chan = spines_socket(Port, localhost_ip, NULL);

	if(chan <= 0) {
	    printf("Disconnected by spines...\n");
	    exit(0);
	}


	ret = spines_bind(chan, recvPort);
	if(ret <= 0) {
	    printf("Bind error...\n");
	    exit(0);
	}

	if(Address != 0) {
	    ret = spines_join(chan, Address);
	    if(ret <= 0) {
		printf("Join error...\n");
		exit(0);
	    }
	}

	/*
	 * scat.num_elements = 1;
	 * scat.elements[0].buf = buf;
	 * scat.elements[0].len = sizeof(buf);
	 */

	type  = (int32 *)buf;
	count = (int32 *)&buf[4];
	size  = (int32 *)&buf[8];

	*count = 0;
	corrupt = 0;
	total_missed = 0;

printf("Ready to receive on port %d\n", recvPort );

	for(i=1; ; i++ )
	{
	        /*
		 * ret = DL_recv( chan, &scat );
		 */

	        ret = spines_recvfrom(chan, &addr, &port, buf, sizeof(buf));
		
		if(ret <= 0) {
		    printf("Disconnected by spines...\n");
		    exit(0);
		}


		if( !Same_endian( *type ) )
		{
			*type  = Flip_int32( *type  );
			*count = Flip_int32( *count );
			*size  = Flip_int32( *size  );
		}
		*type  = Clear_endian( *type );


		if( *size != ret )
		{
			i--;
			corrupt++;
			printf("Corruption: expected packet size is %d, received packet size is %d\n", *size, ret);
			continue;
		}
		if( *type )
		{
			i--;
			missed = *count - i;
			total_missed += missed;
			printf("-------\n");
			printf("Report: total packets %d, total missed %d, total corrupted %d\n", *count, total_missed, corrupt );
			printf("-------\n");

			i = 0;
			total_missed = 0;
			corrupt = 0;
			continue;
		}
		if (*count > i)
		{
			missed = *count - i;
			total_missed += missed;
			if( Detailed_report ) 
				printf(" --> count is %d, i is %d, missed %d total missed %d, corrupt %d\n", *count,i, missed, total_missed, corrupt );
			 i = *count;
		}else if(*count < i){
			printf("-------\n");
			printf("Report: total packets at least %d, total missed %d, total corrupted %d\n", i-1, total_missed, corrupt );
			printf( "Initiating count from %d to %d\n",i-1,*count);
			printf("-------\n");

			i = *count;
			total_missed = *count;
			corrupt = 0;
		}
	}
	return(1);
}

static  void    Usage(int argc, char *argv[])
{
	int i1, i2, i3, i4;

        /* Setting defaults */
	Port = 8100;
	recvPort = 8200;
	Address = 0;
	Detailed_report= 0;
	while( --argc > 0 )
	{
		argv++;

                if( !strncmp( *argv, "-d", 2 ) )
		{
			Detailed_report = 1;
			argc--;
		}else if( !strncmp( *argv, "-p", 2 ) ){
			sscanf(argv[1], "%d", (int*)&Port );
			argc--; argv++;
		}else if( !strncmp( *argv, "-r", 2 ) ){
			sscanf(argv[1], "%d", (int*)&recvPort );
			argc--; argv++;
                }else if( !strncmp( *argv, "-j", 2 ) ){
			sscanf(argv[1], "%s", IP );

			sscanf( IP ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
			Address = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );

			argc--; argv++;
                }else{
			printf( "Usage: r\n%s\n%s\n%s\n%s\n",
				"\t[-p <port number>] : to connect to on, default is 8100",
				"\t[-r <port number>] : to receive on, default is 8200",
				"\t[-j <multicast class D address>] : to join a multicast address",
				"\t[-d ]              : print a detailed report whenever messages are missed");
			exit( 0 );
		}
	}
}

