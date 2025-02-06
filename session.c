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


#include "util/arch.h"


#ifndef	ARCH_PC_WIN95

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#else

#include <winsock.h>

#endif

#ifdef ARCH_SPARC_SOLARIS
#include <unistd.h>
#include <stropts.h>
#endif

#include <errno.h>

#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/memory.h"
#include "util/data_link.h"
#include "stdutil/src/stdutil/stdhash.h"
#include "stdutil/src/stdutil/stdcarr.h"

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "link.h"
#include "network.h"
#include "reliable_link.h"
#include "link_state.h"
#include "hello.h"
#include "udp.h"
#include "reliable_udp.h"
#include "protocol.h"
#include "route.h"
#include "session.h"
#include "reliable_session.h"

/* Global variables */

extern int16     Port;
extern int32     My_Address;
extern int       Reliable_Flag;
extern stdhash   Sessions_ID;
extern stdhash   Sessions_Port;
extern stdhash   Rel_Sessions_Port;
extern stdhash   Sessions_Sock;
extern int16     Link_Sessions_Blocked_On;

/* Local variables */

static int32u   Session_Num;


/***********************************************************/
/* void Init_Session(void)                                 */
/*                                                         */
/* Initializes the session layer                           */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Init_Session(void)
{
    int sk_local;
    struct sockaddr_in name;
    int ret, val;

    Session_Num = 0;

    stdhash_construct(&Sessions_ID, sizeof(int32), sizeof(Session*), 
                      stdhash_int_equals, stdhash_int_hashcode);
    stdhash_construct(&Sessions_Port, sizeof(int32), sizeof(Session*), 
                      stdhash_int_equals, stdhash_int_hashcode);
    stdhash_construct(&Rel_Sessions_Port, sizeof(int32), sizeof(Session*), 
                      stdhash_int_equals, stdhash_int_hashcode);
    stdhash_construct(&Sessions_Sock, sizeof(int32), sizeof(Session*), 
                      stdhash_int_equals, stdhash_int_hashcode);

    sk_local = socket(AF_INET, SOCK_STREAM, 0);
    if (sk_local<0) {
	Alarm(EXIT, "Int_Session(): socket failed\n");
    } 

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons((int16)(Port+SESS_PORT));


   val = 1;
   if(setsockopt(sk_local, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val)))
   {
           Alarm( EXIT, "Init_Session: Failed to set socket option REUSEADDR, errno: %d\n", errno);
   }

    
    ret = bind(sk_local, (struct sockaddr *) &name, sizeof(name)); 
    if (ret == -1) {
	Alarm(EXIT, "Init_Session: bind error for port %d\n",Port);
    }

    if(listen(sk_local, 4) < 0) {
	Alarm(EXIT, "Session_Init(): Listen failure\n");
    }

    Alarm(DEBUG, "listen successful on socket: %d\n", sk_local);

    Link_Sessions_Blocked_On = -1;

    E_attach_fd(sk_local, READ_FD, Session_Accept, 0, NULL, MEDIUM_PRIORITY );
}



/***********************************************************/
/* void Session_Accept(int sk_local, int dummy,            */
/*                     void *dummy_p)                      */
/*                                                         */
/* Accepts a session socket                                */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk_local: the listen socket                             */
/* dummy, dummy_p: not used                                */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Session_Accept(int sk_local, int dummy, void *dummy_p)
{
    struct sockaddr_in acc_sin;
    int acc_sin_len = sizeof(acc_sin);
    int val, ioctl_cmd;
    channel sk;
    Session *ses;
    stdhash_it it;
    int ret;

    sk = accept(sk_local, (struct sockaddr*)&acc_sin, (int*)&acc_sin_len);

    /* Setting no delay option on the socket */
    val = 1;
    if (setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val))) {
	    Alarm( EXIT, "Session_Accept: Failed to set TCP_NODELAY\n");
    }
    
    /* set file descriptor to non blocking */
    ioctl_cmd = 1;

#ifdef ARCH_PC_WIN95
    ret = ioctlsocket(sk, FIONBIO, &ioctl_cmd);
#else
    ret = ioctl(sk, FIONBIO, &ioctl_cmd);
