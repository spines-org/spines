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


#ifndef	ARCH_PC_WIN95

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#else

#include <winsock.h>

#endif

#include "util/arch.h"
#include "util/alarm.h"
#include "util/data_link.h"
#include "util/sp_events.h"
#include "net_types.h"
#include "session.h" 
#include "mutex.h"

#include "spines_lib.h"


#define START_UDP_PORT  20000
#define MAX_UDP_PORT    30000

#define MAX_SPINES_MSG   1430
#define MAX_APP_CLIENTS  1024

/* Local variables */ 

static struct sockaddr spines_addr;
static int             set_flag = 0;
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t data_mutex;
static Lib_Client      all_clients[MAX_APP_CLIENTS];
static int             Max_Client = 0;



void	Flip_udp_hdr( udp_header *udp_hdr )
{
    udp_hdr->source	  = Flip_int32( udp_hdr->source );
    udp_hdr->dest	  = Flip_int32( udp_hdr->dest );
    udp_hdr->source_port  = Flip_int16( udp_hdr->source_port );
    udp_hdr->dest_port	  = Flip_int16( udp_hdr->dest_port );
    udp_hdr->len	  = Flip_int16( udp_hdr->len );
    udp_hdr->seq_no	  = Flip_int16( udp_hdr->seq_no );
    udp_hdr->sess_id	  = Flip_int16( udp_hdr->sess_id );
}



/***********************************************************/
/* int spines_init(const struct sockaddr *serv_addr)       */
/*                                                         */
/* Sets the address of the Spines overlay node             */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* srvr: Address of the Spines node                        */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if the initialization succeded                 */
/*       -1 if the Spines address was already set          */
/*                                                         */
/***********************************************************/

int spines_init(const struct sockaddr *serv_addr)
{
    int i;
    struct sockaddr_in my_addr, *tmp_addr;
    char   machine_name[256];
    struct hostent  *host_ptr;
  

    if(Mutex_Trylock(&init_mutex) == 0 && set_flag == 0) {

	if(serv_addr == NULL) {
	    gethostname(machine_name,sizeof(machine_name)); 
	    host_ptr = gethostbyname(machine_name);
	    
	    if(host_ptr == NULL) {
		printf("could not get my ip address (my name is %s)\n",
		       machine_name );
		return(-1);
	    }
	    if(host_ptr->h_addrtype != AF_INET) {
		printf("Sorry, cannot handle addr types other than IPv4\n");
		return(-1);
	    }
	    
	    if(host_ptr->h_length != 4) {
		printf("Bad IPv4 address length\n");
		return(-1);
	    }
	    memcpy(&my_addr.sin_addr, host_ptr->h_addr, sizeof(struct in_addr));
	    my_addr.sin_port = htons(DEFAULT_SPINES_PORT) ;
	    tmp_addr = &my_addr;
	}
	else {
	    tmp_addr = (struct sockaddr_in*)serv_addr;
	}
	

	Mutex_Init(&data_mutex);
	if(set_flag == 0) {
	    memcpy(&spines_addr, tmp_addr, sizeof(struct sockaddr));
	    set_flag = 1;
	    for(i=0; i<MAX_APP_CLIENTS; i++) {
		all_clients[i].udp_sk = -1; 
	    }
	    return(1);
	}
    }
    return(-1);
}




/***********************************************************/
/* int spines_socket(int domain, int type,                 */
/*                   int protocol,                         */
/*                   const struct sockaddr *serv_addr)     */
/*                                                         */
/* Creates a Spines socket                                 */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* domain: communication domain (PF_SPINES)                */
/* type: type of socket (SOCK_DGRAM or SOCK_STREAM)        */
/* protocol: protocol used on the overlay links            */
/* srvr: Address of the Spines node                        */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the socket the application can use to             */
/*       send/receive data                                 */
/*       -1 in case of an error                            */
/*                                                         */
/***********************************************************/

