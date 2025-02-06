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
#include <sys/ioctl.h>
#include <netinet/in.h> 
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>


#ifndef socklen_t
#define socklen_t int
#endif 


static int  Num_bytes;
static int  Rate;
static int  Num_pkts;
static char IP[80];
static char MCAST_IP[80];
static char filename[80];
static int  fileflag;
static int  sendPort;
static int  recvPort;
static int  Address;
static int  Send_Flag;
static int  Reliable_Flag;
static int  Sping_Flag;
static int  Forwarder_Flag;
static int  Group_Address;
static int  Realtime;

static void Usage(int argc, char *argv[]);
int max_rcv_buff(int sk);
int max_snd_buff(int sk);

#define MAX_PACKET_SIZE        1400


void isleep(int usec)
{
    int diff;
    struct timeval start, now;
    struct timezone tz;

    gettimeofday(&start, &tz);    
    diff = 0;
    while(diff < usec) {
        /* If enough time to sleep, otherwise, busywait */
        if(usec - diff > 200) {
            usleep(usec-20);
        }
        gettimeofday(&now, &tz);
        diff = now.tv_sec - start.tv_sec;
        diff *= 1000000;
        diff += now.tv_usec - start.tv_usec;
    }
}



int main( int argc, char *argv[] )
{
    int  sk, recv_count, first_pkt_flag;
    char buf[MAX_PACKET_SIZE];
    int  i, ret, ioctl_cmd, block_count;
    struct timeval *t1, *t2;
    struct timeval local_recv_time, start, now, report_time, old_time;
    struct timezone tz;
    int  *pkt_no, *msg_size, last_seq, err_cnt;
    long long int duration_now, int_delay, oneway_time;
    double rate_now;
    int sent_packets = 0;
    long elapsed_time;
    struct ip_mreq mreq;
    FILE *f1 = NULL;

    key_t key;
    int shmid, size, opperm_flags;
    char *mem_addr = NULL;
    long long int *avg_clockdiff;
    long long int zero_diff = 0;
    long long int min_clockdiff, max_clockdiff;

    struct sockaddr_in host;
    struct sockaddr_in name;
    struct hostent     h_ent;


    Usage(argc, argv);

    if(fileflag == 1)
    f1 = fopen(filename, "wt");

    memcpy(&h_ent, gethostbyname(IP), sizeof(h_ent));
    memcpy( &host.sin_addr, h_ent.h_addr, sizeof(host.sin_addr) );

    
    msg_size = (int*)buf;
    pkt_no = (int*)(buf+sizeof(int));
    t1 = (struct timeval*)(buf+2*sizeof(int));
    t2 = &local_recv_time;
    
    min_clockdiff =  3600000;
    min_clockdiff *= 1000;
    max_clockdiff = -3600000;
    max_clockdiff *= 1000;
    
    if (Forwarder_Flag == 1) {
        sk = socket(AF_INET, SOCK_DGRAM, 0);
        if (sk < 0) {
            perror("u_flooder_client: socket error");
            exit(1);
        }
        max_rcv_buff(sk);
        max_snd_buff(sk);

        host.sin_family = AF_INET;
        host.sin_port   = htons(sendPort);

        name.sin_family = AF_INET;
        name.sin_addr.s_addr = INADDR_ANY;
        name.sin_port = htons(recvPort);

        if(bind(sk, (struct sockaddr *)&name, sizeof(name) ) < 0) {
            perror("err: bind");
            exit(1);
        }

        if(Group_Address != 0) {
            mreq.imr_multiaddr.s_addr = htonl(Group_Address);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            if(setsockopt(sk, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq)) < 0) {
                printf("Mcast: problem in setsockopt to join multicast address");
                exit(0);
            }	    
        }

        recv_count = 0;
        first_pkt_flag = 1;

        while(1) {
            ret = recv(sk, buf, sizeof(buf), 0);
            if (first_pkt_flag) {
                gettimeofday(&start, &tz);
                first_pkt_flag = 0;
            }
            recv_count++;
            Num_bytes = sendto(sk, buf, ret, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));
            if(ret != Num_bytes) {
                printf("error in writing: %d...\n", ret);
                exit(0);
            }
            if (recv_count%1000 == 0) {
                gettimeofday(&now, &tz);
                duration_now  = now.tv_sec - start.tv_sec;
                duration_now *= 1000000; 
                duration_now += now.tv_usec - start.tv_usec;
    
                rate_now = (double)Num_bytes * recv_count * 8 * 1000;
                rate_now = rate_now/duration_now;

                printf("Forwarder: Pkt Size: %d   Avg. rate: %8.3lf Kbps\n", Num_bytes, rate_now);
            }
        }
    } else if(Send_Flag == 1) {
        sk = socket(AF_INET, SOCK_DGRAM, 0);
        if (sk < 0) {
            perror("u_flooder_client: socket error");
            exit(1);
        }
        max_snd_buff(sk);

        /* Bind to control the source port of sent packets, 
           so that we can go over to a NATed network */
        name.sin_family = AF_INET;
        name.sin_addr.s_addr = INADDR_ANY;
        name.sin_port = htons(recvPort);

        if(bind(sk, (struct sockaddr *)&name, sizeof(name) ) < 0) {
            perror("err: bind");
            exit(1);
        }
        if (Realtime) {
            /* Realtime here means, if I can't send, drop it, and
             * worry only about the next packet.  This will happen 
             * if the sending buffer was full, which is pretty bad 
             * already for time-sensitive data. */
            ioctl_cmd = 1;
            ret = ioctl(sk, FIONBIO, &ioctl_cmd);
            if (ret == -1) {
                perror("err: ioctl");
                exit(1);
            }
        }

        host.sin_family = AF_INET;
        host.sin_port   = htons(sendPort);

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
        block_count = 0;
    
        for(i=0; i<Num_pkts; i++)
        {
            *pkt_no = i;
            *msg_size = Num_bytes;
            gettimeofday(t1, &tz);  
        
            /* printf("-> %d.%06d\n", 
             *     (int)t1->tv_sec, (int)t1->tv_usec);
             */ 

            old_time.tv_sec = t1->tv_sec;
            old_time.tv_usec = t1->tv_usec;


            ret = sendto(sk, buf, Num_bytes, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));
            gettimeofday(&now, &tz);

            if (ret < 0) {
                if((errno == EWOULDBLOCK)||(errno == EAGAIN)) {
                    block_count++;
                    //printf("Dropped %d:%d\n", i, block_count);
                } else {
                    printf("error in writing: %d...\n", ret);
                    exit(0);
                }
            }
            else if(ret != Num_bytes) {
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
                     *     (int)now.tv_sec, (int)now.tv_usec, int_delay);
                     */

                    if((int_delay <= 0)||(int_delay > 10000000))
                        printf("!!! BIG delay !!!  %lld\n", int_delay);
                    if(int_delay > 0)
                        isleep(int_delay);
                } 
            }
        }
        *pkt_no = -1;
        *msg_size = Num_bytes;
        gettimeofday(t1, &tz);
        ret = sendto(sk, buf, Num_bytes, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));
        ret = sendto(sk, buf, Num_bytes, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));
        ret = sendto(sk, buf, Num_bytes, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));
        ret = sendto(sk, buf, Num_bytes, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));
        ret = sendto(sk, buf, Num_bytes, 0, (struct sockaddr *)&host, sizeof(struct sockaddr));

        gettimeofday(&now, &tz);
        duration_now  = now.tv_sec - start.tv_sec;
        duration_now *= 1000000; 
        duration_now += now.tv_usec - start.tv_usec;
    
        rate_now = Num_bytes;
        rate_now = rate_now * Num_pkts * 8 * 1000;
        rate_now = rate_now/duration_now;

        printf("Sender: Avg. rate: %8.3f Kbps\n", rate_now);
        if(fileflag == 1) {
            fprintf(f1, "Sender: Avg. rate: %8.3f Kbps\n", rate_now);
        }
        if(Realtime) {
            printf("   RT: Dropped %d packets \n", block_count);
        }
    } else {
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

        if(Group_Address != 0) {
            mreq.imr_multiaddr.s_addr = htonl(Group_Address);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            if(setsockopt(sk, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq)) < 0) {
                printf("Mcast: problem in setsockopt to join multicast address");
                exit(0);
            }	    
        }
 
        err_cnt = 0;
        last_seq = 0;
        recv_count = 0;
        first_pkt_flag = 1;
        while(1) {
            ret = recv(sk, buf, sizeof(buf), 0);
            if (first_pkt_flag) {
                gettimeofday(&start, &tz);
                Num_bytes = ret;
                first_pkt_flag = 0;
            }
            recv_count++;
    
            gettimeofday(t2, &tz);
            if(ret != *msg_size) {
                printf("corrupted packet...\n");
                exit(0);
            }
            if(*pkt_no == -1)
                break;

            if(*pkt_no > last_seq + 1) {
                for(i=last_seq + 1; i<*pkt_no; i++) {
                    printf("lost: %d\n", i);
                }
                printf("\n----------------------\n");
                err_cnt += *pkt_no - last_seq - 1;
            }
            if(*pkt_no > last_seq) {
                last_seq = *pkt_no;
            }

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
        gettimeofday(&now, &tz);
        duration_now  = now.tv_sec - start.tv_sec;
        duration_now *= 1000000; 
        duration_now += now.tv_usec - start.tv_usec;
    
        rate_now = (double)Num_bytes * recv_count * 8 * 1000;
        rate_now = rate_now/duration_now;

        printf("Receiver: Pkt Size: %d   Errors: %d   Avg. rate: %8.3lf Kbps\n", Num_bytes, err_cnt, rate_now);
        /*  if(fileflag == 1) {
         *      fprintf(f1, "# min_clockdiff: %lld; max_clockdiff: %lld; => %lld\n",
         *          min_clockdiff, max_clockdiff, max_clockdiff - min_clockdiff);
         *  }
         */ 
    }

    if(fileflag == 1) {
        fclose(f1);
    }
    usleep(2000000);
    return(1);
}