#endif

    if((ses = (Session*) new(SESSION_OBJ))==NULL) {
	Alarm(EXIT, "Session_Accept(): Cannot allocte session object\n");
    }

    ses->sess_id = Session_Num++;
    ses->type = UDP_SES_TYPE;
    ses->sk = sk;
    ses->port = 0;
    ses->total_len = 2;
    ses->partial_len = 0;
    ses->state = READY_LEN;
    ses->r_data = NULL;
    ses->rel_blocked = 0;
    ses->client_stat = SES_CLIENT_ON;

    if((ses->data = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
	Alarm(EXIT, "Session_Accept(): Cannot allocte packet_body object\n");
    }

    stdcarr_construct(&ses->udp_deliver_buff, sizeof(UDP_Cell*));

    stdhash_insert(&Sessions_Sock, &it, &sk, &ses);
    stdhash_insert(&Sessions_ID, &it, &(ses->sess_id), &ses);

    Alarm(PRINT, "new session...%d\n", sk);

    E_attach_fd(ses->sk, READ_FD, Session_Read, 0, NULL, HIGH_PRIORITY );
    E_attach_fd(ses->sk, EXCEPT_FD, Session_Read, 0, NULL, HIGH_PRIORITY );
    
    ses->fd_flags = READ_DESC | EXCEPT_DESC;
} 