int  spines_socket(int domain, int type, int protocol, 
		   const struct sockaddr *serv_addr)
{
    struct sockaddr_in host, my_host;
    udp_header *u_hdr;
    char buf[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *msg_type;
    int ret, sk, u_sk, client;
    int val = 1;
    int32 *flag_var, *rnd_var, *port_var, *addr_var;
    struct hostent  *host_ptr;
    char   machine_name[256];
    int address, port, udp_port;
    int rnd_num, sess_id;
    struct timeval tv;
    struct timezone tz;
    int link_prot, connect_flag;
    int tot_bytes, recv_bytes;
    int32 endianess_type;
    


    if((type != SOCK_DGRAM)&&(type != SOCK_STREAM)) {
	Alarm(PRINT, "spines_socket(): Unknown socket type: %d\n", type);
	return(-1);
    }

    spines_init(serv_addr);

    link_prot = protocol & 0x0000000f;
    connect_flag = protocol & 0x00000010;

    gettimeofday(&tv, &tz);
    srand(tv.tv_sec+tv.tv_usec);
    rnd_num = rand();
    udp_port = -1;


    total_len = (int32*)(buf);
    u_hdr = (udp_header*)(buf+sizeof(int32));
    msg_type = (int32*)(buf+sizeof(int32)+sizeof(udp_header));
    flag_var = (int32*)(buf+sizeof(int32)+sizeof(udp_header)+sizeof(int32));
    rnd_var = (int32*)(buf+sizeof(int32)+sizeof(udp_header)+2*sizeof(int32));
    addr_var = (int32*)(buf+sizeof(int32)+sizeof(udp_header)+3*sizeof(int32));
    port_var = (int32*)(buf+sizeof(int32)+sizeof(udp_header)+4*sizeof(int32));


    sk = socket(AF_INET, SOCK_STREAM, 0);        
    if (sk < 0) {
	Alarm(PRINT, "spines_socket(): Can not initiate socket...");
	return(-1);
    }

    if (setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val))){
	Alarm(PRINT, "spines_socket(): Failed to set socket option TCP_NODELAY\n");
	close(sk);
	return(-1);
    }
    
    u_sk = sk;

    if(serv_addr != NULL) {
	memcpy(&host, serv_addr, sizeof(struct sockaddr_in));
    }
    else if(set_flag == 1) {
	memcpy(&host, &spines_addr, sizeof(struct sockaddr_in));
    }
    else {
	/* No info about where the Spines server is. Try local machine */
	gethostname(machine_name,sizeof(machine_name)); 
	host_ptr = gethostbyname(machine_name);
	
	if(host_ptr == NULL) {
	    Alarm(PRINT, "spines_socket(): could not get my default interface (my name is %s)\n",
		   machine_name );
	    return(-1);
	}
	if(host_ptr->h_addrtype != AF_INET) {
	    Alarm(PRINT, "spines_socket(): Sorry, cannot handle addr types other than IPv4\n");
	    return(-1);
	}
	
	if(host_ptr->h_length != 4) {
	    Alarm(PRINT, "spines_socket(): Bad IPv4 address length\n");
	    return(-1);
	}
	memcpy(&host.sin_addr.s_addr, host_ptr->h_addr, sizeof(struct in_addr));

	host.sin_port   = htons((int16)DEFAULT_SPINES_PORT);
    }
    address = ntohl(host.sin_addr.s_addr);
    port = ntohs(host.sin_port);
    host.sin_family = AF_INET;
    host.sin_port   = htons((int16)(ntohs(host.sin_port)+SESS_PORT));

    ret = connect(sk, (struct sockaddr *)&host, sizeof(host));
    if( ret < 0) {
	Alarm(PRINT, "1 spines_socket(): Can not initiate connection to Spines...\n");
	return(-1);
    }


    if((type == SOCK_DGRAM)&&(connect_flag == UDP_CONNECT)) {
	u_sk = socket(AF_INET, SOCK_DGRAM, 0);
	if (u_sk < 0) {
	    Alarm(PRINT, "spines_socket(): Can not initiate socket...");
	    close(sk);
	    return(-1);
	}


	gethostname(machine_name,sizeof(machine_name)); 
	host_ptr = gethostbyname(machine_name);
	
	if(host_ptr == NULL) {
	    Alarm(PRINT, "spines_socket(): could not get my default interface (my name is %s)\n",
		   machine_name );
	    return(-1);
	}
	if(host_ptr->h_addrtype != AF_INET) {
	    Alarm(PRINT, "spines_socket(): Sorry, cannot handle addr types other than IPv4\n");
	    return(-1);
	}
	
	if(host_ptr->h_length != 4) {
	    Alarm(PRINT, "spines_socket(): Bad IPv4 address length\n");
	    return(-1);
	}
	memcpy(&my_host.sin_addr.s_addr, host_ptr->h_addr, sizeof(struct in_addr));

	
	/* memcpy(&my_host.sin_addr.s_addr, &host.sin_addr.s_addr, sizeof(struct in_addr));*/

	my_host.sin_family = AF_INET;
	udp_port = START_UDP_PORT;
	while(udp_port < MAX_UDP_PORT) {
	    my_host.sin_port = htons(udp_port);
	    if(bind(u_sk, (struct sockaddr *)&my_host, sizeof(my_host) ) == 0 ) {
		break;
	    }
	    udp_port++;
	}

	if(udp_port == MAX_UDP_PORT) {
	    Alarm(PRINT, "spines_socket(): Can not bind socket...");
	    close(sk);
	    close(u_sk);
	    return(-1);
	}
    }

    *flag_var = link_prot;
    *rnd_var = rnd_num;
    if((type == SOCK_DGRAM)&&(connect_flag == UDP_CONNECT)) {
	*addr_var = ntohl(my_host.sin_addr.s_addr);
	*port_var = udp_port;
    }
    else {
	*addr_var = -1;
	*port_var = -1;
    }
    *total_len = (int32)(sizeof(udp_header) + 5*sizeof(int32));

    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *msg_type = LINKS_TYPE_MSG;


    /* Send the endianess */
    endianess_type = Set_endian(0);

    tot_bytes = 0;
    while(tot_bytes < sizeof(int32)) {
	ret = send(sk, ((char*)(&endianess_type))+tot_bytes, sizeof(int32)-tot_bytes, 0);
	tot_bytes += ret;
    }   
    if(tot_bytes != sizeof(int32)) {
	Alarm(PRINT, "2 spines_socket(): Can not initiate connection to Spines...\n");
	close(sk);
	if(type == SOCK_DGRAM) {
	    close(u_sk);
	}
	return(-1);
    }


    /* Send the first packet */
    tot_bytes = 0;
    while(tot_bytes < *total_len+sizeof(int32)) {
	ret = send(sk, buf+tot_bytes, *total_len+sizeof(int32)-tot_bytes, 0);
	tot_bytes += ret;
    }   
    if(tot_bytes != sizeof(udp_header)+sizeof(int32)+5*sizeof(int32)) {
	Alarm(PRINT, "2 spines_socket(): Can not initiate connection to Spines...\n");
	close(sk);
	if(type == SOCK_DGRAM) {
	    close(u_sk);
	}
	return(-1);
    }

    /* Get the endianess */
    recv_bytes = 0;
    while(recv_bytes < sizeof(int32)) {
	ret = recv(sk, ((char*)&endianess_type)+recv_bytes, sizeof(int32)-recv_bytes, 0);
	if(ret < 0) {
	    Alarm(PRINT, "spines_socket(): Can not initiate connection to Spines...\n");
	    close(sk);
	    if(type == SOCK_DGRAM) {
		close(u_sk);
	    }
	    return(-1);	    
	}
	recv_bytes += ret;
    }

    /* Create the client */
    Mutex_Lock(&data_mutex);
    for(client=0; client<MAX_APP_CLIENTS; client++) {
	if(all_clients[client].udp_sk == -1) {
	    break;
	}
    }
    if(client == MAX_APP_CLIENTS) {
	Alarm(PRINT, "spines_socket(): Too many open sockets\n");
	Mutex_Unlock(&data_mutex);
	close(sk);
	if(type == SOCK_DGRAM) {
	    close(u_sk);
	}
	return(-1);
    }
    if(client == Max_Client) {
	Max_Client++;
    }
    all_clients[client].type = 0;
    all_clients[client].connect_flag = 0;
    all_clients[client].endianess_type = endianess_type;
    all_clients[client].tcp_sk = sk;
    all_clients[client].udp_sk = sk;
    Mutex_Unlock(&data_mutex);

    /* Get the session ID */
    ret = spines_recvfrom(sk, buf, sizeof(buf), 1, NULL, NULL);

    if(ret <= 0) {
	close(sk);
	if(type == SOCK_DGRAM) {
	    close(u_sk);
	}
	return(-1);
    }
    sess_id = *(int*)buf;


    /* Update the client */
    Mutex_Lock(&data_mutex);
    if(type == SOCK_DGRAM) {
	all_clients[client].udp_sk = u_sk;
	all_clients[client].my_addr = ntohl(my_host.sin_addr.s_addr);    
	all_clients[client].my_port = udp_port;
	all_clients[client].connect_flag = connect_flag;
    }    
    else {
	all_clients[client].udp_sk = sk;
	all_clients[client].connect_flag = 0;
    
    }
    all_clients[client].type = type;
    all_clients[client].rnd_num = rnd_num;
    all_clients[client].sess_id = sess_id;
    all_clients[client].srv_addr = address;
    all_clients[client].srv_port = port;
    all_clients[client].protocol = protocol;
    all_clients[client].connect_addr = -1;
    all_clients[client].connect_port = -1;   
    all_clients[client].rel_mcast_flag = 0;       
    Mutex_Unlock(&data_mutex);


    if((type == SOCK_DGRAM)&&(connect_flag == UDP_CONNECT)) {
	return(u_sk);
    }

    return(sk);
}


