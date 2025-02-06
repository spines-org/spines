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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>




static int  Num_bytes;
static int  Rate;
static int  Num_pkts;
static char IP[80];
static char filename[80];
static int  fileflag;
static int  sendPort;
static int  recvPort;
static int  Address;
static int  Send_Flag;
static int  Reliable_Flag;
static int  Sping_Flag;

static void Usage(int argc, char *argv[]);

#define MAX_PACKET_SIZE        1400


int main( int argc, char *argv[] )
{
    int  sk;
    char buf[MAX_PACKET_SIZE];
    int  i, ret;
    struct timeval *t1, *t2;
    struct timeval local_recv_time, start, now, report_time, old_time;
    struct timezone tz;
    int  *pkt_no, *msg_size;
    long long int duration_now, int_delay, oneway_time;
    double rate_now;
    int total_read;
    int sent_packets = 0;
    long elapsed_time;
    FILE *f1;

    key_t key;
    int shmid, size, opperm_flags;
    char *mem_addr;
    long long int *avg_clockdiff;
    long long int zero_diff = 0;
    long long int min_clockdiff, max_clockdiff;

    struct sockaddr_in host;
    struct sockaddr_in name;
    struct hostent     h_ent;


    Usage(argc, argv);

    if(fileflag == 1)
	f1 = fopen(filename, "wt");

    bcopy(gethostbyname(IP), &h_ent, sizeof(h_ent));
    bcopy( h_ent.h_addr, &host.sin_addr, sizeof(host.sin_addr) );

    
    msg_size = (int*)buf;
    pkt_no = (int*)(buf+sizeof(int));
    t1 = (struct timeval*)(buf+2*sizeof(int));
    t2 = &local_recv_time;
    
    min_clockdiff =  3600000;
    min_clockdiff *= 1000;
    max_clockdiff = -3600000;
    max_clockdiff *= 1000;
	
    if(Send_Flag == 1) {
	host.sin_family = AF_INET;
	host.sin_port   = htons(sendPort);

        sk = socket(AF_INET, SOCK_DGRAM, 0);
        if (sk < 0) {
	    perror("u_flooder_client: socket error");
	    exit(1);
        }


      	printf("Checking %s, %d; %d pakets of %d bytes each ", 
	       IP, sendPort, Num_pkts, Num_bytes);

	if(fileflag == 1) {
	    fprintf(f1, "Checking %s, %d; %d pakets of %d bytes each ", 
		   IP, sendPort, Num_pkts, Num_bytes);
	}	    

	if(Rate > 0) {
	    printf("at a rate of %d Kbps\n\n", Rate);
	    if(fileflag == 1) {
		fprintf(f1, "at a rate of %d Kbps\n\n", Rate);
	    }		    
	}
	else {
	    printf("\n\n");
	    if(fileflag == 1) {
		fprintf(f1, "\n\n");
	    }
	}

	gettimeofday(&start, &tz);
	report_time.tv_sec = start.tv_sec;
	report_time.tv_usec = start.tv_usec;
	
	for(i=0; i<Num_pkts; i++)
	{
	    *pkt_no = i;
	    *msg_size = Num_bytes;
	    gettimeofday(t1, &tz);	
	    
	    /* printf("-> %d.%06d\n", 
	     *	   (int)t1->tv_sec, (int)t1->tv_usec);
	     */ 

	    old_time.tv_sec = t1->tv_sec;
	    old_time.tv_usec = t1->tv_usec;


	    ret = sendto(sk, buf, Num_bytes, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));
	    if(ret != Num_bytes) {
		printf("error in writing: %d...\n", ret);
		exit(0);
	    }

	    gettimeofday(&now, &tz);
		
	    if(fileflag == 1) {
		sent_packets++;
		elapsed_time  = (now.tv_sec - report_time.tv_sec);
		elapsed_time *= 1000000;
		elapsed_time += now.tv_usec - report_time.tv_usec;
		
		if(elapsed_time >= 1000000) {
		    fprintf(f1, "%ld.%ld\t%ld\n", (long)now.tv_sec, (long)now.tv_usec, 
			    sent_packets*1000000/elapsed_time);

		    sent_packets = 0;
		    report_time.tv_sec = now.tv_sec;
		    report_time.tv_usec = now.tv_usec;
		}
	    }
	    
	    if((Rate > 0)&&(i != Num_pkts-1)) {
		duration_now  = (now.tv_sec - start.tv_sec);
		duration_now *= 1000000;
		duration_now += now.tv_usec - start.tv_usec;
		rate_now = Num_bytes;
		rate_now = rate_now * (i+1) * 8 * 1000;
		rate_now = rate_now/duration_now;

		if(rate_now > Rate) {
		    int_delay = Num_bytes;
		    int_delay = int_delay * (i+1) * 8 * 1000;
		    int_delay = int_delay/Rate; 
		    int_delay = int_delay - duration_now;

		    /*printf("   %d.%06d -> delay: %lld\n", 
		     *	   (int)now.tv_sec, (int)now.tv_usec, int_delay);
		     */

		    if((int_delay <= 0)||(int_delay > 10000000))
		     	printf("!!! BIG delay !!!  %lld\n", int_delay);
		    if(int_delay > 0)
			usleep(int_delay);
		}
	    }
	}
	*pkt_no = -1;
	*msg_size = Num_bytes;
	gettimeofday(t1, &tz);
	ret = sendto(sk, buf, Num_bytes, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));

	gettimeofday(&now, &tz);
	duration_now  = now.tv_sec - start.tv_sec;
	duration_now *= 1000000; 
	duration_now += now.tv_usec - start.tv_usec;
	
	rate_now = Num_bytes;
	rate_now = rate_now * Num_pkts * 8 * 1000;
	rate_now = rate_now/duration_now;

	printf("Avg. rate: %8.3f Kbps\n", rate_now);
	if(fileflag == 1) {
	    fprintf(f1, "Avg. rate: %8.3f Kbps\n", rate_now);
	}

    }
    else {
	printf("Just answering flooder msgs on port %d\n", recvPort);

	if(Sping_Flag == 1) {
	    key = 0x01234567;
	    size = sizeof(long long int); 
	    opperm_flags = SHM_R | SHM_W;
	    
	    shmid = shmget (key, size, opperm_flags); 
	    if(shmid == -1) {
		perror("shmget:");
		exit(0);
	    }    
	    
	    mem_addr = (char*)shmat(shmid, 0, SHM_RND);    
	    if(mem_addr == (char*)-1) {
		perror("shmat:");
		exit(0);
	    }
	    
	    avg_clockdiff = (long long int*)mem_addr;
	}
	else {
	    avg_clockdiff = &zero_diff;
	}
	
	sk = socket(AF_INET, SOCK_DGRAM, 0);
	if(sk <= 0) {
	    printf("error socket...\n");
	    exit(0);
	}
	
        name.sin_family = AF_INET;
        name.sin_addr.s_addr = INADDR_ANY;
        name.sin_port = htons(recvPort);


        if(bind(sk, (struct sockaddr *)&name, sizeof(name) ) < 0) {
	    perror("err: bind");
	    exit(1);
        }
 
	while(1) {
	    ret = recv(sk, buf, sizeof(buf), 0);
	
	    gettimeofday(t2, &tz);
	    if(ret != *msg_size) {
		printf("corrupted packet...\n");
		exit(0);
	    }

	    if(*pkt_no == -1)
		break;

	    oneway_time = (t2->tv_sec - t1->tv_sec);
	    oneway_time *= 1000000; 
	    oneway_time += t2->tv_usec - t1->tv_usec;

	    /*printf("t1: %d.%06d\n", (int)t1->tv_sec, (int)t1->tv_usec);*/

	    /* Adjusting for clock skew */
	    oneway_time += *avg_clockdiff;


	    if(min_clockdiff > *avg_clockdiff)
		min_clockdiff = *avg_clockdiff;
	    if(max_clockdiff < *avg_clockdiff)
		max_clockdiff = *avg_clockdiff;
	   
	    if(fileflag == 1) {
		fprintf(f1, "%d\t%lld\n", *pkt_no+1, oneway_time);
	    }	    
	}	    
	if(Sping_Flag == 1) {
	    ret = shmdt(mem_addr);
	    if(ret == -1) {
		perror("shmdt:");
		exit(0);
	    }
	}
	if(fileflag == 1) {
	    fprintf(f1, "# min_clockdiff: %lld; max_clockdiff: %lld; => %lld\n",
		    min_clockdiff, max_clockdiff, max_clockdiff - min_clockdiff);
	}
	
    }

    if(fileflag == 1) {
	fclose(f1);
    }
    usleep(2000000);
    return(1);
}