/***********************************************************/
/* void Session_Read(int sk, int dummy, void *dummy_p)     */
/*                                                         */
/* Reads data from a session socket                        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk: the session socket                                  */
/* dummy, dummy_p: not used                                */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Session_Read(int sk, int dummy, void *dummy_p)
{
    sys_scatter scat;
    Session *ses;
    stdhash_it it;
    int received_bytes;
    int ret;

    stdhash_find(&Sessions_Sock, &it, &sk);
    if(stdhash_it_is_end(&it)) {
        Alarm(EXIT, "Session_Read(): socket does not exist\n");
    }
    ses = *((Session **)stdhash_it_val(&it));

    scat.num_elements = 1;
    scat.elements[0].len = ses->total_len - ses->partial_len;
    scat.elements[0].buf = (char*)(ses->data + ses->partial_len);

    received_bytes = DL_recv(ses->sk, &scat);

    if(received_bytes <= 0) {
	/* This is non-blocking socket. Not all the errors are treated as 
	 * a disconnect. */
	if(received_bytes == -1) {

#ifndef	ARCH_PC_WIN95
	    if((errno == EWOULDBLOCK)||(errno == EAGAIN)) {
#else
	    if((errno == WSAEWOULDBLOCK)||(errno == EAGAIN)) {
#endif
		Alarm(DEBUG, "EAGAIN - Session_Read()\n");
		return;
	    }
	    else {
		if(ses->r_data == NULL) {
		    Session_Close(ses->sk, SOCK_ERR);
		}		
		else {
		    Disconnect_Reliable_Session(ses);
		}
	    }
	}
	else {
	    if(ses->r_data == NULL) {
		Session_Close(ses->sk, SOCK_ERR);
	    }		
	    else {
		Disconnect_Reliable_Session(ses);
	    }
	}
	return;
    }

    if(received_bytes + ses->partial_len > ses->total_len) 
	Alarm(EXIT, "Session_Read(): Too many bytes...\n");
    
    if(received_bytes + ses->partial_len < ses->total_len) {
	ses->partial_len += received_bytes;
    }
    else {
	if(ses->state == READY_LEN) {
	    ses->total_len = *((int16u*)(ses->data));
	    ses->partial_len = 0;
	    ses->state = READY_DATA;
	}
	else if(ses->state == READY_DATA) {

	    ret = Process_Session_Packet(ses);
	    
	    if(ret == NO_BUFF){
		return;
	    }
	    else if((ret == BUFF_EMPTY)||(ret == BUFF_DROP)||
		    (ret == BUFF_FULL)||(ret == BUFF_OK)||
		    (ret == NO_ROUTE)) {
		ses->total_len = 2;
		ses->partial_len = 0;
		ses->state = READY_LEN;

		if(get_ref_cnt(ses->data) > 1) {
		    dec_ref_cnt(ses->data);
		    if((ses->data = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
			Alarm(EXIT, "Session_Read(): Cannot allocte packet_body object\n");
			
		    }
		}
	    }
	    else {
		Alarm(PRINT, "!!!!! ERROR\n");
	    }
	}
    }	
}



/***********************************************************/
/* void Session_Close(int sk, int reason)                  */
/*                                                         */
/* Closes a session                                        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:     the session socket                              */
/* reason: see session.h                                   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Session_Close(int sk, int reason)
{
    Session *ses;
    stdhash_it it;
    stdcarr_it c_it;
    UDP_Cell *u_cell;
    char *buff;
    int32 dummy_port;


    stdhash_find(&Sessions_Sock, &it, &sk);
    if(stdhash_it_is_end(&it)) {
        Alarm(EXIT, "Session_Close(): socket does not exist\n");
    }
    ses = *((Session **)stdhash_it_val(&it));

    stdhash_erase(&it);

    stdhash_find(&Sessions_ID, &it, &(ses->sess_id));
    if(!stdhash_it_is_end(&it)) {
	stdhash_erase(&it);
    }
    else {
	Alarm(EXIT, "Session_Close(): invalid ID\n");
    }

    if(reason != PORT_IN_USE){
	dummy_port = (int32)ses->port;
	if(dummy_port != 0) {
	    stdhash_find(&Sessions_Port, &it, &dummy_port);
	    if(!stdhash_it_is_end(&it)) {
		stdhash_erase(&it);
	    }
	}
    }


    if(ses->client_stat == SES_CLIENT_ON) {
	if(ses->fd_flags & READ_DESC)
	    E_detach_fd(sk, READ_FD);
	
	if(ses->fd_flags & EXCEPT_DESC)
	    E_detach_fd(sk, EXCEPT_FD);
	
	if(ses->fd_flags & WRITE_DESC)
	    E_detach_fd(sk, WRITE_FD);
	
	DL_close_channel(sk);


	while(!stdcarr_empty(&ses->udp_deliver_buff)) {
	    stdcarr_begin(&ses->udp_deliver_buff, &c_it);
	    
	    u_cell = *((UDP_Cell **)stdcarr_it_val(&c_it));
	    buff = u_cell->buff;
	    
	    dec_ref_cnt(buff);
	    dispose(u_cell);
	    stdcarr_pop_front(&ses->udp_deliver_buff);
	}
	stdcarr_destruct(&ses->udp_deliver_buff);
    }

    if(ses->data != NULL)
	dec_ref_cnt(ses->data);


    if(ses->r_data != NULL) {
	Close_Reliable_Session(ses); 	
    }

    dispose(ses);

    Alarm(PRINT, "session closed: %d\n", sk);
}



/***********************************************************/
/* int Process_Session_Packet(Session *ses)                */ 
/*                                                         */
/* Processes a packet from a client                        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:      the session defining the client               */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) status of the packet (see udp.h)                  */
/*                                                         */
/***********************************************************/

int Process_Session_Packet(Session *ses)
{
    udp_header *hdr; 
    udp_header *cmd;
    Node* next_hop;
    int ret;
    int32 dummy_port;
    stdhash_it it;
    int32 *type;
    
    /* Process the packet */
    hdr = (udp_header*)(ses->data);

    type = (int32*)(ses->data + sizeof(udp_header)); 
    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));

    if((hdr->len == 0) && (hdr->source == 0) && (hdr->dest == 0)) {
	/* Session command */
	type = (int32*)(ses->data + sizeof(udp_header)); 

	if(*type == BIND_TYPE_MSG) {
	    /* spines_bind() */

	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    ses->port = cmd->dest_port;
	    dummy_port = ses->port;
	    
	    Alarm(PRINT, "Accepted bind for port: %d\n", dummy_port);
	    
	    stdhash_find(&Sessions_Port, &it, &dummy_port);
	    if(!stdhash_it_is_end(&it)) {
		Alarm(PRINT, "Session_Read(): port already exists\n");
		Session_Close(ses->sk, PORT_IN_USE);
		return(NO_BUFF);
	    }
	    
	    stdhash_insert(&Sessions_Port, &it, &dummy_port, &ses);
	    
	    ses->total_len = 2;
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == CONNECT_TYPE_MSG) {
	    /* spines_connect() */

	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    ret = Init_Reliable_Connect(ses, cmd->dest, cmd->dest_port); 
	    
	    if(ret == -1) {
		Alarm(PRINT, "Session_Read(): No ports available\n");
		Session_Close(ses->sk, PORT_IN_USE);
		return(NO_BUFF);		
	    }

	    ses->total_len = 2;
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == LISTEN_TYPE_MSG) {
	    /* spines_listen() */

	    ses->type = LISTEN_SES_TYPE;
	    
	    ses->total_len = 2;
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == ACCEPT_TYPE_MSG) {
	    /* spines_accept() */
	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));

	    ret = Accept_Rel_Session(ses, cmd, ses->data+2*sizeof(udp_header)+sizeof(int32));

	    ses->total_len = 2;
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else {
	    Alarm(EXIT, "Session unknown command: %X\n", *type);
	}
    }

    /* Ok, this is data */

    if(ses->r_data != NULL) {
	/* This is Reliable UDP Data */

	ret = Process_Reliable_Session_Packet(ses);
	return(ret);	
    }
    else {
	/* This is UDP Data*/

	hdr->source = My_Address;
	hdr->source_port = ses->port;
	
	if(hdr->len + sizeof(udp_header) == ses->total_len) {
	    if(hdr->dest == My_Address) {
		ret = Deliver_UDP_Data(ses->data, ses->total_len, 0);
		return(ret);
	    }
	    else {
		next_hop = Get_Route(hdr->dest);
		if(next_hop != NULL) {
		    if(Reliable_Flag == 1) {
			ret = Forward_Rel_UDP_Data(next_hop, ses->data, 
						   ses->total_len, 0);
		    }
		    else {
			ret = Forward_UDP_Data(next_hop, ses->data, 
					       ses->total_len);
		    }
		    return(ret);
		}
		else {
		    return(NO_ROUTE);
		}
	    }
	}
	else {
	    Alarm(EXIT, "Process_udp_data_packet: Packed data... not available yet\n");
	}
    }
    return(-1); /* The flow will not reach here anyway... */
}