/***********************************************************/
/* void spines_close(int sk)                               */
/*                                                         */
/* Closes a Spines socket                                  */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk: the socket defining the connection to Spines        */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void spines_close(int s)
{
    int client, type, tcp_sk, connect_flag;


    Mutex_Lock(&data_mutex);
    client = spines_get_client(s);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return;
    }
    type = all_clients[client].type;
    tcp_sk = all_clients[client].tcp_sk;
    connect_flag = all_clients[client].connect_flag;
    all_clients[client].udp_sk = -1;
    if(client == Max_Client-1) {
	Max_Client--;
    }
    Mutex_Unlock(&data_mutex);

    close(s);
    if((type == SOCK_DGRAM)&&(connect_flag == UDP_CONNECT)) {
	close(tcp_sk);
    }
}


/***********************************************************/
/* int spines_sendto(int s, const void *msg, size_t len,   */
/*                   int flags, const struct sockaddr *to, */
/*                   socklen_t tolen);                     */
/*                                                         */
/* Sends best effort data through the Spines network       */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* s:       the Spines socket                              */
/* msg:     a pointer to the message                       */
/* len:     length of the message                          */
/* flags:   not used yet                                   */
/* to:      the target of the message                      */
/* tolen:   length of the target                           */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the number of bytes sent (or -1 if an error)      */
/*                                                         */
/***********************************************************/

int spines_sendto(int s, const void *msg, size_t len,  
		  int flags, const struct sockaddr *to, 
		  socklen_t tolen)
{
    int ret;

    ret = spines_sendto_internal(s, msg, len,  flags, to, tolen, 0);
    return(ret);
}


int spines_sendto_internal(int s, const void *msg, size_t len,  
			   int flags, const struct sockaddr *to, 
			   socklen_t tolen, int force_tcp)
{
    udp_header u_hdr;
    sys_scatter scat;
    int port, address, ret;
    int sess_id, rnd_num;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    udp_header *hdr;
    int client, type, tcp_sk, my_addr, my_port, srv_addr, srv_port, connect_flag, rel_mcast_flag;
    int tot_bytes;

    address = ntohl(((struct sockaddr_in*)to)->sin_addr.s_addr);
    port = ntohs(((struct sockaddr_in*)to)->sin_port);

    if(port == 0) {
	Alarm(PRINT, "spines_sendto(): cannot send to port 0\n");
	return(-1);
    }

    Mutex_Lock(&data_mutex);
    client = spines_get_client(s);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    type = all_clients[client].type;
    tcp_sk = all_clients[client].tcp_sk;
    sess_id = all_clients[client].sess_id;
    rnd_num = all_clients[client].rnd_num;
    my_addr = all_clients[client].my_addr;
    my_port = all_clients[client].my_port;
    srv_addr = all_clients[client].srv_addr;
    srv_port = all_clients[client].srv_port;
    connect_flag = all_clients[client].connect_flag;
    rel_mcast_flag = all_clients[client].rel_mcast_flag;
    Mutex_Unlock(&data_mutex);

    if((type == SOCK_STREAM)&&(force_tcp != 1)) {
	return(spines_send(tcp_sk, msg, len, flags));
    }
    if((force_tcp == 1)||(connect_flag != UDP_CONNECT)) {
	/*Force TCP*/
	total_len = (int32*)(pkt);
	hdr = (udp_header*)(pkt+sizeof(int32));
	
	hdr->source = 0;
	hdr->dest   = (int32)address;
	hdr->dest_port   = port;
	hdr->len = len;

	if(rel_mcast_flag == 0) {
	    *total_len = len + sizeof(udp_header);
	    ret = send(tcp_sk, pkt, 
		       sizeof(int32)+sizeof(udp_header), 0); 
	    if(ret != sizeof(int32)+sizeof(udp_header)) {
		Alarm(PRINT, "spines_sendto(): error sending header: %d\n", ret);	    
		return(-1);
	    }
	}
	    
	tot_bytes = 0;
	while(tot_bytes < len) {
	    ret = send(tcp_sk, msg, len, 0);
	    tot_bytes += ret;
	}
	if(tot_bytes != len) {
	    Alarm(PRINT, "spines_sendto(): error sending: %d\n", ret);	    
	    return(-1);
	}
	return(len);
    }
    else {
	/* Use UDP communication */
	u_hdr.source = my_addr;
	u_hdr.source_port = my_port;    
	u_hdr.dest   = (int32)address;
	u_hdr.dest_port   = port;
	u_hdr.len = len;

	scat.num_elements = 4;
	scat.elements[0].len = sizeof(int);
	scat.elements[0].buf = (char*)&sess_id;
	scat.elements[1].len = sizeof(int);
	scat.elements[1].buf = (char*)&rnd_num;
	scat.elements[2].len = sizeof(udp_header);
	scat.elements[2].buf = (char*)&u_hdr;
	scat.elements[3].len = len;
	scat.elements[3].buf = (char*)msg;
	ret = DL_send(s, srv_addr, srv_port+SESS_UDP_PORT, &scat);
	if(ret != 2*sizeof(int32)+sizeof(udp_header)+len) {
	    return(-1);
	}
	return(len);
    }
}





/***********************************************************/
/* int spines_recvfrom(int s, void *buf, size_t len,       */
/*                     int flags, struct sockaddr *from,   */
/*                     socklen_t *fromlen);                */
/*                                                         */
/* Receives data from the Spines network                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* s:       the Spines socket                              */
/* buff:    a buffer to receive data                       */
/* len:     length of the buffer                           */
/* flags:   not used yet                                   */
/* from:    a buffer to get the sender of the message      */
/* fromlen: length of the sender buffer                    */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the number of bytes received                      */
/*       -1 if an error                                    */
/*                                                         */
/***********************************************************/

int  spines_recvfrom(int s, void *buf, size_t len, int flags, 
		     struct sockaddr *from, socklen_t *fromlen) 
{
    int ret;

    ret = spines_recvfrom_internal(s, buf, len, flags, from, fromlen, 0);
    return(ret);
}


