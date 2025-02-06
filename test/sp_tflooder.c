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
 * Copyright (c) 2003 - 2005 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
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
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "../spines_lib.h"



static int  Num_bytes;
static int  Rate;
static int  Num_pkts;
static char IP[80];
static char SP_IP[80];
static char filename[80];
static int  fileflag;
static int  spinesPort;
static int  sendPort;
static int  recvPort;
static int  Send_Flag;
static int  Sping_Flag;
static int  Protocol;

static void Usage(int argc, char *argv[]);

#define MAX_PKT_SIZE  1000000


void isleep(int usec)
{
    int diff;
    struct timeval start, now;
    struct timezone tz;

    gettimeofday(&start, &tz);    
    diff = 0;
    
    while(diff < usec) {
	if(usec - diff > 11000) {
	    usleep(1);
	}
	gettimeofday(&now, &tz);
	diff = now.tv_sec - start.tv_sec;
	diff *= 1000000;
	diff += now.tv_usec - start.tv_usec;
    }
}



int main( int argc, char *argv[] )
{
    int  sk, sk_listen;
    char buf[MAX_PKT_SIZE];
    int  i, ret;
    struct timeval *t1, *t2;
    struct timeval local_recv_time, start, now, report_time;
    struct timezone tz;
    int  *pkt_no, *msg_size, total_read;
    long long int duration_now, int_delay, oneway_time;
    long long int cnt;
    double rate_now;
    int sent_packets = 0;
    long elapsed_time;
    FILE *f1 = NULL;

    key_t key;
    int shmid, size, opperm_flags;
    char *mem_addr = NULL;
    long long int *avg_clockdiff;
    long long int zero_diff = 0;
    long long int min_clockdiff, max_clockdiff;

    struct sockaddr_in host, serv_addr, name, remote;
    int remote_addr_sz;
    struct hostent     h_ent;
    struct hostent  *host_ptr;
    char   machine_name[256];

    unsigned char * p_ip;


    Usage(argc, argv);

    if(fileflag == 1)
	f1 = fopen(filename, "wt");

    if(strcmp(SP_IP, "") != 0) {
	memcpy(&h_ent, gethostbyname(SP_IP), sizeof(h_ent));
	memcpy( &serv_addr.sin_addr, h_ent.h_addr, sizeof(struct in_addr) );
    }
    else {
	gethostname(machine_name,sizeof(machine_name)); 
	host_ptr = gethostbyname(machine_name);
	
	if(host_ptr == NULL) {
	    printf("Init_My_Node: could not get my ip address (my name is %s)\n",
		   machine_name );
	    exit(1);
	}
	if(host_ptr->h_addrtype != AF_INET) {
	    printf("Init_My_Node: Sorry, cannot handle addr types other than IPv4\n");
	exit(1);
	}
	
	if(host_ptr->h_length != 4) {
	    printf("Conf_init: Bad IPv4 address length\n");
	    exit(1);
	}
	memcpy(&serv_addr.sin_addr, host_ptr->h_addr, sizeof(struct in_addr));
    }
    serv_addr.sin_port = htons(spinesPort);
    
    if(spines_init((struct sockaddr*)(&serv_addr)) < 0) {
	printf("flooder_client: socket error\n");
	exit(1);
    }

    if(strcmp(IP, "") != 0) {
	memcpy(&h_ent, gethostbyname(IP), sizeof(h_ent));
	memcpy( &host.sin_addr, h_ent.h_addr, sizeof(host.sin_addr) );
    }
    else {
	memcpy(&host.sin_addr, &serv_addr.sin_addr, sizeof(struct in_addr));
    }
    
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

        sk = spines_socket(PF_SPINES, SOCK_STREAM, Protocol, NULL);
        if (sk < 0) {
	    printf("flooder_client: socket error\n");
	    exit(1);
        }

        ret = spines_connect(sk, (struct sockaddr *)&host, sizeof(host));
        if( ret < 0)
        {
                printf( "sp_flooder: could not connect to server\n"); 
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
	    *pkt_no = htonl(i);
	    *msg_size = htonl(Num_bytes);
	    gettimeofday(t1, &tz);	

	    ret = spines_send(sk, buf, Num_bytes, 0);
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
			printf("!!!!!!!!!!!!!! %lld\n", int_delay);
		    if(int_delay > 0)
			isleep(int_delay);
		}
	    }
	}
	*pkt_no = htonl(-1);
	*msg_size = htonl(Num_bytes);
	gettimeofday(t1, &tz);
	ret = spines_send(sk, buf, Num_bytes, 0);
	if(ret != Num_bytes) {
	    printf("error in writing: %d...\n", ret);
	    exit(0);
	}


	gettimeofday(&now, &tz);
	duration_now  = now.tv_sec - start.tv_sec;
	duration_now *= 1000000; 
	duration_now += now.tv_usec - start.tv_usec;

	rate_now = Num_bytes;
	rate_now = rate_now * Num_pkts * 8 * 1000;
	rate_now = rate_now/duration_now;

	printf("rate: %5.3f\n", rate_now);
	if(fileflag == 1) {
	    fprintf(f1, "rate: %5.3f\n", rate_now);
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


	sk_listen = spines_socket(PF_SPINES, SOCK_STREAM, Protocol, NULL);
	if(sk_listen <= 0) {
	    printf("error socket...\n");
	    exit(0);
	}

        name.sin_family = AF_INET;
        name.sin_addr.s_addr = INADDR_ANY;
        name.sin_port = htons(recvPort);


	ret = spines_bind(sk_listen, (struct sockaddr *)&name, sizeof(name));
	if(ret < 0) {
	    printf("disconnected by spines...\n");
	    exit(0);
	}	
    
        if(spines_listen(sk_listen, 0) < 0) {
	    perror("err: listen");
	    exit(1);
        }

	remote_addr_sz = sizeof(remote);
	sk = spines_accept(sk_listen, (struct sockaddr *)&remote, &remote_addr_sz);
	if(sk < 0) {
	    perror("err: accept");
	    exit(1);	
	}
	p_ip = (unsigned char*) &remote.sin_addr.s_addr;
	printf("accept from %u.%u.%u.%u port %d\r\n", p_ip[0], p_ip[1], p_ip[2], p_ip[3], remote.sin_port);

	gettimeofday(&start, &tz);

	Num_pkts = 0;

	cnt = 0;
	while(1) {
	    total_read = 0;
	    while(total_read < sizeof(int)) {
	        /*printf("receiving\n");*/
		ret = spines_recv(sk, buf+total_read, sizeof(int) - total_read, 0);
		if(ret <= 0) {
		    printf("err reading... read until now: %d; ret: %d\n", total_read, ret);
		    exit(0);
		}
		total_read += ret;		
	    }

	    *msg_size = ntohl(*msg_size);

	    while(total_read < *msg_size) {
		ret = spines_recv(sk, buf+total_read, *msg_size - total_read, 0);
		if(ret <= 0) {
		    printf("err reading... read until now:  %d\n", total_read);
		    exit(0);
		}
		total_read += ret;
	    }

	    *pkt_no = ntohl(*pkt_no);
	    

	    gettimeofday(t2, &tz);
	    if(total_read != *msg_size) {
		printf("corrupted packet...total_read: %d; msg_size: %d\n",
		       total_read, *msg_size);
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
	   

	    cnt++;
	    if(fileflag == 1) {
		fprintf(f1, "%d.%06d\t%d\t%lld\n", (int)t2->tv_sec, 
			(int)t2->tv_usec, *pkt_no+1, oneway_time);

		printf("%d.%06d\t%d\t%lld\n", (int)t2->tv_sec, 
		       (int)t2->tv_usec, *pkt_no+1, oneway_time);
		fflush(f1);
	    }

	    Num_pkts++;
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

	gettimeofday(&now, &tz);
	duration_now  = now.tv_sec - start.tv_sec;
	duration_now *= 1000000; 
	duration_now += now.tv_usec - start.tv_usec;
	
	rate_now = *msg_size;
	rate_now = rate_now * Num_pkts * 8 * 1000;
	rate_now = rate_now/duration_now;
	printf("Avg. rate: %8.3f Kbps\n", rate_now);

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
    spinesPort = 8100;
    sendPort = 8400;
    recvPort = 8400;
    fileflag = 0;
    Sping_Flag = 0;
    Send_Flag = 0;
    Protocol = 0;
    strcpy(IP, "");
    strcpy(SP_IP, "");
    while( --argc > 0 ) {
	argv++;
	
	if( !strncmp( *argv, "-p", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&spinesPort );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-d", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&sendPort );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-r", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&recvPort );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-a", 2 ) ){
	    sscanf(argv[1], "%s", IP );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-o", 2 ) ){
	    sscanf(argv[1], "%s", SP_IP );
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
	}else if( !strncmp( *argv, "-g", 2 ) ){
	    Sping_Flag = 1;
	}else if( !strncmp( *argv, "-P", 2 ) ){
	    sscanf(argv[1], "%d", (int*)&Protocol );
	    argc--; argv++;
	}else if( !strncmp( *argv, "-f", 2 ) ){
	    sscanf(argv[1], "%s", filename );
	    fileflag = 1;
	    argc--; argv++;
	}else{
	    printf( "Usage: sp_flooder\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		    "\t[-o <address>    ] : address where spines runs, default localhost",
		    "\t[-p <port number>] : port where spines runs, default is 8100",
		    "\t[-d <port number>] : to send packets on, default is 8400",
		    "\t[-r <port number>] : to receive packets on, default is 8400",
		    "\t[-a <address>    ] : address to send packets to",
		    "\t[-b <size>       ] : size of the packets (in bytes)",
		    "\t[-R <rate>       ] : sending rate (in 1000's of bits per sec)",
		    "\t[-n <rounds>     ] : number of packets",
		    "\t[-f <filename>   ] : file where to save statistics",
		    "\t[-g              ] : run with sping for clock sync",
		    "\t[-P <0, 1 or 2>  ] : overlay links (0 : UDP; 1; Reliable; 2: Soft Realtime)",
		    "\t[-s              ] : sender flooder");
	    exit( 0 );
	}
    }
    
    if(Num_bytes > MAX_PKT_SIZE)
	Num_bytes = MAX_PKT_SIZE;
    
    if(Num_bytes < sizeof(struct timeval) + 2*sizeof(int))
	Num_bytes = sizeof(struct timeval) + 2*sizeof(int);
    
}