/***********************************************************/
/* int Deliver_UDP_Data(char* buff, int16u len,            */
/*                      int32u type)                       */ 
/*                                                         */
/* Delivers UDP data to the application                    */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* buff: pointer to the UDP packet                         */
/* len:  length of the packet                              */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) status of the packet (see udp.h)                  */
/*                                                         */
/***********************************************************/

int Deliver_UDP_Data(char* buff, int16u len, int32u type) {
    UDP_Cell *u_cell;
    sys_scatter scat;
    udp_header *hdr;
    stdhash_it h_it;
    Session *ses;
    int16 total_bytes;
    int ret;
    int32 dummy_port;
 

    hdr = (udp_header*)buff;
    dummy_port = (int32)hdr->dest_port;
    stdhash_find(&Sessions_Port, &h_it, &dummy_port);

    if(stdhash_it_is_end(&h_it)) {
	return(NO_ROUTE);
    }
    
    ses = *((Session **)stdhash_it_val(&h_it));

    if(ses->type == RELIABLE_SES_TYPE) {
	/* This is a reliable session */
	ret = Deliver_Rel_UDP_Data(buff, len, type);

	return(ret);
    }

    /* This is either a regular UDP message or a connect request for Listen/Accept */ 

    if(ses->type == LISTEN_SES_TYPE) {
	/* This is a connect request as it addresses a listen session */
	/* Check whether is a double request. If not, the message will be delivered to 
	 * the client for an accept. */

	if(Check_Double_Connect(buff, len, type)) {
	    return(BUFF_DROP);
	}
    }

    if(ses->client_stat == SES_CLIENT_OFF)
	return(BUFF_DROP);

    if(!stdcarr_empty(&ses->udp_deliver_buff)) {
	if(stdcarr_size(&ses->udp_deliver_buff) >= MAX_BUFF_SESS) {
	    return(BUFF_DROP);
	}
	
	if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
	    Alarm(EXIT, "Deliver_UDP_Data(): Cannot allocte udp cell\n");
	}
	u_cell->len = len;
	u_cell->buff = buff;
	stdcarr_push_back(&ses->udp_deliver_buff, &u_cell);
	inc_ref_cnt(buff);
	return(BUFF_OK);
    }
    

    /* If I got up to here, the buffer is empty. 
     * Let's see if we can send this packet */

    total_bytes = 2 + len;
    ses->sent_bytes = 0;
    while(ses->sent_bytes < total_bytes) {
	if(ses->sent_bytes < 2) {
	    scat.num_elements = 2;
	    scat.elements[0].len = 2 - ses->sent_bytes;
	    scat.elements[0].buf = ((char*)(&len)) + ses->sent_bytes;
	    scat.elements[1].len = len;
	    scat.elements[1].buf = buff;
	}
	else {
	    scat.num_elements = 1;
	    scat.elements[0].len = len - (ses->sent_bytes - 2);
	    scat.elements[0].buf = buff + (ses->sent_bytes - 2);
	}
	ret = DL_send(ses->sk,  My_Address, ses->port,  &scat );
	if(ret < 0) {
	    
#ifndef	ARCH_PC_WIN95
	    if((ret == -1)&&
	       ((errno == EWOULDBLOCK)||(errno == EAGAIN))) {
#else
	    if((ret == -1)&&
			((errno == WSAEWOULDBLOCK)||(errno == EAGAIN))) {
#endif	
			
		if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
		    Alarm(EXIT, "Deliver_UDP_Data(): Cannot allocte udp cell\n");
		}
		u_cell->len = len;
		u_cell->buff = buff;
		stdcarr_push_back(&ses->udp_deliver_buff, &u_cell);
		inc_ref_cnt(buff);
	
		E_attach_fd(ses->sk, WRITE_FD, Session_Write, ses->sess_id, 
			    NULL, HIGH_PRIORITY );
		ses->fd_flags = ses->fd_flags | WRITE_DESC;
		return(BUFF_OK);
	    } 
	    else {
		if(ses->r_data == NULL) {
		    Session_Close(ses->sk, SOCK_ERR);
		}		
		else {
		    Disconnect_Reliable_Session(ses);
		}
		return(BUFF_DROP);
	    }
	}
	if(ret == 0) {
	    Alarm(PRINT, "Error: ZERO write 1; sent: %d, total: %d\n",
		  ses->sent_bytes, total_bytes);

	}
	ses->sent_bytes += ret;
    }
    ses->sent_bytes = 0;
    return(BUFF_EMPTY);
}