int  spines_recvfrom_internal(int s, void *buf, size_t len, int flags, 
			      struct sockaddr *from, socklen_t *fromlen,
			      int force_tcp) 
{
    int received_bytes;
    sys_scatter scat;
    udp_header u_hdr;
    int32 msg_len;
    udp_header *hdr;
    char pkt[MAX_PACKET_SIZE];
    char *pnt;
    int32 *pkt_len;
    int total_bytes, r_add_size;
    int client, type = 0, connect_flag = 0;
    int32 endianess_type;

    endianess_type = Set_endian(0);

    Mutex_Lock(&data_mutex);
    client = spines_get_client(s);
    if(client != -1) {
	type = all_clients[client].type;
	connect_flag = all_clients[client].connect_flag;
	endianess_type = all_clients[client].endianess_type;
    }
    Mutex_Unlock(&data_mutex);

    if((connect_flag == UDP_CONNECT)&&(force_tcp != 1)) {
	/* USe UDP communication */

	scat.num_elements = 3;
	scat.elements[0].len = sizeof(int32);
	scat.elements[0].buf = (char*)&msg_len;
	scat.elements[1].len = sizeof(udp_header);
	scat.elements[1].buf = (char*)&u_hdr;
	scat.elements[2].len = len;
	scat.elements[2].buf = buf;
	
      	received_bytes = DL_recv(s, &scat);
	if(received_bytes <= 0) {
	    return(-1);
	}
	
	if(!Same_endian(endianess_type)) {
	    msg_len = Flip_int32(msg_len);
	    Flip_udp_hdr(&u_hdr);
	}

	if(u_hdr.dest == -1) {
	    return(-1);
	}

	if(from != NULL) {
	    if(*fromlen < sizeof(struct sockaddr_in)) {
		Alarm(PRINT, "spines_recvfrom(): fromlen too small\n");
		return(-1);
	    }
	    ((struct sockaddr_in*)from)->sin_port = htons((short)u_hdr.source_port);
	    ((struct sockaddr_in*)from)->sin_addr.s_addr = htonl(u_hdr.source);
	    *fromlen = sizeof(struct sockaddr_in);
	}
	return(received_bytes - sizeof(int32) - sizeof(udp_header));
    }
    else {
	/* Force TCP */
	if((type == SOCK_STREAM)&&(force_tcp != 1)) {
	    return(spines_recv(s, buf, len, flags));
	}
	
	pkt_len = (int32*)(pkt);
	hdr = (udp_header*)(pkt+sizeof(int32));

	total_bytes = 0;
	while(total_bytes < sizeof(int32)+sizeof(udp_header)) { 
	    received_bytes = recv(s, pkt+total_bytes, 
				  sizeof(int32)+sizeof(udp_header)-total_bytes, 0);
	    if(received_bytes <= 0)
		return(-1);
	    total_bytes += received_bytes;
	    if(total_bytes > sizeof(int32)+sizeof(udp_header)) {
		Alarm(PRINT, "spines_recvfrom(): socket error\n");
		return(-1);
	    }
	}

	if(!Same_endian(endianess_type)) {
	    *pkt_len = Flip_int32(*pkt_len);
	    Flip_udp_hdr(hdr);
	}


	if(*pkt_len - (int)sizeof(udp_header) > len) {
	    Alarm(PRINT, "spines_recvfrom(): message too big: %d :: %d\n", *pkt_len, len);
	    return(-1);
	}
	
	r_add_size = 0;
	total_bytes = 0;    
	while(total_bytes < *pkt_len - (int)sizeof(udp_header)-r_add_size) { 
	    received_bytes = recv(s, buf+total_bytes, 
				  *pkt_len-(int)sizeof(udp_header)-r_add_size-total_bytes, 0);
	    if(received_bytes <= 0)
		return(-1);
	    total_bytes += received_bytes;
	    if(total_bytes > *pkt_len - (int)sizeof(udp_header)) {
		Alarm(PRINT, "spines_recvfrom(): socket error\n");
		return(-1);
	    }
	}

	if(from != NULL) {
	    if(*fromlen < sizeof(struct sockaddr_in)) {
		Alarm(PRINT, "spines_recvfrom(): fromlen too small\n");
		return(-1);
	    }
	    ((struct sockaddr_in*)from)->sin_port = htons((short)hdr->source_port);
	    ((struct sockaddr_in*)from)->sin_addr.s_addr = htonl(hdr->source);
	    *fromlen = sizeof(struct sockaddr_in);
	}
 
	return(total_bytes);
    }
}




/***********************************************************/
/* int spines_bind(int sockfd, struct sockaddr *my_addr,   */
/*                 socklen_t addrlen)                      */
/*                                                         */
/* Assigns a Spines virtual address to a Spines socket     */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sockfd:  the Spines socket                              */
/* my_addr: the Spines virtual address to be assigned      */
/* addrlen: length of the address                          */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  0 if success                                     */
/*       -1 if error                                       */
/*                                                         */
/***********************************************************/

int spines_bind(int sockfd, struct sockaddr *my_addr,  
		socklen_t addrlen)
{
    udp_header *u_hdr, *cmd;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *type;
    int port, ret;
    int client, my_type, tcp_sk, sk, connect_flag;
    int tot_bytes;


    Mutex_Lock(&data_mutex);
    client = spines_get_client(sockfd);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    my_type = all_clients[client].type;
    tcp_sk = all_clients[client].tcp_sk;
    connect_flag = all_clients[client].connect_flag;
    Mutex_Unlock(&data_mutex);

    if(addrlen < sizeof(struct sockaddr_in)) {
	Alarm(PRINT, "spines_bind(): invalid address\n");
	return(-1);	
    }
    port = ntohs(((struct sockaddr_in*)my_addr)->sin_port);

    if(port == 0) {
	Alarm(PRINT, "spines_bind(): cannot bind on port 0\n");
	return(-1);
    }

    if(my_type == SOCK_STREAM) {
	sk = sockfd;
    }
    else {
	sk = tcp_sk;
    }


    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    cmd = (udp_header*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));


    *total_len = (int32)(2*sizeof(udp_header) + sizeof(int32));

    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = BIND_TYPE_MSG;

    cmd->source = 0;
    cmd->dest   = 0;
    cmd->dest_port   = port;
    cmd->len    = 0;
   
    tot_bytes = 0;
    while(tot_bytes < *total_len+sizeof(int32)) {
	ret = send(sk, pkt+tot_bytes, *total_len+sizeof(int32)-tot_bytes, 0);
	tot_bytes += ret;
    }
    if(tot_bytes != 2*sizeof(udp_header)+2*sizeof(int32))
	return(-1);


    if((my_type == SOCK_DGRAM)&&(connect_flag == UDP_CONNECT)) {
	ret = spines_recvfrom(sk, pkt, sizeof(pkt), 1, NULL, NULL);
	if(ret <= 0) {
	    return(-1);
	}
    }

    return(0);
}



