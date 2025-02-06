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


#ifndef	ARCH_PC_WIN95

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#else

#include <winsock.h>

#endif

#include <string.h>


#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/data_link.h"
#include "net_types.h"
#include "session.h"

#include "spines_lib.h"




/***********************************************************/
/* int spines_socket(int port, int address)                */
/*                                                         */
/* Connects the application to the Spines network          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* port: the port Spines is running                        */
/* address: the address of the on daemon                   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the socket the application can use to             */
/*       send/receive                                      */
/*                                                         */
/***********************************************************/

int spines_socket(int port, int address)
{
    struct sockaddr_in host;
    int ret, sk;
    int val = 1;
    int16u on_port;
    int32 on_address;
    
    on_port = (int16u)port;
    on_address = (int32)address;

    
    sk = socket(AF_INET, SOCK_STREAM, 0);
        
    if (sk < 0) {
	Alarm(EXIT, "on_socket(): Can not initiate socket...");
    }

    if (setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val))){
	Alarm( EXIT, "on_socket(): Failed to set socket option TCP_NODELAY\n");
    }
    
    host.sin_family = AF_INET;
    host.sin_port   = htons((int16)(on_port+SESS_PORT));
    host.sin_addr.s_addr = htonl(on_address);
    
    ret = connect(sk, (struct sockaddr *)&host, sizeof(host));
    if( ret < 0) {
	Alarm(EXIT, "spines_socket(): Can not initiate connection to on...\n");
    }

    return sk;
}


/***********************************************************/
/* void spines_close(int sk)                               */
/*                                                         */
/* Disconnects the application from the Spines network     */
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

void spines_close(int sk)
{
    DL_close_channel(sk);
}