/***********************************************************/
/* void Session_Write(int sk, int sess_id, void* dummy_p)  */ 
/*                                                         */
/* Called by the event system when the socket is again     */
/* ready for writing (after it was blocked)                */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      socket to the application                      */
/* sess_id: id of the session                              */
/* dummy_p: not used                                       */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Session_Write(int sk, int sess_id, void *dummy_p)
{
    UDP_Cell *u_cell;
    sys_scatter scat;
    stdcarr_it it;
    stdhash_it h_it;
    int16u len;
    Session *ses;
    char *buff;
    int16 total_bytes;
    int ret;

    
    /* Find the session. This is not the session that I read packets from,
     * but rather the one I'm trying to write data to. */

    stdhash_find(&Sessions_Sock, &h_it, &sk);
    if(stdhash_it_is_end(&h_it)) {
	return;
    }    
    ses = *((Session **)stdhash_it_val(&h_it));
    
    if(ses->sess_id != sess_id) {
	/* There's another session here, and it just uses the same socket */
	return;
    }

    if(ses->fd_flags & WRITE_DESC) {
	E_detach_fd(sk, WRITE_FD);
	ses->fd_flags = ses->fd_flags ^ WRITE_DESC;
    }
    else{
	Alarm(EXIT, "Session_Write():socket was not set for WRITE_FD\n");
    }

    if(ses->client_stat == SES_CLIENT_OFF)
	return;


    while(!stdcarr_empty(&ses->udp_deliver_buff)) {
	stdcarr_begin(&ses->udp_deliver_buff, &it);
	
	u_cell = *((UDP_Cell **)stdcarr_it_val(&it));
	buff = u_cell->buff;
	get_ref_cnt(buff);
	len = u_cell->len;

	total_bytes = 2 + len;
	while(ses->sent_bytes < total_bytes) {
	    if(ses->sent_bytes < 2) {
		scat.num_elements = 2;
		scat.elements[0].len = 2 - ses->sent_bytes;
		scat.elements[0].buf = ((char*)(&len)) + ses->sent_bytes;
		scat.elements[1].len = len;
		scat.elements[1].buf = buff;
	    }
	    else {
		scat.num_elements = 1;
		scat.elements[0].len = len - (ses->sent_bytes - 2);
		scat.elements[0].buf = buff + (ses->sent_bytes - 2);
	    }
	    ret = DL_send(ses->sk,  My_Address, ses->port,  &scat );
	    if(ret < 0) {

#ifndef	ARCH_PC_WIN95
		if((ret == -1)&&
		   ((errno == EWOULDBLOCK)||(errno == EAGAIN))) 
#else
		if((ret == -1)&&
		   ((errno == WSAEWOULDBLOCK)||(errno == EAGAIN))) 
#endif
		{
		    E_attach_fd(ses->sk, WRITE_FD, Session_Write, 0, 
				NULL, HIGH_PRIORITY );
		    ses->fd_flags = ses->fd_flags | WRITE_DESC;
		    return;
		} 
		else {
		    if(ses->r_data == NULL) {
			Session_Close(ses->sk, SOCK_ERR);
		    }		
		    else {
			Disconnect_Reliable_Session(ses);
		    }
		    return;
		}
	    }
	    if(ret == 0) {
		
		Alarm(PRINT, "Error: ZERO write 2; sent: %d, total: %d\n",
		      ses->sent_bytes, total_bytes);
		return;
	    }

	    ses->sent_bytes += ret;
	}
	dec_ref_cnt(buff);
	dispose(u_cell);
	stdcarr_pop_front(&ses->udp_deliver_buff);
	ses->sent_bytes = 0;
    }

    /* Check if this is a disconnected reliable session */
    if(ses->r_data != NULL) {
	if(ses->r_data->connect_state == DISCONNECT_LINK) {
	    if(ses->r_data->recv_tail == ses->r_data->last_seq_sent) {
		if(stdcarr_empty(&ses->udp_deliver_buff)) {
		    Disconnect_Reliable_Session(ses);
		}
	    }
	}
    }
}
 
 
void Block_Session(struct Session_d *ses) 
{
    int ret, ioctl_cmd;

    /* set file descriptor to blocking */
    ioctl_cmd = 0;
#ifdef ARCH_PC_WIN95
    ret = ioctlsocket(ses->sk, FIONBIO, &ioctl_cmd);
#else
    ret = ioctl(ses->sk, FIONBIO, &ioctl_cmd);
#endif

    if(ses->fd_flags & READ_DESC) {
	E_detach_fd(ses->sk, READ_FD);
	ses->fd_flags = ses->fd_flags ^ READ_DESC;
    }
    
    if(ses->fd_flags & EXCEPT_DESC) {
	E_detach_fd(ses->sk, EXCEPT_FD);
	ses->fd_flags = ses->fd_flags ^ EXCEPT_DESC;
    }

    /* set file descriptor to non blocking */
    ioctl_cmd = 1;
#ifdef ARCH_PC_WIN95
    ret = ioctlsocket(ses->sk, FIONBIO, &ioctl_cmd);
#else
    ret = ioctl(ses->sk, FIONBIO, &ioctl_cmd);
#endif

}