/***********************************************************/
/* int spines_setsockopt(int s, int level, int optname,    */
/*                       const void *optval,               */
/*                       socklen_t optlen)                 */ 
/*                                                         */
/* Sets the options for a Spines socket. Currently only    */
/* used for multicast join and leave (ADD/DROP MEMBERSHIP) */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* s:         the socket defining the connection to Spines */
/* level:     not currently used                           */
/* optname:   SPINES_ADD_MEMBERSHIP or                     */
/*            SPINES_DROP_MEMBERSHIP                       */
/* optval:    a struct ip_mreq containing the multicast    */
/*            group                                        */
/* optlen:    the length of the optval parameter           */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  0 if join was ok                                 */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

int  spines_setsockopt(int s, int  level,  int  optname,  
		       const  void  *optval, socklen_t optlen)
{
    udp_header *u_hdr, *cmd;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *type;
    int sk, ret;
    int client, my_type, tcp_sk;

    if( optname != SPINES_ADD_MEMBERSHIP &&
        optname != SPINES_DROP_MEMBERSHIP &&
        optname != SPINES_MULTICAST_LOOP &&
        optname != SPINES_ADD_NEIGHBOR       ) {
	return(-1);
    }

    Mutex_Lock(&data_mutex);
    client = spines_get_client(s);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    my_type = all_clients[client].type;
    tcp_sk = all_clients[client].tcp_sk;
    Mutex_Unlock(&data_mutex);

    if(my_type == SOCK_STREAM) {
	/* sk = tcp_sk; */
	Alarm(PRINT, "spines_setsockopt(): Multicast for STREAM sockets not supported\n");
	return(-1);	

    }
    else {
	sk = tcp_sk;
    }

    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    cmd = (udp_header*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));

    *total_len = (int32)(2*sizeof(udp_header) + sizeof(int32));
        
    u_hdr->source  = 0;
    u_hdr->dest    = 0;
    u_hdr->len     = 0;

    cmd->source    = 0;
    cmd->dest      = 0;
    cmd->dest_port = 0;
    cmd->len       = 0;
    
    if(optname == SPINES_ADD_MEMBERSHIP) {
	*type = JOIN_TYPE_MSG; 
	if (optlen < sizeof(struct ip_mreq)) {
	    return(-1);
	}
	cmd->dest = ntohl(((struct ip_mreq*)optval)->imr_multiaddr.s_addr);
    }
    else if (optname == SPINES_DROP_MEMBERSHIP) {
	*type = LEAVE_TYPE_MSG;
	if (optlen < sizeof(struct ip_mreq)) {
	    return(-1);
	}
	cmd->dest = ntohl(((struct ip_mreq*)optval)->imr_multiaddr.s_addr);
    }
    else if (optname == SPINES_MULTICAST_LOOP) {
	*type = LOOP_TYPE_MSG;
	if (optlen < sizeof(unsigned char)) {
	    return(-1);
	}
	cmd->dest = *((char*)(optval));
    }
    else if (optname == SPINES_ADD_NEIGHBOR) {
	*type = ADD_NEIGHBOR_MSG; 
	if(optlen < sizeof(struct sockaddr_in)) 
	    return(-1); 
	cmd->dest = ntohl(((struct sockaddr_in*)optval)->sin_addr.s_addr);
    } else { 
	Alarm(PRINT, "spines_setsockopt(): Bad Option\n");
	return(-1);
    }

    ret = send(sk, pkt, *total_len+sizeof(int32), 0);
    if(ret != 2*sizeof(udp_header)+2*sizeof(int32))
	return(-1);

    return(0);
}

/***********************************************************/
/* int spines_ioctl(int s, int level, int optname,    */
/*                       const void *optval,               */
/*                       socklen_t optlen)                 */ 
/*                                                         */
/* Controls spines router.                                 */
/* Currently only allows to add a neighbor                 */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* s:         the socket defining the connection to Spines */
/* level:     not currently used                           */
/* optname:   SPINES_ADD_NEIGHBOR                          */
/*            SPINES_SET_METRIC                            */
/* optval:    a struct ip_mreq containing the multicast    */
/*            group                                        */
/* optlen:    the length of the optval parameter           */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  0 if join was ok                                 */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

int  spines_ioctl(int s, int  level,  int  optname,  
		       const  void  *optval, socklen_t optlen)
{
    return spines_setsockopt(s, level, optname, optval, optlen);
}




/***********************************************************/
/* int spines_connect(int sockfd,                          */
/*                    const struct sockaddr *serv_addr,    */
/*                    socklen_t addrlen)                   */
/*                                                         */
/* Connects to another Spines socket at an address         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sockdef:   the Spines socket                            */
/* serv_addr: the address to connect to                    */
/* addrlen:   The length of the address                    */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  0 if connect was ok                              */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

/*int spines_connect(int sk, int address, int port)*/
int  spines_connect(int sockfd, const struct sockaddr *serv_addr, 
		    socklen_t addrlen)
{
    udp_header *u_hdr, *cmd;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *type;
    int ret, recv_bytes;
    char buf[200];
    int address, port;
    int client, my_type;


    if(addrlen < sizeof(struct sockaddr_in))
	return(-1);

    address = ntohl(((struct sockaddr_in*)serv_addr)->sin_addr.s_addr);
    port = ntohs(((struct sockaddr_in*)serv_addr)->sin_port);

    Mutex_Lock(&data_mutex);
    client = spines_get_client(sockfd);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    my_type = all_clients[client].type;
    if(my_type == SOCK_DGRAM) {
	all_clients[client].connect_addr = address;
	all_clients[client].connect_port = port;
	Mutex_Unlock(&data_mutex);
	return(0);
    }
    Mutex_Unlock(&data_mutex);


    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    cmd = (udp_header*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));


    *total_len = (int32)(2*sizeof(udp_header) + sizeof(int32));
        
    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = CONNECT_TYPE_MSG;

    cmd->source = 0;
    cmd->dest   = address;
    cmd->dest_port   = port;
    cmd->len    = 0;
    
    ret = send(sockfd, pkt, *total_len+sizeof(int32), 0);

    if(ret != 2*sizeof(udp_header)+2*sizeof(int32))
	return(-1);

    /*    ret = spines_recvfrom_internal(sockfd, buf, sizeof(buf), 0, NULL, NULL, 1);*/

    recv_bytes = 0;
    while(recv_bytes < sizeof(ses_hello_packet)) {
	ret = spines_recv(sockfd, buf, sizeof(ses_hello_packet) - recv_bytes, 0); 
	if(ret <= 0)
	    return(-1);
	recv_bytes += ret;
    }

    
    return(0);
}




/***********************************************************/
/* int spines_send(int s, const void *msg, size_t len,     */
/*                 int flags)                              */
/*                                                         */
/* Sends data through the Spines network                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* s:       the Spines socket                              */
/* msg:     a pointer to the message                       */
/* len:     length of the message                          */
/* flags:   not used yet                                   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the number of bytes sent (or -1 if an error)      */
/*                                                         */
/***********************************************************/