/***********************************************************/
/* int spines_sendto(int sk, int address, int port_i       */
/*                   char *buff, int len_i)                */
/*                                                         */
/* Sends data through the Spines network                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the socket defining the connection to Spines   */
/* address: the IP address to send to                      */
/* port_i:  the port (fake) on which the receiver "binds"  */
/* buff:    a pointer to the message                       */
/* len_i:   length of the message                          */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the number of bytes sent (or -1 if it's an error) */
/*                                                         */
/***********************************************************/

int spines_sendto(int sk, int address, int port_i, char *buff, int len_i)
{
    udp_header u_hdr;
    sys_scatter scat;
    int16u port, len, total_len;
    int ret;

    port = (int16u)port_i;
    len = (int16u)len_i;
    total_len = len + sizeof(udp_header);

    scat.num_elements = 3;
    scat.elements[0].len = 2;
    scat.elements[0].buf = (char*)&total_len;
    scat.elements[1].len = sizeof(udp_header);
    scat.elements[1].buf = (char*)&u_hdr;
    scat.elements[2].len = len;
    scat.elements[2].buf = buff;
    
    u_hdr.source = 0;
    u_hdr.dest   = (int32)address;
    u_hdr.dest_port   = port;
    u_hdr.len    = len;

    ret = DL_send(sk, 0, 0, &scat );
    
    if(ret > 0)
	ret -= sizeof(udp_header) + 2;
    
    return(ret);
}


/***********************************************************/
/* int spines_bind(int sk, int port_i)                     */ 
/*                                                         */
/* Binds to a port of the Spines network                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the socket defining the connection to Spines   */
/* port_i:  the port (fake) on which the receiver "binds"  */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if bind was ok                                 */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

int spines_bind(int sk, int port_i)
{
    udp_header u_hdr, cmd;
    sys_scatter scat;
    int16u port, total_len;
    int32 type;
    int ret;

    port = (int16u)port_i;
    total_len = (int16u)(2*sizeof(udp_header) + sizeof(int32));

    scat.num_elements = 4;
    scat.elements[0].len = 2;
    scat.elements[0].buf = (char*)&total_len;
 
    scat.elements[1].len = sizeof(udp_header);
    scat.elements[1].buf = (char*)&u_hdr;
       
    scat.elements[2].len = sizeof(int32);
    scat.elements[2].buf = (char*)&type;    
    
    scat.elements[3].len = sizeof(udp_header);
    scat.elements[3].buf = (char*)&cmd;
        
    u_hdr.source = 0;
    u_hdr.dest   = 0;
    u_hdr.len    = 0;

    type = BIND_TYPE_MSG;

    cmd.source = 0;
    cmd.dest   = 0;
    cmd.dest_port   = port;
    cmd.len    = 0;
    
    ret = DL_send(sk, 0, 0, &scat );
    
    if(ret == 2*sizeof(udp_header)+2+sizeof(int32))
	return(1);
    else
	return(-1);
}




/***********************************************************/
/* int spines_recvfrom(int sk, int *sender, int *port,     */
/*                 char *buff, int len)                    */ 
/*                                                         */
/* Receives data from the Spines network                   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the socket defining the connection to Spines   */
/* sender:  the IP address of the sender                   */
/* buff:    a buffer to receive into                       */
/* len_i:   length of the buffer                           */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the number of bytes received                      */
/*                                                         */
/***********************************************************/

int spines_recvfrom(int sk, int *sender, int *port, char *buff, int len) {
    int received_bytes;
    udp_header u_hdr;
    sys_scatter scat;
    int16u msg_len;
    char *ptr;
    int total_bytes;
    
    
    ptr = (char*)&msg_len;
    scat.num_elements = 1;
    scat.elements[0].len = 2;
    scat.elements[0].buf = ptr;
    total_bytes = 0;
    
    while(total_bytes < 2) { 
	received_bytes = DL_recv(sk, &scat);
	if(received_bytes <= 0)
	    return(received_bytes);
	total_bytes += received_bytes;
	if(total_bytes > 2)
	    Alarm(EXIT, "on_recv(): socket error\n");
	scat.elements[0].len -= received_bytes;
	scat.elements[0].buf = ptr + received_bytes;
    }

    if(msg_len > len)
	Alarm(EXIT, "on_recv(): message too big\n");

    ptr = (char*)&u_hdr;
    scat.num_elements = 1;
    scat.elements[0].len = sizeof(udp_header);
    scat.elements[0].buf = ptr;
    total_bytes = 0;
    
    while(total_bytes < sizeof(udp_header)) { 
	received_bytes = DL_recv(sk, &scat);
	if(received_bytes <= 0)
	    return(received_bytes);
	total_bytes += received_bytes;
	if(total_bytes > sizeof(udp_header))
	    Alarm(EXIT, "on_recv(): socket error\n");
	scat.elements[0].len -= received_bytes;
	scat.elements[0].buf = ptr + received_bytes;
    }

    ptr = buff;
    scat.num_elements = 1;
    scat.elements[0].len = msg_len - sizeof(udp_header);
    scat.elements[0].buf = ptr;
    total_bytes = 0;
    
    while(total_bytes < (int)msg_len - (int)sizeof(udp_header)) { 
	received_bytes = DL_recv(sk, &scat);
	if(received_bytes <= 0)
	    return(received_bytes);
	total_bytes += received_bytes;
	if(total_bytes > (int)msg_len - (int)sizeof(udp_header))
	    Alarm(EXIT, "on_recv(): socket error\n");
	scat.elements[0].len -= received_bytes;
	scat.elements[0].buf = ptr + received_bytes;
    }

    *sender = u_hdr.source;
    *port = (int)u_hdr.source_port;

    return(total_bytes);
}





/***********************************************************/
/* int spines_connect(int sk, int address_i, int port_i)   */ 
/*                                                         */
/* Connects to another application reliably using spines   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the socket defining the connection to Spines   */
/* address_i: the address to connect to                    */
/* port_i:  the port to connect to                         */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if connect was ok                              */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

int spines_connect(int sk, int address_i, int port_i)
{
    udp_header u_hdr, cmd;
    sys_scatter scat;
    int16u port, total_len;
    int32 address;
    int32 type;
    int ret;
    char buf[200];

    
    address = (int32)address_i;
    port = (int16u)port_i;

    total_len = (int16u)(2*sizeof(udp_header) + sizeof(int32));   

    scat.num_elements = 4;
    scat.elements[0].len = 2;
    scat.elements[0].buf = (char*)&total_len;
 
    scat.elements[1].len = sizeof(udp_header);
    scat.elements[1].buf = (char*)&u_hdr;
    
    scat.elements[2].len = sizeof(int32);
    scat.elements[2].buf = (char*)&type;    

    scat.elements[3].len = sizeof(udp_header);
    scat.elements[3].buf = (char*)&cmd;
  

    u_hdr.source = 0;
    u_hdr.dest   = 0;
    u_hdr.len    = 0;

    type = CONNECT_TYPE_MSG;

    cmd.source = 0;
    cmd.dest   = address;
    cmd.dest_port   = port;
    cmd.len    = 0;

    ret = DL_send(sk, 0, 0, &scat );
    
    if(ret != 2*sizeof(udp_header)+2+sizeof(int32))
	return(-1);

    ret = spines_recv(sk, buf, sizeof(buf));
    if(ret < 0)
	return(-1);

    Alarm(PRINT, "Connect successfull !\n");

    return(1);
}



/***********************************************************/
/* int spines_send(int sk, char *buff, int len_i)          */
/*                                                         */
/* Sends reliable data through the Spines network          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the socket defining the connection to Spines   */
/* buff:    a pointer to the message                       */
/* len_i:   length of the message                          */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the number of bytes sent (or -1 if it's an error) */
/*                                                         */
/***********************************************************/

int spines_send(int sk, char *buff, int len_i)
{
    udp_header u_hdr;
    rel_udp_pkt_add r_add;
    sys_scatter scat;
    int16u len, total_len;
    int ret;

    len = (int16u)len_i;
    total_len = len + sizeof(udp_header) + sizeof(rel_udp_pkt_add);

    scat.num_elements = 4;
    scat.elements[0].len = 2;
    scat.elements[0].buf = (char*)&total_len;
    scat.elements[1].len = sizeof(udp_header);
    scat.elements[1].buf = (char*)&u_hdr;
    scat.elements[2].len = sizeof(rel_udp_pkt_add);
    scat.elements[2].buf = (char*)&r_add;
    scat.elements[3].len = len;
    scat.elements[3].buf = buff;
    
    u_hdr.source = 0;
    u_hdr.dest   = 0;
    u_hdr.dest_port   = 0;
    u_hdr.len    = len + sizeof(rel_udp_pkt_add);

    r_add.type = Set_endian(0);
    r_add.data_len = len;
    r_add.ack_len = 0;

    ret = DL_send(sk, 0, 0, &scat );
    
    if(ret > 0)
	ret -= sizeof(udp_header) + sizeof(rel_udp_pkt_add) + 2;
    
    return(ret);
}





/***********************************************************/
/* int spines_recv(int sk, char *buff, int len)            */ 
/*                                                         */
/* Receives reliable data from the Spines network          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the socket defining the connection to Spines   */
/* buff:    a buffer to receive into                       */
/* len:     length of the buffer                           */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) the number of bytes received                      */
/*                                                         */
/***********************************************************/

int spines_recv(int sk, char *buff, int len) {
    int received_bytes;
    udp_header u_hdr;
    rel_udp_pkt_add r_add;
    sys_scatter scat;
    int16u msg_len;
    char *ptr;
    int total_bytes;
    
    
    ptr = (char*)&msg_len;
    scat.num_elements = 1;
    scat.elements[0].len = 2;
    scat.elements[0].buf = ptr;
    total_bytes = 0;
    
    while(total_bytes < 2) { 
	received_bytes = DL_recv(sk, &scat);
	if(received_bytes <= 0)
	    return(received_bytes);
	total_bytes += received_bytes;
	if(total_bytes > 2)
	    Alarm(EXIT, "on_recv(): socket error\n");
	scat.elements[0].len -= received_bytes;
	scat.elements[0].buf = ptr + received_bytes;
    }

    if(msg_len > len)
	Alarm(EXIT, "on_recv(): message too big\n");

    ptr = (char*)&u_hdr;
    scat.num_elements = 1;
    scat.elements[0].len = sizeof(udp_header);
    scat.elements[0].buf = ptr;
    total_bytes = 0;
    
    while(total_bytes < sizeof(udp_header)) { 
	received_bytes = DL_recv(sk, &scat);
	if(received_bytes <= 0)
	    return(received_bytes);
	total_bytes += received_bytes;
	if(total_bytes > sizeof(udp_header))
	    Alarm(EXIT, "on_recv(): socket error\n");
	scat.elements[0].len -= received_bytes;
	scat.elements[0].buf = ptr + received_bytes;
    }


    ptr = (char*)&r_add;
    scat.num_elements = 1;
    scat.elements[0].len = sizeof(rel_udp_pkt_add);
    scat.elements[0].buf = ptr;
    total_bytes = 0;
    
    while(total_bytes < sizeof(rel_udp_pkt_add)) { 
	received_bytes = DL_recv(sk, &scat);
	if(received_bytes <= 0)
	    return(received_bytes);
	total_bytes += received_bytes;
	if(total_bytes > sizeof(rel_udp_pkt_add))
	    Alarm(EXIT, "spines_recv(): socket error\n");
	scat.elements[0].len -= received_bytes;
	scat.elements[0].buf = ptr + received_bytes;
    }


    ptr = buff;
    scat.num_elements = 1;
    scat.elements[0].len = msg_len - sizeof(udp_header) - 
	sizeof(rel_udp_pkt_add);
    scat.elements[0].buf = ptr;
    total_bytes = 0;
    
    while(total_bytes < (int)msg_len - (int)sizeof(udp_header) - 
	  sizeof(rel_udp_pkt_add)) { 
	received_bytes = DL_recv(sk, &scat);
	if(received_bytes <= 0)
	    return(received_bytes);
	total_bytes += received_bytes;
	if(total_bytes > (int)msg_len - (int)sizeof(udp_header) - 
	   sizeof(rel_udp_pkt_add))
	    Alarm(EXIT, "on_recv(): socket error\n");
	scat.elements[0].len -= received_bytes;
	scat.elements[0].buf = ptr + received_bytes;
    }

    return(total_bytes);
}



/***********************************************************/
/* int spines_listen(int sk)                               */ 
/*                                                         */
/* Listens on a port of the Spines network                 */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      the socket defining the connection to Spines   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 if listen was ok                               */
/*       -1 otherwise                                      */
/*                                                         */
/***********************************************************/

int spines_listen(int sk)
{
    udp_header u_hdr, cmd;
    sys_scatter scat;
    int16u total_len;
    int32 type;
    int ret;

    total_len = (int16u)(2*sizeof(udp_header) + sizeof(int32));

    scat.num_elements = 4;
    scat.elements[0].len = 2;
    scat.elements[0].buf = (char*)&total_len;
 
    scat.elements[1].len = sizeof(udp_header);
    scat.elements[1].buf = (char*)&u_hdr;
       
    scat.elements[2].len = sizeof(int32);
    scat.elements[2].buf = (char*)&type;    
    
    scat.elements[3].len = sizeof(udp_header);
    scat.elements[3].buf = (char*)&cmd;
        
    u_hdr.source = 0;
    u_hdr.dest   = 0;
    u_hdr.len    = 0;

    type = LISTEN_TYPE_MSG;

    cmd.source = 0;
    cmd.dest   = 0;
    cmd.dest_port   = 0;
    cmd.len    = 0;
    
    ret = DL_send(sk, 0, 0, &scat );
    
    if(ret == 2*sizeof(udp_header)+2+sizeof(int32))
	return(1);
    else
	return(-1);
}



/***********************************************************/
/* int spines_accept(int sk, int on_port_i,                */
/*                                      int on_address_i)  */ 
/*                                                         */
/* Accepts a conection, similarly to TCP accept            */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:           the socket defining the listen session    */
/* on_port_i:    the port Spines is running                */
/* on_address_i: the address of the on daemon              */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) a socket for a new session                        */
/*       -1 error                                          */
/*                                                         */
/***********************************************************/

