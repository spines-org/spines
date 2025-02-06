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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "../spines_lib.h"



static int  Num_bytes;
static int  Delay;
static int  Num_rounds;
static char IP[16];
static int  spinesPort;
static int  sendPort;
static int  recvPort;
static int  Address;
static int  Send_Flag;

static void Usage(int argc, char *argv[]);

#define MAX_ROUNDS      10000
#define MAX_PACKET_SIZE  1400


int main( int argc, char *argv[] )
{
    int  sk;
    int  delays[MAX_ROUNDS];
    int  clock_diffs[MAX_ROUNDS];
    int  min_diff = 100000000;
    int  max_diff = -100000000;
    char buf[MAX_PACKET_SIZE];
    int  i, ret, num_losses, read_flag;
    int  localhost_ip;
    struct timeval *t1, *t2, *t3, *t4;
    struct timeval timeout, temp_timeout, local_recv_time, start, now, prog_life;
    struct timezone tz;
    int  *round_no, *msg_size;
    int  addr, port;
    struct timeval oneway_send, oneway_recv;
    int  avg_delay, avg_diff;
    fd_set mask, dummy_mask, temp_mask;


    Usage(argc, argv);

    localhost_ip = (127 << 24) + 1; /* 127.0.0.1 */
    
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;

    num_losses = 0;

    sk = spines_socket(spinesPort, localhost_ip, NULL);
    if(sk <= 0) {
	printf("disconnected by spines...\n");
	exit(0);
    }

    ret = spines_bind(sk, recvPort);
    if(ret <= 0) {
	printf("disconnected by spines...\n");
	exit(0);
    }
    

    t1 = (struct timeval*)buf;
    t2 = (struct timeval*)(buf+sizeof(struct timeval));
    t3 = (struct timeval*)(buf+2*sizeof(struct timeval));
    t4 = &local_recv_time;
    round_no = (int*)(buf+3*sizeof(struct timeval));
    msg_size = (int*)(buf+3*sizeof(struct timeval)+sizeof(int));

	
    FD_ZERO(&mask);
    FD_ZERO(&dummy_mask);
    FD_SET(sk,&mask);

    if(Send_Flag == 1) {
	printf("Checking %s, %d; %d byte pings, every %d milliseconds: %d rounds\n\n", 
	       IP, sendPort, Num_bytes, Delay, Num_rounds);

	gettimeofday(&start, &tz);

	for(i=0; i<Num_rounds; i++)
	{
	    *round_no = i;
	    gettimeofday(t1, &tz);
	    *msg_size = Num_bytes;
	    ret = spines_sendto(sk, Address, sendPort, buf, Num_bytes);
	    if(ret <= 0) {
		printf("disconnected by spines...\n");
		exit(0);
	    }
	    
	    read_flag = 1;
	    while(read_flag == 1) {
		temp_mask = mask;
		temp_timeout = timeout;
		select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &temp_timeout);
		
		if(FD_ISSET(sk, &temp_mask)) {
		    ret = spines_recvfrom(sk, &addr, &port, buf, sizeof(buf));
		    gettimeofday(t4, &tz);
		    if(ret <= 0) {
			printf("Disconnected by spines...\n");
			exit(0);
		    }
		    if(*round_no != i) {
			printf("err: i: %d; round_no: %d\n", i, *round_no);
			continue;
		    }
		    if(ret != Num_bytes) {
			printf("corrupted packet...\n");
			exit(0);
		    }

		    oneway_send.tv_sec = t2->tv_sec - t1->tv_sec;
		    oneway_send.tv_usec = t2->tv_usec - t1->tv_usec;
		    
		    oneway_recv.tv_sec = t4->tv_sec - t3->tv_sec;
		    oneway_recv.tv_usec = t4->tv_usec - t3->tv_usec;

		    delays[i] = oneway_send.tv_sec*1000000 + oneway_send.tv_usec +
			oneway_recv.tv_sec * 1000000 + oneway_recv.tv_usec;

		    clock_diffs[i] = oneway_send.tv_sec*1000000 + oneway_send.tv_usec -
			oneway_recv.tv_sec * 1000000 - oneway_recv.tv_usec;

		    clock_diffs[i] /= 2;

		    gettimeofday(&now, &tz);
		    prog_life.tv_sec = now.tv_sec - start.tv_sec;
		    prog_life.tv_usec = now.tv_usec - start.tv_usec;
		    if(prog_life.tv_usec < 0) {
			prog_life.tv_sec--;
			prog_life.tv_usec += 1000000;
		    }

		    printf("%4ld.%06ld - rtt: %d usec; clock diff: %d usec\n", 
			   prog_life.tv_sec, prog_life.tv_usec, delays[i],
			   clock_diffs[i]);

		    if(max_diff < clock_diffs[i])
			max_diff = clock_diffs[i];
		    if(min_diff > clock_diffs[i])
			min_diff = clock_diffs[i];
		}
		else {
		    num_losses++;
		    delays[i] = 0;
		    clock_diffs[i] = 0;
		    printf("%d: timeout; errors: %d\n", i, num_losses);
		}
		read_flag = 0;
	    }
	    usleep(Delay*1000);
	}

	avg_delay = 0;
	avg_diff = 0;
	for(i=0; i<Num_rounds; i++) {
	    avg_delay += delays[i];
	    avg_diff += clock_diffs[i];
	}
	avg_delay = avg_delay/(Num_rounds - num_losses);
	avg_diff = avg_diff/(Num_rounds - num_losses);

	printf("\nAverage rtt: %d.%d msec; Average clock diff: %d usec\n\n",
	       avg_delay/1000, avg_delay%1000, avg_diff);
	printf("max diff: %d usec; min diff: %d usec\n", max_diff, min_diff);

    }
    else {
	printf("Just answering pings on port %d\n", recvPort);
	while(1) {
	    ret = spines_recvfrom(sk, &addr, &port, buf, sizeof(buf));
	    gettimeofday(t2, &tz);
	    if(ret <= 0) {
		printf("Disconnected by spines...\n");
		exit(0);
	    }
	    if(ret != *msg_size) {
		printf("corrupted packet...\n");
		exit(0);
	    }
	    gettimeofday(t3, &tz);
	    ret = spines_sendto(sk, addr, port, buf, *msg_size);
	    if(ret <= 0) {
		printf("disconnected by spines...\n");
		exit(0);
	    }	
	}
    }
    return(1);
}