int  spines_send(int s, const void *msg, size_t len, int flags)
{
    udp_header *u_hdr;
    char pkt[MAX_PACKET_SIZE];
    rel_udp_pkt_add *r_add;
    int32 *total_len;
    struct sockaddr_in host;
    int ret;
    int client, type, connect_addr, connect_port;



    Mutex_Lock(&data_mutex);
    client = spines_get_client(s);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    type = all_clients[client].type;
    connect_addr = all_clients[client].connect_addr;
    connect_port = all_clients[client].connect_port;    
    Mutex_Unlock(&data_mutex);

    if(type == SOCK_DGRAM) {
	if(connect_port == -1) {
	    Alarm(PRINT, "DGRAM socket not connected\n");
	    return(-1);
	}
	host.sin_family = AF_INET;
	host.sin_addr.s_addr = htonl(connect_addr);
	host.sin_port   = htons(connect_port);
	return(spines_sendto(s, msg, len, flags, 
			     (struct sockaddr*)&host, sizeof(struct sockaddr)));
    }


    /* Check for maximum length */
    /*   */    
    /*   */

    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    r_add = (rel_udp_pkt_add*)(pkt+sizeof(int32)+sizeof(udp_header));


    *total_len = len + sizeof(udp_header) + sizeof(rel_udp_pkt_add);
     
    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->dest_port   = 0;
    u_hdr->len    = len + sizeof(rel_udp_pkt_add);

    r_add->type = Set_endian(0);
    r_add->data_len = len;
    r_add->ack_len = 0;

    ret = send(s, pkt, sizeof(udp_header)+sizeof(rel_udp_pkt_add)+sizeof(int32), 0);
    if(ret != sizeof(udp_header)+sizeof(rel_udp_pkt_add)+sizeof(int32)) {
	return(-1);
    }
    
    ret = send(s, msg, len, 0);
    if(ret != len) {
	return(-1);
    }

    
    return(ret);
}




/***********************************************************/
/* int spines_recv(int s, void *buf, size_t len, int flags)*/ 
/*                                                         */
/* Receives data from the Spines network                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* s:       the Spines socket                              */
/* buff:    a buffer to receive into                       */
/* len:     length of the buffer                           */
/* flags:   not used yet                                   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the number of bytes received                      */
/*       -1 if an error                                    */
/*                                                         */
/***********************************************************/

int  spines_recv(int s, void *buf, size_t len, int flags)
{
    int received_bytes;
    int client, type;

    Mutex_Lock(&data_mutex);
    client = spines_get_client(s);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    type = all_clients[client].type;
    Mutex_Unlock(&data_mutex);

    if(type == SOCK_DGRAM) {
	return(spines_recvfrom(s, buf, len, flags, NULL, NULL));
    }

    received_bytes = recv(s, buf, len, 0);

    return(received_bytes);
}




/***********************************************************/
/* int spines_listen(int s, int backlog)                   */ 
/*                                                         */
/* Listens on a port of the Spines network                 */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* s:       the socket defining the connection to Spines   */
/* backlog: not used yet                                   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  0 if listen was ok                               */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

int spines_listen(int s, int backlog)
{
    udp_header *u_hdr, *cmd;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *type;
    int ret;
    int client, my_type;

    Mutex_Lock(&data_mutex);
    client = spines_get_client(s);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    my_type = all_clients[client].type;
    Mutex_Unlock(&data_mutex);

    if(my_type == SOCK_DGRAM) {
	Alarm(PRINT, "DATAGRAM socket. spines_listen() not supported\n");
	return(-1);
    }


    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    cmd = (udp_header*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));


    *total_len = (int32)(2*sizeof(udp_header) + sizeof(int32));
        
    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = LISTEN_TYPE_MSG;

    cmd->source = 0;
    cmd->dest   = 0;
    cmd->dest_port   = 0;
    cmd->len    = 0;
    
    ret = send(s, pkt, *total_len+sizeof(int32), 0);
    
    if(ret == 2*sizeof(udp_header)+2*sizeof(int32))
	return(0);
    else
	return(-1);
}




/***********************************************************/
/* int spines_accept(int s, struct sockaddr *addr,         */
/*                   socklen_t *addrlen)                   */
/*                                                         */
/* Accepts a conection with another Spines socket          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* s :           the Spines socket                         */
/* addr:         not used yet                              */
/* addrlen       not used yet                              */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) a socket for a new session                        */
/*       -1 error                                          */
/*                                                         */
/***********************************************************/

/*int spines_accept(int sk, int port, int address, int *flags)*/
int  spines_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    char pkt[MAX_PACKET_SIZE];
    udp_header *u_hdr, *cmd;
    int32 *total_len;
    int32 *type;
    char *buf;
    int32 addrtmp, porttmp;
    int ret, data_size, recv_bytes;
    int new_sk;
    struct sockaddr_in old_addr, otherside_addr;
    int client, my_type, protocol;
    socklen_t lenaddr;



    Mutex_Lock(&data_mutex);
    client = spines_get_client(s);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    my_type = all_clients[client].type;
    protocol = all_clients[client].protocol;
    old_addr.sin_addr.s_addr = htonl(all_clients[client].srv_addr);
    old_addr.sin_port = htons(all_clients[client].srv_port);
    Mutex_Unlock(&data_mutex);

    if(my_type == SOCK_DGRAM) {
	Alarm(PRINT, "DATAGRAM socket. spines_accept() not supported\n");
	return(-1);
    }


    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    cmd = (udp_header*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));
    buf = pkt+sizeof(int32)+2*sizeof(udp_header)+sizeof(int32);

    lenaddr = sizeof(struct sockaddr_in);

    ret = spines_recvfrom_internal(s, buf, sizeof(pkt)-(buf-pkt), 0,
				   (struct sockaddr*)(&otherside_addr), 
				   &lenaddr, 1);



    if(ret <= 0)
	return(-1);
    
    data_size = ret;
    
    addrtmp = ntohl(otherside_addr.sin_addr.s_addr);
    porttmp = ntohs(otherside_addr.sin_port);

    new_sk = spines_socket(PF_SPINES, my_type, protocol, (struct sockaddr*)(&old_addr));

    if(new_sk < 0) 
	return(-1);
	
    *total_len = (int32)(2*sizeof(udp_header) + sizeof(int32) + data_size);

    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = ACCEPT_TYPE_MSG;

    cmd->source = 0;
    cmd->dest   = addrtmp;
    cmd->dest_port = porttmp;
    cmd->len    = data_size;
    
    ret = send(new_sk, pkt, *total_len+sizeof(int32), 0);
    
    if(ret != 2*sizeof(udp_header)+2*sizeof(int32)+data_size) {
	spines_close(new_sk);
	return(-1);
    }
        
    
    recv_bytes = 0;
    while(recv_bytes < sizeof(ses_hello_packet)) {
	ret = spines_recv(new_sk, buf, sizeof(ses_hello_packet) - recv_bytes, 0); 
	if(ret <= 0)
	    return(-1);
	recv_bytes += ret;
    }

    if(ret <= 0) {
	spines_close(new_sk);
	return(-1);
    }

    return(new_sk);
}