int spines_accept(int sk, int on_port_i, int on_address_i)
{
    udp_header u_hdr, cmd;
    sys_scatter scat;
    int16u total_len;
    int32 type;
    int32 addr, port;
    int32 on_addr;
    int16 on_port;
    char buf[100];
    int ret, data_size;
    int new_sk;


    on_addr = on_address_i;
    on_port = (int16)on_port_i;

    ret = spines_recvfrom(sk, &addr, &port, buf, sizeof(buf));

    if(ret <= 0)
	return(-1);
    
    data_size = ret;


    new_sk = spines_socket(on_port, on_addr);

    if(new_sk < 0) 
	return(-1);
	

    total_len = (int16u)(2*sizeof(udp_header) + sizeof(int32) + data_size);

    scat.num_elements = 5;
    scat.elements[0].len = 2;
    scat.elements[0].buf = (char*)&total_len;
 
    scat.elements[1].len = sizeof(udp_header);
    scat.elements[1].buf = (char*)&u_hdr;
       
    scat.elements[2].len = sizeof(int32);
    scat.elements[2].buf = (char*)&type;    
    
    scat.elements[3].len = sizeof(udp_header);
    scat.elements[3].buf = (char*)&cmd;
    
    scat.elements[4].len = data_size;
    scat.elements[4].buf = buf;
        
    u_hdr.source = 0;
    u_hdr.dest   = 0;
    u_hdr.len    = 0;

    type = ACCEPT_TYPE_MSG;

    cmd.source = 0;
    cmd.dest   = addr;
    cmd.dest_port   = port;
    cmd.len    = data_size;
    
    ret = DL_send(new_sk, 0, 0, &scat );
    
    if(ret != 2*sizeof(udp_header)+2+sizeof(int32)+data_size) {
	spines_close(new_sk);
	return(-1);
    }
        
    ret = spines_recv(new_sk, buf, sizeof(buf));
    if(ret < 0)
	return(-1);

    Alarm(PRINT, "Connect successfull !\n");
  
    return(new_sk);
}