static  void    Usage(int argc, char *argv[])
{
    int i1, i2, i3, i4;
    
    /* Setting defaults */
    Num_bytes = 64;
    Delay = 1000;
    Num_rounds = 30;
    spinesPort = 8100;
    sendPort = 8400;
    recvPort = 8400;
    Address = 0;
    Send_Flag = 0;
    strcpy( IP, "127.0.0.1" );
    while( --argc > 0 ) {
	argv++;
	
	if( !strncmp( *argv, "-p", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&spinesPort );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-d", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&recvPort );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-r", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&recvPort );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-a", 2 ) ){
	    sscanf(argv[1], "%s", IP );

	    sscanf( IP ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	    Address = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
	    
	    argc--; argv++;
	}else if( !strncmp( *argv, "-b", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&Num_bytes );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-t", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&Delay );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-n", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&Num_rounds );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-s", 2 ) ){
	    Send_Flag = 1;
	}else{
	    printf( "Usage: sp_ping\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		    "\t[-p <port number>] : to connect to spines, default is 8100",
		    "\t[-d <port number>] : to send packets on, default is 8400",
		    "\t[-r <port number>] : to receive packets on, default is 8400",
		    "\t[-a <IP address> ] : IP address to send ping packets to",
		    "\t[-b <size>       ] : size of the ping packets (in bytes)",
		    "\t[-t <delay>      ] : delay between ping packets (in milliseconds)",
		    "\t[-n <rounds>     ] : number of rounds",
		    "\t[-s              ] : sender ping");
	    exit( 0 );
	}
    }
    sscanf( IP ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
    Address = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
    
    if(Num_bytes > MAX_PACKET_SIZE)
	Num_bytes = MAX_PACKET_SIZE;
    
    if(Num_bytes < 3*sizeof(struct timeval) + 2*sizeof(int))
	Num_bytes = 3*sizeof(struct timeval) + 2*sizeof(int);
    
    if(Num_rounds > MAX_ROUNDS)
	Num_rounds = MAX_ROUNDS;   
}