/***********************************************************/
/* int spines_setlink(int sk, const struct sockaddr *addr, */
/*                    int bandwidth, int latency,          */
/*                    float loss, float burst)             */ 
/*                                                         */
/* Sets the loss rate on packets received from a daemon    */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the Spines socket (SOCK_STREAM type)           */
/* addr:    Spines node from which the loss rate is set    */
/* link:    link latency                                   */
/* loss:    loss rate                                      */
/* burst:   conditional probability of loss                */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  0 if success                                     */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

 int spines_setlink(int sk, const struct sockaddr *addr, 
                    int bandwidth, int latency, float loss, float burst)
{
    udp_header *u_hdr, *cmd;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *bandwidth_in, *latency_in, *loss_rate, *burst_rate;
    int32 *type;
    int ret;
    int address;


    address = ntohl(((struct sockaddr_in*)addr)->sin_addr.s_addr);

    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    cmd = (udp_header*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));
    bandwidth_in = (int32*)(pkt+sizeof(int32)+2*sizeof(udp_header)+sizeof(int32));
    latency_in = (int32*)(pkt+sizeof(int32)+2*sizeof(udp_header)+2*sizeof(int32));
    loss_rate = (int32*)(pkt+sizeof(int32)+2*sizeof(udp_header)+3*sizeof(int32));
    burst_rate = (int32*)(pkt+sizeof(int32)+2*sizeof(udp_header)+4*sizeof(int32));

    *bandwidth_in = bandwidth; 
    *latency_in = latency; 
    *loss_rate = (int32)(loss*10000);
    *burst_rate = (int32)(burst*10000);
    
    *total_len = (int32)(2*sizeof(udp_header) + 5*sizeof(int32));   
    
    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = SETLINK_TYPE_MSG;

    cmd->source = 0;
    cmd->dest   = address;
    cmd->dest_port   = 0;
    cmd->len    = 4*sizeof(int32);

    ret = send(sk, pkt, *total_len+sizeof(int32), 0);
    
    if(ret != 2*sizeof(udp_header)+6*sizeof(int32))
	return(-1);

    return(0);  
}




int spines_get_client(int sk) {
    int i;
    for(i=0; i<Max_Client; i++) {
	if(all_clients[i].udp_sk == sk) {
	    return(i);
	}
    }
    return(-1);
}



/***********************************************************/
/* int spines_reliable_join(int sk, int address, int flags)*/ 
/*                                                         */
/* Joins a group for reliable multicast                    */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the socket defining the connection to Spines   */
/* address: the multicast address                          */
/* flags:   SEND_GROUP or RECV_GROUP or SENDRECV_GROUP     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if join operation was ok                       */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/


int spines_reliable_join(int sk, int address, int flags)
{
    udp_header *u_hdr, *cmd;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *type;
    int ret;
    int client;
    int32 *flags_store;

    if((address & 0xF0000000) != 0xE0000000) {
    	Alarm(PRINT, "spines_reliable_join: not a multicast address\n");
    	Alarm(PRINT, "address = "IPF"\n", IP(address));
    	Alarm(PRINT, "address & 0xF0000000 = %x\n", address & 0xF0000000);
    	return(-1);
    }

    Mutex_Lock(&data_mutex);
    client = spines_get_client(sk);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    all_clients[client].rel_mcast_flag = 1;
    Mutex_Unlock(&data_mutex);


    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    cmd = (udp_header*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));
    flags_store = (int32*)(pkt+sizeof(int32)+2*sizeof(udp_header)+sizeof(int32));


    *total_len = (int32)(2*sizeof(udp_header) + 2*sizeof(int32));
    *flags_store = flags;

        
    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = REL_JOIN_TYPE_MSG;

    cmd->source = 0;
    cmd->dest   = address;
    cmd->dest_port   = 0;
    cmd->len    = sizeof(int32);
    
    Alarm(PRINT,"socket number: %d\n", sk);

    ret = send(sk, pkt, (*total_len)+sizeof(int32), 0);
    
    if(ret == 2*sizeof(udp_header)+3*sizeof(int32))
	return(1);
    else
	return(-1);
}


int spines_flood_send(int sockfd, int address, int port, int rate, int size, int num_pkt)
{
    udp_header *u_hdr;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *type;
    int ret;
    int client, my_type, tcp_sk, sk, connect_flag;
    
    int32 *dest, *dest_port, *send_rate, *pkt_size, *num;


    Mutex_Lock(&data_mutex);
    client = spines_get_client(sockfd);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    my_type = all_clients[client].type;
    tcp_sk = all_clients[client].tcp_sk;
    connect_flag = all_clients[client].connect_flag;
    Mutex_Unlock(&data_mutex);


    sk = sockfd;

    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    dest = (int32*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));
    dest_port = (int32*)(pkt+sizeof(int32)+sizeof(udp_header)+2*sizeof(int32));
    send_rate = (int32*)(pkt+sizeof(int32)+sizeof(udp_header)+3*sizeof(int32));
    pkt_size = (int32*)(pkt+sizeof(int32)+sizeof(udp_header)+4*sizeof(int32));
    num = (int32*)(pkt+sizeof(int32)+sizeof(udp_header)+5*sizeof(int32));
    


    *total_len = (int32)(sizeof(udp_header) + 6*sizeof(int32));

    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = FLOOD_SEND_TYPE_MSG;

    *dest   = address;
    *dest_port   = port;
    *send_rate   = rate;
    *pkt_size    = size;
    *num         = num_pkt;
    
    ret = send(sk, pkt, *total_len+sizeof(int32), 0);
    if(ret != sizeof(udp_header)+7*sizeof(int32))
	return(-1);


    ret = spines_recvfrom(sk, pkt, sizeof(pkt), 1, NULL, NULL);
    if(ret <= 0) {
	return(-1);
    }

    return(0);
}