int max_rcv_buff(int sk)
{
    /* Increasing the buffer on the socket */
    int i, val, ret;
    socklen_t lenval;

    for(i=10; i <= 100; i+=5)
    {
      val = 1024*i;
      ret = setsockopt(sk, SOL_SOCKET, SO_RCVBUF, (void *)&val, sizeof(val));
      if (ret < 0) 
          break;
      lenval = sizeof(val);
      ret = getsockopt(sk, SOL_SOCKET, SO_RCVBUF, (void *)&val, &lenval);
      if(val < i*1024 ) 
          break;
    }
    return(1024*(i-5));
}

int max_snd_buff(int sk)
{
    /* Increasing the buffer on the socket */
    int i, val, ret;
    socklen_t lenval;

    for(i=10; i <= 100; i+=5)
    {
      val = 1024*i;
      ret = setsockopt(sk, SOL_SOCKET, SO_SNDBUF, (void *)&val, sizeof(val));
      if (ret < 0) 
          break;
      lenval = sizeof(val);
      ret = getsockopt(sk, SOL_SOCKET, SO_SNDBUF, (void *)&val,  &lenval);
      if(val < i*1024) 
          break;
    }
    return(1024*(i-5));
}




static  void    Usage(int argc, char *argv[])
{
    int i1, i2, i3, i4;

    /* Setting defaults */
    Num_bytes = 1000;
    Rate = 500;
    Num_pkts = 10000;
    sendPort = 8400;
    recvPort = 8400;
    Address = 0;
    Send_Flag = 0;
    Sping_Flag = 0;
    fileflag = 0;
    Reliable_Flag = 0;
    Forwarder_Flag = 0;
    strcpy(IP, "127.0.0.1");
    strcpy(MCAST_IP, "");
    Group_Address = 0;
    Realtime = 0;

    while( --argc > 0 ) {
        argv++;
    
        if( !strncmp( *argv, "-d", 2 ) ){
            sscanf(argv[1], "%d", (int*)&sendPort );
            argc--; argv++;
        }else if( !strncmp( *argv, "-rt", 3 ) ){
            Realtime=1;
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
        }else if( !strncmp( *argv, "-j", 2 ) ){
            sscanf(argv[1], "%s", MCAST_IP );
            sscanf(MCAST_IP ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
            Group_Address = ((i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4);
            argc--; argv++;
        }else if( !strncmp( *argv, "-s", 2 ) ){
            Send_Flag = 1;
        }else if( !strncmp( *argv, "-p", 2 ) ){
            Sping_Flag = 1;
        }else if( !strncmp( *argv, "-F", 2 ) ){
            Forwarder_Flag = 1;
        }else if( !strncmp( *argv, "-f", 2 ) ){
            sscanf(argv[1], "%s", filename );
            fileflag = 1;
            argc--; argv++;
        }else{
            printf( "Usage: sp_flooder\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
            "\t[-d <port number>] : to send packets on, default is 8400",
            "\t[-r <port number>] : to receive packets on, default is 8400",
            "\t[-a <address>    ] : address to send packets to",
            "\t[-b <size>       ] : size of the packets (in bytes)",
            "\t[-R <rate>       ] : sending rate (in 1000's of bits per sec)",
            "\t[-n <rounds>     ] : number of packets",
		    "\t[-j <mcast addr> ] : multicas address to join",
            "\t[-f <filename>   ] : file where to save statistics",
            "\t[-rt             ] : real-time, non-blocking i/o",
            "\t[-p              ] : run with sping for clock sync",
            "\t[-s              ] : sender flooder",
            "\t[-F              ] : forwarder only");
            exit( 0 );
        }
    }
    
    if(Num_bytes > MAX_PACKET_SIZE)
        Num_bytes = MAX_PACKET_SIZE;

    if(Num_bytes < sizeof(struct timeval) + 2*sizeof(int))
        Num_bytes = sizeof(struct timeval) + 2*sizeof(int);
    
}