static  void    Usage(int argc, char *argv[])
{
    /* Setting defaults */
    Num_bytes = 1000;
    Rate = -1;
    Num_pkts = 10000;
    sendPort = 8400;
    recvPort = 8400;
    Address = 0;
    Send_Flag = 0;
    Sping_Flag = 0;
    fileflag = 0;
    Reliable_Flag = 0;
    strcpy( IP, "127.0.0.1" );
    while( --argc > 0 ) {
	argv++;
	
	if( !strncmp( *argv, "-d", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&sendPort );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-r", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&recvPort );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-a", 2 ) ){
	    sscanf(argv[1], "%s", IP );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-b", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&Num_bytes );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-R", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&Rate );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-n", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&Num_pkts );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-s", 2 ) ){
	    Send_Flag = 1;
	}else if( !strncmp( *argv, "-p", 2 ) ){
	    Sping_Flag = 1;
	}else if( !strncmp( *argv, "-f", 2 ) ){
	    sscanf(argv[1], "%s", filename );
	    fileflag = 1;
	    argc--; argv++;
	}else{
	    printf( "Usage: sp_flooder\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		    "\t[-d <port number>] : to send packets on, default is 8400",
		    "\t[-r <port number>] : to receive packets on, default is 8400",
		    "\t[-a <address>    ] : address to send packets to",
		    "\t[-b <size>       ] : size of the packets (in bytes)",
		    "\t[-R <rate>       ] : sending rate (in 1000's of bits per sec)",
		    "\t[-n <rounds>     ] : number of packets",
		    "\t[-f <filename>   ] : file where to save statistics",
		    "\t[-p              ] : run with sping for clock sync",
		    "\t[-s              ] : sender flooder");
	    exit( 0 );
	}
    }
    
    if(Num_bytes > MAX_PACKET_SIZE)
	Num_bytes = MAX_PACKET_SIZE;
    
    if(Num_bytes < sizeof(struct timeval) + 2*sizeof(int))
	Num_bytes = sizeof(struct timeval) + 2*sizeof(int);
    
}