int spines_flood_recv(int sockfd, char *filename, int namelen)
{
    udp_header *u_hdr;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *type;
    int ret;
    int client, my_type, tcp_sk, sk, connect_flag;
    int *len;
    char *name;

    Mutex_Lock(&data_mutex);
    client = spines_get_client(sockfd);
    if(client == -1) {
	Mutex_Unlock(&data_mutex);
	return(-1);
    }
    my_type = all_clients[client].type;
    tcp_sk = all_clients[client].tcp_sk;
    connect_flag = all_clients[client].connect_flag;
    Mutex_Unlock(&data_mutex);


    sk = sockfd;

    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    len = (int32*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));
    name = (char*)(pkt+sizeof(int32)+sizeof(udp_header)+2*sizeof(int32));


    *total_len = (int32)(sizeof(udp_header) + 2*sizeof(int32) + namelen);

    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = FLOOD_RECV_TYPE_MSG;
    *len   = namelen;
    memcpy(name, filename, namelen);

    
    ret = send(sk, pkt, *total_len+sizeof(int32), 0);
    if(ret != *total_len+sizeof(int32))
	return(-1);


    ret = spines_recvfrom(sk, pkt, sizeof(pkt), 1, NULL, NULL);
    if(ret <= 0) {
	return(-1);
    }

    return(0);
}


/***********************************************************/
/* int spines_ctrl_socket(const struct sockaddr *serv_addr)*/
/*                                                         */
/* Creates a Spines Control socket                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* srvr_addr: Address of the Spines node                   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the socket the application can use to             */
/*       send/receive data                                 */
/*       -1 in case of an error                            */
/*                                                         */
/***********************************************************/

int  spines_ctrl_socket(const struct sockaddr *serv_addr)
{
    struct sockaddr_in host;
    udp_header *u_hdr;
    char buf[MAX_PACKET_SIZE];
    int32 *total_len;
    int32 *msg_type;
    int ret, sk, u_sk;
    int val = 1;
    int32 *flag_var, *rnd_var, *port_var, *addr_var;
    struct hostent  *host_ptr;
    char   machine_name[256];
    int address, port;

    spines_init(serv_addr);

    total_len = (int32*)(buf);
    u_hdr = (udp_header*)(buf+sizeof(int32));
    msg_type = (int32*)(buf+sizeof(int32)+sizeof(udp_header));
    flag_var = (int32*)(buf+sizeof(int32)+sizeof(udp_header)+sizeof(int32));
    rnd_var = (int32*)(buf+sizeof(int32)+sizeof(udp_header)+2*sizeof(int32));
    addr_var = (int32*)(buf+sizeof(int32)+sizeof(udp_header)+3*sizeof(int32));
    port_var = (int32*)(buf+sizeof(int32)+sizeof(udp_header)+4*sizeof(int32));


    sk = socket(AF_INET, SOCK_STREAM, 0);        
    if (sk < 0) {
	Alarm(PRINT, "spines_socket(): Can not initiate socket...");
	return(-1);
    }

    if (setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val))){
	Alarm(PRINT, "spines_socket(): Failed to set socket option TCP_NODELAY\n");
	close(sk);
	return(-1);
    }

    u_sk = sk;

    if(serv_addr != NULL) {
	memcpy(&host, serv_addr, sizeof(struct sockaddr_in));
    }
    else if(set_flag == 1) {
	memcpy(&host, &spines_addr, sizeof(struct sockaddr_in));
    }
    else {
	/* No info about where the Spines server is. Try local machine */
	gethostname(machine_name,sizeof(machine_name)); 
	host_ptr = gethostbyname(machine_name);
	
	if(host_ptr == NULL) {
	    Alarm(PRINT, "spines_socket(): could not get my default interface (my name is %s)\n",
		   machine_name );
	    return(-1);
	}
	if(host_ptr->h_addrtype != AF_INET) {
	    Alarm(PRINT, "spines_socket(): Sorry, cannot handle addr types other than IPv4\n");
	    return(-1);
	}
	
	if(host_ptr->h_length != 4) {
	    Alarm(PRINT, "spines_socket(): Bad IPv4 address length\n");
	    return(-1);
	}
	memcpy(&host.sin_addr.s_addr, host_ptr->h_addr, sizeof(struct in_addr));

	host.sin_port   = htons((int16)DEFAULT_SPINES_PORT);
    }
    address = ntohl(host.sin_addr.s_addr);
    port = ntohs(host.sin_port);
    host.sin_family = AF_INET;
    host.sin_port   = htons((int16)(ntohs(host.sin_port)+SESS_CTRL_PORT));

    ret = connect(sk, (struct sockaddr *)&host, sizeof(host));
    if( ret < 0) {
	Alarm(PRINT, "3 spines_socket(): Can not initiate connection to Spines...\n");
	return(-1);
    }
    return(sk);
}

/***********************************************************/
/* int spines_traceroute()                                 */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*  spines_trace: Pointer to Struct spines_trace           */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if connect was ok                              */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

int spines_traceroute(const struct sockaddr *dest_addr, spines_trace *sp_trace, 
	const struct sockaddr *serv_addr)
{

    udp_header *u_hdr, *cmd;
    char pkt[MAX_PACKET_SIZE];
    int32 *total_len, *type, dest, src;
    int ctrl_sk, ret;

    ctrl_sk = spines_ctrl_socket(serv_addr);
    src  = ntohl(((struct sockaddr_in*)serv_addr)->sin_addr.s_addr);
    dest = ntohl(((struct sockaddr_in*)dest_addr)->sin_addr.s_addr);

    if (dest == src || Is_mcast_addr(dest)) {
        Alarm(PRINT, "spines_lib: trace_route() error: Invalid Destination\n");
        return(-1);
    }

    total_len = (int32*)(pkt);
    u_hdr = (udp_header*)(pkt+sizeof(int32));
    type = (int32*)(pkt+sizeof(int32)+sizeof(udp_header));
    cmd = (udp_header*)(pkt+sizeof(int32)+sizeof(udp_header)+sizeof(int32));

    *total_len = (int32)(2*sizeof(udp_header) + sizeof(int32));

    u_hdr->source = 0;
    u_hdr->dest   = 0;
    u_hdr->len    = 0;

    *type = TRACEROUTE_TYPE_MSG;

    cmd->source = 0;
    cmd->dest = dest;
    cmd->dest_port = 0;
    cmd->len = 0;

    ret = send(ctrl_sk, pkt, *total_len+sizeof(int32), 0);

    if (ret != 2+sizeof(udp_header)+2*sizeof(int32)) {
        Alarm(PRINT, "spines_lib: Problem Sending Trace Route Request\n");
        return(-1);
    }

    ret = spines_recvfrom(ctrl_sk, pkt, sizeof(pkt), 1, NULL, NULL);

    if (ret < sizeof(spines_trace)) {
        Alarm(PRINT, "spines_lib: Internet Hosts Struct Received [%d bytes] too short.\n", ret);
        return(-1);
    }

    memcpy(sp_trace, pkt, sizeof(spines_trace));

    close(ctrl_sk);
    return(1);
}