void Block_All_Sessions(void) {
    stdhash_it it;
    Session *ses;

    Alarm(DEBUG, "Block_All_Sessions\n");

    stdhash_begin(&Sessions_ID, &it); 
    while(!stdhash_it_is_end(&it)) {
        ses = *((Session **)stdhash_it_val(&it));
	if(ses->rel_blocked == 0) {
	    Block_Session(ses);
	}
	stdhash_it_next(&it);
    }
}


void Resume_Session(struct Session_d *ses) 
{
    int ret, ioctl_cmd;

    /* set file descriptor to blocking */
    ioctl_cmd = 0;
#ifdef ARCH_PC_WIN95
    ret = ioctlsocket(ses->sk, FIONBIO, &ioctl_cmd);
#else
    ret = ioctl(ses->sk, FIONBIO, &ioctl_cmd);
#endif

    if(!(ses->fd_flags & READ_DESC)) {
	E_attach_fd(ses->sk, READ_FD, Session_Read, 0, NULL, HIGH_PRIORITY );
	ses->fd_flags = ses->fd_flags | READ_DESC;
    }
     
    if(!(ses->fd_flags & EXCEPT_DESC)) {
	E_attach_fd(ses->sk, EXCEPT_FD, Session_Read, 0, NULL, HIGH_PRIORITY );
	ses->fd_flags = ses->fd_flags | EXCEPT_DESC;
    }

    /* set file descriptor to non blocking */
    ioctl_cmd = 1;
#ifdef ARCH_PC_WIN95
    ret = ioctlsocket(ses->sk, FIONBIO, &ioctl_cmd);
#else
    ret = ioctl(ses->sk, FIONBIO, &ioctl_cmd);
#endif

}


void Resume_All_Sessions(void) {
    stdhash_it it;
    Session *ses;

    Alarm(DEBUG, "Resume_All_Sessions\n");

    stdhash_begin(&Sessions_ID, &it); 
    while(!stdhash_it_is_end(&it)) {
        ses = *((Session **)stdhash_it_val(&it));
	if(ses->rel_blocked == 0) {
	    Resume_Session(ses);
	}

	stdhash_it_next(&it);
    }
}

