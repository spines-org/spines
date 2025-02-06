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
 * Copyright (c) 2003 - 2007 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
 *
 */


#ifndef SPINES_LIB_H
#define SPINES_LIB_H

#include <sys/types.h>
#include <sys/socket.h>

#include "util/arch.h"


#ifdef __cplusplus
extern "C" {
#endif

#ifdef ARCH_SPARC_SUNOS
#define socklen_t size_t
#endif

#ifdef ARCH_SPARC_SOLARIS
#define socklen_t size_t
#endif



#define     UDP_LINKS               0x00000000
#define     RELIABLE_LINKS          0x00000001
#define     SOFT_REALTIME_LINKS     0x00000002

#define     UDP_CONNECT             0x00000010


#define     SEND_GROUP              0x1000
#define     RECV_GROUP              0x2000
#define     SENDRECV_GROUP          0x3000

#define     AF_SPINES               AF_INET
#define     PF_SPINES               AF_SPINES

#define     SPINES_ADD_MEMBERSHIP   IP_ADD_MEMBERSHIP
#define     SPINES_DROP_MEMBERSHIP  IP_DROP_MEMBERSHIP
#define     SPINES_MULTICAST_LOOP   IP_MULTICAST_LOOP
#define     SPINES_ADD_NEIGHBOR     12 

#define     DEFAULT_SPINES_PORT     8100

#define     MAX_HOPS                32


typedef struct Lib_Client_d {
    int tcp_sk;
    int udp_sk;
    int type;
    int endianess_type;
    int sess_id;
    int rnd_num;
    int srv_addr;
    int srv_port;
    int protocol;
    int my_addr;
    int my_port;
    int connect_addr;
    int connect_port;
    int connect_flag;
    int rel_mcast_flag;
} Lib_Client;


typedef struct spines_trace_d {
    long address[MAX_HOPS];
    int distance[MAX_HOPS];
    int cost[MAX_HOPS];
} spines_trace;


int spines_flood_send(int sockfd, int address, int port, int rate, int size, int num_pkt);
int spines_flood_recv(int sockfd, char *filename, int namelen);

int  spines_init(const struct sockaddr *serv_addr);
int  spines_socket(int domain, int type, int protocol, 
		   const struct sockaddr *serv_addr);
int  spines_socket_internal(int domain, int type, int protocol, 
		   const struct sockaddr *serv_addr, int *udp_sk, int *tcp_sk);
void spines_close(int s);
int  spines_sendto(int s, const void *msg, size_t len, int flags, 
		   const struct sockaddr *to, socklen_t tolen);
int  spines_sendto_internal(int s, const void *msg, size_t len, int flags, 
			    const struct sockaddr *to, socklen_t tolen, 
			    int force_tcp);
int  spines_recvfrom(int s, void *buf, size_t len, int flags, 
		     struct sockaddr *from, socklen_t *fromlen);
int  spines_recvfrom_internal(int s, void *buf, size_t len, int flags, 
			      struct sockaddr *from, socklen_t *fromlen,
			      int force_tcp);
int  spines_bind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
int  spines_setsockopt(int s, int  level,  int  optname,  const  void  *optval,
		       socklen_t optlen);
int  spines_ioctl(int s, int  level,  int  optname,  const  void  *optval,
		       socklen_t optlen);
int  spines_connect(int  sockfd,  const  struct sockaddr *serv_addr, 
		    socklen_t addrlen);
int  spines_send(int s, const void *msg, size_t len, int flags);
int  spines_recv(int s, void *buf, size_t len, int flags);
int  spines_listen(int s, int backlog);
int  spines_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
 int spines_setlink(int sk, const struct sockaddr *addr, 
                    int bandwidth, int latency, float loss, float burst);
int  spines_get_client(int sk);
int  spines_ctrl_socket(const struct sockaddr *serv_addr);
int  spines_traceroute(const struct sockaddr *dest_addr, spines_trace *sp_trace,
	               const struct sockaddr *serv_addr);




#ifdef __cplusplus
}
#endif

#endif
