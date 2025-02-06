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
 * Copyright (c) 2003 - 2008 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
 *
 */


#include "util/arch.h"


#ifndef	ARCH_PC_WIN95

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#else

#include <winsock.h>

#endif

#ifdef ARCH_SPARC_SOLARIS
#include <unistd.h>
#include <stropts.h>
#endif

#ifndef _WIN32_WCE
#include <errno.h>
#endif

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
#include "reliable_datagram.h"
#include "link_state.h"
#include "hello.h"
#include "udp.h"
#include "reliable_udp.h"
#include "realtime_udp.h"
#include "protocol.h"
#include "route.h"
#include "session.h"
#include "reliable_session.h"
#include "state_flood.h"
#include "multicast.h"

/* Global variables */

extern int16     Port;
extern int32     My_Address;
extern stdhash   Sessions_ID;
extern stdhash   Sessions_Port;
extern stdhash   Rel_Sessions_Port;
extern stdhash   Sessions_Sock;
extern int16     Link_Sessions_Blocked_On;
extern stdhash   All_Groups_by_Node;
extern stdhash   All_Groups_by_Name;
extern stdhash   All_Nodes;
extern stdhash   Neighbors;
extern stdhash   Monitor_Params;
extern int       Accept_Monitor;
extern int       Unicast_Only;
extern channel   Ses_UDP_Send_Channel;
extern channel   Ses_UDP_Recv_Channel;


/* Local variables */

static int32u   Session_Num;
static const sp_time zero_timeout  = {     0,    0};
static int last_sess_port;
static sys_scatter Ses_UDP_Pack;
static char *frag_buf[55];
static channel ctrl_sk_requests[MAX_CTRL_SK_REQUESTS];


#define FRAG_TTL         30



Frag_Packet* Delete_Frag_Element(Frag_Packet **head, Frag_Packet *frag_pkt)
{
    int i;
    Frag_Packet *frag_tmp;
    
    if(frag_pkt == NULL) {
	return NULL;
    }
    if(head == NULL) {
	return NULL;
    }


    for(i=0; i<frag_pkt->scat.num_elements; i++) {
	if(frag_pkt->scat.elements[i].buf != NULL) {
	    dec_ref_cnt(frag_pkt->scat.elements[i].buf);
	}
    }
    if(frag_pkt->prev == NULL) {
	*head = frag_pkt->next;
    }
    else {
	frag_pkt->prev->next = frag_pkt->next;
    }
    if(frag_pkt->next != NULL) {
	frag_pkt->next->prev = frag_pkt->prev;
    }
    
    frag_tmp = frag_pkt->next;
    dispose(frag_pkt);
    return(frag_tmp);
}





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
    int i, ret, val;

    Session_Num = 0;
    last_sess_port = 40000;

    for(i=0; i<50; i++) {
	frag_buf[i] = new_ref_cnt(PACK_BODY_OBJ);
	if(frag_buf[i] == NULL) {
	    Alarm(EXIT, "Init_Session: Cannot allocate memory\n");
	}
    }


    stdhash_construct(&Sessions_ID, sizeof(int32), sizeof(Session*),
		      NULL, NULL, 0);

    stdhash_construct(&Sessions_Port, sizeof(int32), sizeof(Session*),
		      NULL, NULL, 0);

    stdhash_construct(&Rel_Sessions_Port, sizeof(int32), sizeof(Session*),
		      NULL, NULL, 0);

    stdhash_construct(&Sessions_Sock, sizeof(int32), sizeof(Session*),
		      NULL, NULL, 0);

    stdhash_construct(&Neighbors, sizeof(int32), sizeof(Node*),
		      NULL, NULL, 0);


    sk_local = socket(AF_INET, SOCK_STREAM, 0);
    if (sk_local<0) {
      Alarm(EXIT, "Int_Session(): socket failed\n");
    }

    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(My_Address);
    name.sin_port = htons((int16)(Port+SESS_PORT));


   val = 1;
   if(setsockopt(sk_local, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val)))
   {
#ifndef _WIN32_WCE
       Alarm( EXIT, "Init_Session: Failed to set socket option REUSEADDR, errno: %d\n", errno);
#else
       Alarm( EXIT, "Init_Session: Failed to set socket option REUSEADDR, errno: %d\n", WSAGetLastError());
#endif
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

    E_attach_fd(sk_local, READ_FD, Session_Accept, SESS_PORT, NULL, HIGH_PRIORITY );

    /* For Datagram sockets */

    Ses_UDP_Send_Channel = DL_init_channel(SEND_CHANNEL, 
					   (int16)(Port+SESS_UDP_PORT), 0, 0);
    Ses_UDP_Recv_Channel = DL_init_channel(RECV_CHANNEL, 
					   (int16)(Port+SESS_UDP_PORT), 0, My_Address);
    
    E_attach_fd(Ses_UDP_Recv_Channel, READ_FD, Session_UDP_Read, 0, 
		    NULL, LOW_PRIORITY );


    /* For Control socket */

    sk_local = socket(AF_INET, SOCK_STREAM, 0);
    if (sk_local<0) {
      Alarm(EXIT, "Int_Session(): socket failed\n");
    }

    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(My_Address);
    name.sin_port = htons((int16)(Port+SESS_CTRL_PORT));

    val = 1;
    if(setsockopt(sk_local, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val)))
    {
#ifndef _WIN32_WCE
       Alarm( EXIT, "Init_Session: Failed to set socket option REUSEADDR, errno: %d\n", errno);
#else
       Alarm( EXIT, "Init_Session: Failed to set socket option REUSEADDR, errno: %d\n", WSAGetLastError());
#endif
    }

    ret = bind(sk_local, (struct sockaddr *) &name, sizeof(name));
    if (ret == -1) {
	Alarm(EXIT, "Init_Session: bind error for port %d\n",Port+SESS_CTRL_PORT);
    }

    if(listen(sk_local, 4) < 0) {
	Alarm(EXIT, "Session_Init(): Listen failure\n");
    }

    Alarm(DEBUG, "listen successful on socket: %d\n", sk_local);

    E_attach_fd(sk_local, READ_FD, Session_Accept, SESS_CTRL_PORT, NULL, LOW_PRIORITY );

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
/* port: port number of incomming request                  */
/* dummy_p: not used                                       */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Session_Accept(int sk_local, int port, void *dummy_p)
{
    struct sockaddr_in acc_sin;
    u_int acc_sin_len = sizeof(acc_sin);
    u_int val, lenval, ioctl_cmd;
    channel sk;
    Session *ses;
    stdit it;
    int ret, i, tot_bytes;
    int32 endianess_type;

    sk = accept(sk_local, (struct sockaddr*)&acc_sin, &acc_sin_len);

    /* Increasing the buffer on the socket */
    for(i=10; i <= 200; i+=5) {
	val = 1024*i;

	ret = setsockopt(sk, SOL_SOCKET, SO_SNDBUF, (void *)&val, sizeof(val));
	if (ret < 0) break;

	ret = setsockopt(sk, SOL_SOCKET, SO_RCVBUF, (void *)&val, sizeof(val));
	if (ret < 0) break;

	lenval = sizeof(val);
	ret = getsockopt(sk, SOL_SOCKET, SO_SNDBUF, (void *)&val,  &lenval);
	if(val < i*1024) break;
	Alarm(DEBUG, "Sess_accept: set sndbuf %d, ret is %d\n", val, ret);

	lenval = sizeof(val);
	ret= getsockopt(sk, SOL_SOCKET, SO_RCVBUF, (void *)&val, &lenval);
	if(val < i*1024 ) break;
	Alarm(DEBUG, "Sess_accept: set rcvbuf %d, ret is %d\n", val, ret);
    }
    Alarm(DEBUG, "Sess_accept: set sndbuf/rcvbuf to %d\n", 1024*(i-5));


    /* Setting no delay option on the socket */
    val = 1;
    if (setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val))) {
	    Alarm(PRINT, "Session_Accept: Failed to set TCP_NODELAY\n");
    }

    /* set file descriptor to non blocking */
    ioctl_cmd = 1;

#ifdef ARCH_PC_WIN95
    ret = ioctlsocket(sk, FIONBIO, (void*) &ioctl_cmd);
#else
    ret = ioctl(sk, FIONBIO, &ioctl_cmd);
#endif

    /* If this is a Control channel setup, just store socket in a 
       safe place, and send socket to the client so that it can
       tell me how to link it's session to the appropiate control channel */
    if (port == SESS_CTRL_PORT) {
        tot_bytes = 0;
        while(tot_bytes < sizeof(int32)) {
            ret = send(sk, ((char*)(&sk))+tot_bytes, sizeof(int32)-tot_bytes, 0);
	    tot_bytes += ret;
        } 
        if(tot_bytes != sizeof(int32)) {
            close(sk);
        }
        for (i=0; i<MAX_CTRL_SK_REQUESTS; i++) {
            if (ctrl_sk_requests[i] == 0) {
                ctrl_sk_requests[i] = sk;
                break;
            }
        }
        if (i == MAX_CTRL_SK_REQUESTS) {
            Alarm(EXIT, "Session_Accept(): Too many in-progress requests in parallel\n");
        }
        return;
    }

    if((ses = (Session*) new(SESSION_OBJ))==NULL) {
	Alarm(EXIT, "Session_Accept(): Cannot allocte session object\n");
    }

    ses->sess_id = Session_Num++;
    ses->type = UDP_SES_TYPE;
    ses->endianess_type = 0;
    ses->sk = sk;
    ses->ctrl_sk = 0;
    ses->port = 0;
    ses->read_len = sizeof(int32);
    ses->partial_len = 0;
    ses->state = READY_ENDIAN;
    ses->r_data = NULL;
    ses->rel_blocked = 0;
    ses->client_stat = SES_CLIENT_ON;
    ses->udp_port = -1;
    ses->recv_fd_flag = 0;
    ses->fd = -1;
    ses->multicast_loopback = 1;

    if((ses->data = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
	Alarm(EXIT, "Session_Accept(): Cannot allocte packet_body object\n");
    }


    ses->frag_pkts = NULL;

    stdcarr_construct(&ses->rel_deliver_buff, sizeof(UDP_Cell*), 0);
    stdhash_construct(&ses->joined_groups, sizeof(int32), sizeof(Group_State*),
                      NULL, NULL, 0);


    /* Allocating a port for the session */
    for(i=last_sess_port+1; i < 60000; i++) {
	stdhash_find(&Sessions_Port, &it, &i);
	if(stdhash_is_end(&Sessions_Port, &it)) {
	    break;
	}
    }
    if(i == 60000) {
	for(i= 40000; i < last_sess_port; i++) {
	    stdhash_find(&Sessions_Port, &it, &i);
	    if(stdhash_is_end(&Sessions_Port, &it)) {
		break;
	    }
	}
	if(i == last_sess_port) {
	    Alarm(EXIT, "Session: No more ports for the session\n");
	}
    }

    last_sess_port = i;

    ses->port = (int16u)i;
    stdhash_insert(&Sessions_Port, &it, &i, &ses);
    stdhash_insert(&Sessions_Sock, &it, &sk, &ses);
    stdhash_insert(&Sessions_ID, &it, &(ses->sess_id), &ses);

    Alarm(PRINT, "new session...%d\n", sk);

    E_attach_fd(ses->sk, READ_FD, Session_Read, 0, NULL, HIGH_PRIORITY );
    E_attach_fd(ses->sk, EXCEPT_FD, Session_Read, 0, NULL, HIGH_PRIORITY );

    ses->fd_flags = READ_DESC | EXCEPT_DESC;

    /* Send the endianess to the session */
    endianess_type = Set_endian(0);

    tot_bytes = 0;
    while(tot_bytes < sizeof(int32)) {
	ret = send(sk, ((char*)(&endianess_type))+tot_bytes, sizeof(int32)-tot_bytes, 0);
	tot_bytes += ret;
    }   
    if(tot_bytes != sizeof(int32)) {
	Session_Close(ses->sess_id, SOCK_ERR);
    }

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
    udp_header *u_hdr;
    rel_udp_pkt_add *r_add;
    Session *ses;
    stdit it;
    int received_bytes;
    int ret, add_size, i;

    stdhash_find(&Sessions_Sock, &it, &sk);
    if(stdhash_is_end(&Sessions_Sock, &it)) {
        Alarm(PRINT, "Session_Read(): socket does not exist\n");
	return;
    }
    ses = *((Session **)stdhash_it_val(&it));

    scat.num_elements = 1;
    scat.elements[0].len = ses->read_len - ses->partial_len;
    scat.elements[0].buf = (char*)(ses->data + ses->partial_len);

    received_bytes = DL_recv(ses->sk, &scat);

    if(received_bytes <= 0) {

	Alarm(DEBUG, "\nsocket err; len: %d; read_len: %d; partial_len: %d; STATE: %d\n",  
	      scat.elements[0].len, ses->read_len, ses->partial_len, ses->state);

	/* This is non-blocking socket. Not all the errors are treated as
	 * a disconnect. */
	if(received_bytes == -1) {
#ifndef	ARCH_PC_WIN95
	    if((errno == EWOULDBLOCK)||(errno == EAGAIN))
#else
#ifndef _WIN32_WCE
	    if((errno == WSAEWOULDBLOCK)||(errno == EAGAIN))
#else
	    int sk_errno = WSAGetLastError();
	    if((sk_errno == WSAEWOULDBLOCK)||(sk_errno == EAGAIN))
#endif /* Windows CE */
#endif
	    {
		Alarm(DEBUG, "EAGAIN - Session_Read()\n");
		return;
	    }
	    else {
		if(ses->r_data == NULL) {
		    Session_Close(ses->sess_id, SOCK_ERR);
		}
		else {
		    Disconnect_Reliable_Session(ses);
		}
	    }
	}
	else {
	    if(ses->r_data == NULL) {
		Session_Close(ses->sess_id, SOCK_ERR);
	    }
	    else {
		Disconnect_Reliable_Session(ses);
	    }
	}
	return;
    }

    if(received_bytes + ses->partial_len > ses->read_len)
	Alarm(EXIT, "Session_Read(): Too many bytes...\n");
    
    if(ses->r_data != NULL) {
	add_size = sizeof(rel_udp_pkt_add);
    }
    else {
	add_size = 0;
    }

    /*
     *Alarm(DEBUG, "* received_bytes: %d; partial_len: %d; read_len: %d; STATE: %d\n",
     *	  received_bytes, ses->partial_len, ses->read_len, ses->state);
     */

    if(received_bytes + ses->partial_len < ses->read_len) {
	ses->partial_len += received_bytes;
    }
    else {
	if(ses->state == READY_ENDIAN) {
	    ses->endianess_type = *((int32*)(ses->data));

	    ses->received_len = 0;
	    ses->read_len = sizeof(int32);
	    ses->partial_len = 0;
	    ses->state = READY_CTRL_SK;
	}
	else if(ses->state == READY_CTRL_SK) {
            ses->ctrl_sk = *((int32*)(ses->data));
            for (i=0; i<MAX_CTRL_SK_REQUESTS; i++) {
                if (ctrl_sk_requests[i] == ses->ctrl_sk) {
                    ctrl_sk_requests[i] = 0;
                    break;
                }
            }
            if (i == MAX_CTRL_SK_REQUESTS) {
                Alarm(EXIT, "Session_Read(): No such control channel: %d\n", ses->ctrl_sk);
            }
            Alarm(PRINT, "linked Spines Socket Channel %d with Control Channel %d\n", ses->sk, ses->ctrl_sk);
	    ses->state = READY_LEN;
        }
	else if(ses->state == READY_LEN) {
	    ses->total_len = *((int32*)(ses->data));
	    if(!Same_endian(ses->endianess_type)) {
		ses->total_len = Flip_int32(ses->total_len);
	    }

	    if(ses->total_len > MAX_SPINES_MSG + sizeof(udp_header) + add_size) {
		ses->read_len = MAX_SPINES_MSG + sizeof(udp_header) + add_size;
	    }
	    else {
		ses->read_len = ses->total_len;
	    }
	    
	    ses->partial_len = 0;
	    ses->received_len = 0;
	    ses->seq_no++;
	    if(ses->seq_no >= 10000) {
		ses->seq_no = 0;
	    }
	    ses->frag_num = (ses->total_len-sizeof(udp_header)-add_size)/MAX_SPINES_MSG;
	    if((ses->total_len-sizeof(udp_header)-add_size)%MAX_SPINES_MSG != 0) {
		ses->frag_num++;
	    }
	    ses->frag_idx = 0;
	    ses->state = READY_DATA;
	}
	else if(ses->state == READY_DATA) {
	    u_hdr = (udp_header*)ses->data;
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(u_hdr);
	    }
	    if(ses->frag_num > 1) {
		if(ses->frag_idx == 0) {
		    memcpy((void*)(&ses->save_hdr), (void*)u_hdr, sizeof(udp_header));
		}
		u_hdr->len = ses->read_len - sizeof(udp_header);
		
		if(ses->r_data != NULL) {
		    r_add = (rel_udp_pkt_add*)(ses->data + sizeof(udp_header));
		    r_add->type = Set_endian(0);
		    r_add->data_len = u_hdr->len - sizeof(rel_udp_pkt_add);
		    r_add->ack_len = 0;
		}
	    }
	    u_hdr->seq_no = ses->seq_no;
	    u_hdr->frag_num = (int16u)ses->frag_num;
	    u_hdr->frag_idx = (int16u)ses->frag_idx;
	    u_hdr->sess_id = (int16u)(ses->sess_id & 0x0000ffff);

	    ses->received_len += ses->read_len;
	    if(ses->frag_idx > 0) {
		ses->received_len -= sizeof(udp_header)+add_size;
	    }
	    ses->frag_idx++;


	    ret = Process_Session_Packet(ses);

	    if(ret == NO_BUFF){
		return;
	    }

	    if(get_ref_cnt(ses->data) > 1) {
		dec_ref_cnt(ses->data);
		if((ses->data = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
		    Alarm(EXIT, "Session_Read(): Cannot allocte packet_body\n");
		    
		}
	    }

	    if(ses->frag_idx == ses->frag_num) {
		ses->read_len = sizeof(int32);
		ses->partial_len = 0;
		ses->state = READY_LEN;
	    }
	    else {
		ses->read_len = ses->total_len - ses->received_len;
		if(ses->read_len > MAX_SPINES_MSG) {
		    ses->read_len = MAX_SPINES_MSG;
		}
		/*
		 *Alarm(PRINT, "TOT total: %d; received: %d; read: %d\n",
		 *     ses->total_len, ses->received_len, ses->read_len);
		 */
		memcpy((void*)(ses->data), (void*)(&ses->save_hdr), sizeof(udp_header));
		ses->read_len += sizeof(udp_header);
		ses->partial_len = sizeof(udp_header);
		if(ses->r_data != NULL) {
		    ses->read_len += sizeof(rel_udp_pkt_add);
		    ses->partial_len += sizeof(rel_udp_pkt_add);
		}
		ses->state = READY_DATA;
	    }
	}
    }
}



/***********************************************************/
/* void Session_Close(int sesid, int reason)               */
/*                                                         */
/* Closes a session                                        */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sesid:  the session id                                  */
/* reason: see session.h                                   */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Session_Close(int sesid, int reason)
{
    Session *ses;
    stdit it;
    stdit c_it;
    UDP_Cell *u_cell;
    char *buff;
    int32 dummy_port;
    Group_State *g_state;
    int cnt = 0;
    int i;
    Frag_Packet *frag_pkt;


    /* Get the session */
    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_is_end(&Sessions_ID, &it)) {
        Alarm(PRINT, "Session_Close(): session does not exist: %d\n", sesid);
	return;
    }
    ses = *((Session **)stdhash_it_val(&it));

    ses->close_reason = reason;

    /* Detach the socket so it won't bother us */
    if(ses->client_stat == SES_CLIENT_ON) {
	ses->client_stat = SES_CLIENT_OFF;

	if(ses->fd_flags & READ_DESC)
	    E_detach_fd(ses->sk, READ_FD);

	if(ses->fd_flags & EXCEPT_DESC)
	    E_detach_fd(ses->sk, EXCEPT_FD);

	if(ses->fd_flags & WRITE_DESC)
	    E_detach_fd(ses->sk, WRITE_FD);


	while(!stdcarr_empty(&ses->rel_deliver_buff)) {
	    stdcarr_begin(&ses->rel_deliver_buff, &c_it);

	    u_cell = *((UDP_Cell **)stdcarr_it_val(&c_it));
	    buff = u_cell->buff;

	    dec_ref_cnt(buff);
	    dispose(u_cell);
	    stdcarr_pop_front(&ses->rel_deliver_buff);
	}
	stdcarr_destruct(&ses->rel_deliver_buff);

	stdhash_find(&Sessions_Sock, &it, &ses->sk);
	if(!stdhash_is_end(&Sessions_Sock, &it)) {
	    stdhash_erase(&Sessions_Sock, &it);
	}
    }

    /* Dispose the receiving buffer */
    if(ses->data != NULL) {
	dec_ref_cnt(ses->data);
	ses->data = NULL;
    }

    /* Dispose all the incomplete fragmented packets */
    while(ses->frag_pkts != NULL) {
	frag_pkt = ses->frag_pkts;
	ses->frag_pkts = ses->frag_pkts->next;
	for(i=0; i<frag_pkt->scat.num_elements; i++) {
	    if(frag_pkt->scat.elements[i].buf != NULL) {
		dec_ref_cnt(frag_pkt->scat.elements[i].buf);
	    }
	}
	dispose(frag_pkt);
    }

    /* Remove the reliability data structures */
    if(ses->r_data != NULL) {
	Close_Reliable_Session(ses);
	ses->r_data = NULL;
    }

    /* Leave all the groups */
    stdhash_begin(&ses->joined_groups, &it);
    while(!stdhash_is_end(&ses->joined_groups, &it)) {
	g_state = *((Group_State **)stdhash_it_val(&it));
	/*Alarm(DEBUG, "Disconnect; Leaving group: %d.%d.%d.%d == %d\n",
	*/
        Alarm(PRINT, "Disconnect; Leaving group: %d.%d.%d.%d == %d\n",
	      IP1(g_state->dest_addr), IP2(g_state->dest_addr),
	      IP3(g_state->dest_addr), IP4(g_state->dest_addr), cnt);
	Leave_Group(g_state->dest_addr, ses);
	cnt++;
	if(cnt > 10) {
	    /* Too many groups to leave at once. Queue the function again  */
	    E_queue(Try_Close_Session, (int)ses->sess_id, NULL, zero_timeout);
	    return;
	}
        stdhash_begin(&ses->joined_groups, &it);
    }

    /* Left all the groups... */
    stdhash_destruct(&ses->joined_groups);


    stdhash_find(&Sessions_ID, &it, &(ses->sess_id));
    if(!stdhash_is_end(&Sessions_ID, &it)) {
	stdhash_erase(&Sessions_ID, &it);
    }
    else {
	Alarm(EXIT, "Session_Close(): invalid ID\n");
    }

    if(reason != PORT_IN_USE){
	dummy_port = (int32)ses->port;
	if(dummy_port != 0) {
	    stdhash_find(&Sessions_Port, &it, &dummy_port);
	    if(!stdhash_is_end(&Sessions_Port, &it)) {
		stdhash_erase(&Sessions_Port, &it);
	    }
	}
    }

    if(ses->client_stat != SES_CLIENT_ORPHAN) {
	/* Close the socket (now we can, since we won't access the session by socket) */
	Alarm(PRINT, "Session_Close: closing channel: %d\n", ses->sk);
	DL_close_channel(ses->sk);
	DL_close_channel(ses->ctrl_sk);
	Alarm(PRINT, "session closed: %d\n", ses->sk);
    }

    if(ses->recv_fd_flag == 1) {
	if(ses->fd != -1) {
	    close(ses->fd);
	}
    }

    /* Dispose the session */
    dispose(ses);
}




/***********************************************************/
/* int void Try_Close_Session(int sesid, void *dummy)      */
/*                                                         */
/* Calls Session_Close() again, until it leaves all groups */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sesid:    the id of the session                         */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Try_Close_Session(int sesid, void *dummy)
{
    Session *ses;
    stdit it;

    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_is_end(&Sessions_ID, &it)) {
	return;
    }
    ses = *((Session **)stdhash_it_val(&it));

    Session_Close(ses->sess_id, ses->close_reason);
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
    int32 *cmd_int, *pkt_len;
    Node *next_hop;
    int ret, tot_bytes;
    int32 dummy_port, dest_addr;
    stdit it, ngb_it;
    int32 *type;
    Group_State *g_state;
    Lk_Param lkp;
    stdhash *neighbors;
    spines_trace *spines_tr;
    char *buf;

    /* Process the packet */
    hdr = (udp_header*)(ses->data);

    type = (int32*)(ses->data + sizeof(udp_header));
    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
    cmd_int = (int32*)(ses->data + sizeof(udp_header)+sizeof(int32));

    if((hdr->len == 0) && (hdr->source == 0) && (hdr->dest == 0)) {
	/* Session command */
	type = (int32*)(ses->data + sizeof(udp_header));
	if(!Same_endian(ses->endianess_type)) {
	    *type = Flip_int32(*type);
	}

	if(*type == BIND_TYPE_MSG) {
	    /* spines_bind() */

	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(cmd);
	    }

	    if(cmd->dest_port == 0) {
		Alarm(PRINT, "\n!!! Session: you cannot bind on port 0 (zero)\n");
		Session_Close(ses->sess_id, PORT_IN_USE);
		return(NO_BUFF);
	    }
	    if(ses->type == LISTEN_SES_TYPE) {
		Alarm(PRINT, "Cannot bind on a listen session\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }
	    if(ses->r_data != NULL) {
		Alarm(PRINT, "\n!!! spines_bind(): session already connected\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }


	    /* Check whether the port is already used */
	    dummy_port = cmd->dest_port;
	    stdhash_find(&Sessions_Port, &it, &dummy_port);
	    if(!stdhash_is_end(&Sessions_Port, &it)) {
		Alarm(PRINT, "\n!!! Process_Session_Packet(): port already exists\n");
		Session_Close(ses->sess_id, PORT_IN_USE);
		return(NO_BUFF);
	    }

	    /* release the current port of the session */
	    dummy_port = ses->port;
	    stdhash_find(&Sessions_Port, &it, &dummy_port);
	    if(stdhash_is_end(&Sessions_Port, &it)) {
		Alarm(EXIT, "BIND: session does not have a port\n");
	    }
	    stdhash_erase(&Sessions_Port, &it);

	    ses->port = cmd->dest_port;
	    dummy_port = cmd->dest_port;
	    stdhash_insert(&Sessions_Port, &it, &dummy_port, &ses);

	    if(ses->udp_port != -1) {
		Ses_Send_ID(ses);
	    }

	    Alarm(PRINT, "Accepted bind for port: %d\n", dummy_port);

	    ses->read_len = sizeof(int32);
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == CONNECT_TYPE_MSG) {
	    /* spines_connect() */

	    if(ses->r_data != NULL) {
		Alarm(PRINT, "\n!!! spines_connect(): session already connected\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }
	    if(ses->type == LISTEN_SES_TYPE) {
		Alarm(PRINT, "Listen session\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }

	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(cmd);
	    }

	    if((cmd->dest & 0xF0000000) == 0xE0000000) {
		/* Multicast address */
		Alarm(PRINT, "Error: Connect to a Multicast address\n");
	    }
	    else {
		/* Reliable Connect */
		ret = Init_Reliable_Connect(ses, cmd->dest, cmd->dest_port);

		if(ret == -1) {
		    Alarm(PRINT, "Session_Read(): No ports available\n");
		    Session_Close(ses->sess_id, SES_DISCONNECT);
		    return(NO_BUFF);
		}
	    }
	    ses->read_len = sizeof(int32);
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == LISTEN_TYPE_MSG) {
	    /* spines_listen() */
	    if(ses->r_data != NULL) {
		Alarm(PRINT, "\n!!! spines_listen(): session already connected\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }
	    if(ses->type == LISTEN_SES_TYPE) {
		Alarm(PRINT, "This session already listens\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }

	    ses->type = LISTEN_SES_TYPE;

	    ses->read_len = sizeof(int32);
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == LINKS_TYPE_MSG) {
	    /* spines_socket() */

	    ses->links_used = *cmd_int;
	    ses->rnd_num  = *(cmd_int+1);
	    ses->udp_addr = *(cmd_int+2);
	    ses->udp_port = *(cmd_int+3);
	    if(!Same_endian(ses->endianess_type)) {
		ses->links_used = Flip_int32(ses->links_used);
		ses->rnd_num  = Flip_int32(ses->rnd_num);
		ses->udp_addr = Flip_int32(ses->udp_addr);
		ses->udp_port = Flip_int32(ses->udp_port);
	    }
	    ses->seq_no   = ses->rnd_num%MAX_PKT_SEQ;

	    Ses_Send_ID(ses);

	    ses->read_len = sizeof(int32);
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == SETLINK_TYPE_MSG) {
	    /* spines_setloss() */

	    if(Accept_Monitor == 1) {
		cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
		lkp.loss_rate = *(int32*)(ses->data + 2*sizeof(udp_header)+3*sizeof(int32));
		lkp.burst_rate = *(int32*)(ses->data + 2*sizeof(udp_header)+4*sizeof(int32));
		lkp.bandwidth = *(int32*)(ses->data + 2*sizeof(udp_header)+sizeof(int32));
		lkp.delay.sec = 0;
		lkp.delay.usec = *(int32*)(ses->data + 2*sizeof(udp_header)+2*sizeof(int32));
		lkp.was_loss = 0;
		
		if(!Same_endian(ses->endianess_type)) {
		    Flip_udp_hdr(cmd);
		    lkp.loss_rate = Flip_int32(lkp.loss_rate);
		    lkp.burst_rate = Flip_int32(lkp.burst_rate);
		    lkp.bandwidth = Flip_int32(lkp.bandwidth);
		    lkp.delay.usec = Flip_int32(lkp.delay.usec);
		}
		/* Delay is given in milliseconds */
		lkp.delay.usec *= 1000;


		lkp.bucket = BWTH_BUCKET;
		lkp.last_time_add = E_get_time();

		Alarm(PRINT, "\nSetting link params: bandwidth: %d; latency: %d; loss: %d; burst: %d; was_loss %d\n\n",
		      lkp.bandwidth, lkp.delay.usec, lkp.loss_rate, lkp.burst_rate, lkp.was_loss);

		stdhash_find(&Monitor_Params, &it, &cmd->dest);
		if (!stdhash_is_end(&Monitor_Params, &it)) {
		    stdhash_erase(&Monitor_Params, &it);
		}
		if((lkp.loss_rate > 0)||(lkp.delay.sec > 0)||
		   (lkp.delay.usec > 0)||(lkp.bandwidth > 0)) {
		    stdhash_insert(&Monitor_Params, &it, &cmd->dest, &lkp);
		}
	    }

	    return(BUFF_EMPTY);
	}
	else if(*type == FLOOD_SEND_TYPE_MSG) {
	    /* spines_flood_send() */
	    ses->Sendto_address = *(int32*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    ses->Sendto_port = *(int32*)(ses->data + sizeof(udp_header)+2*sizeof(int32));
	    ses->Rate        = *(int32*)(ses->data + sizeof(udp_header)+3*sizeof(int32));
	    ses->Packet_size = *(int32*)(ses->data + sizeof(udp_header)+4*sizeof(int32));
	    ses->Num_packets = *(int32*)(ses->data + sizeof(udp_header)+5*sizeof(int32));
	    ses->Sent_packets = 0;
	    ses->Start_time = E_get_time();
	    if(!Same_endian(ses->endianess_type)) {
		ses->Sendto_address = Flip_int32(ses->Sendto_address);
		ses->Sendto_port = Flip_int32(ses->Sendto_port);
		ses->Rate        = Flip_int32(ses->Rate);
		ses->Packet_size = Flip_int32(ses->Packet_size);
		ses->Num_packets = Flip_int32(ses->Num_packets);
	    }

	    Session_Flooder_Send(ses->sess_id, NULL);

	    return(BUFF_EMPTY);
	}
	else if(*type == FLOOD_RECV_TYPE_MSG) {
	    /* spines_flood_recv() */
	    ses->recv_fd_flag = 1;
	    ses->fd = open(ses->data+sizeof(udp_header)+2*sizeof(int32), O_WRONLY|O_CREAT|O_TRUNC, 00666);
	    if(ses->fd == -1) {
		Session_Close(ses->sess_id, SES_BUFF_FULL);
	    }
	    return(BUFF_EMPTY);
	}
	else if(*type == ACCEPT_TYPE_MSG) {
	    /* spines_accept() */
	    if(ses->r_data != NULL) {

		Alarm(PRINT, "\n!!! spines_accept(): session already connected\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }

	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(cmd);
	    }


	    ret = Accept_Rel_Session(ses, cmd, ses->data+2*sizeof(udp_header)+sizeof(int32));

	    ses->read_len = sizeof(int32);
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == JOIN_TYPE_MSG) {
	    /* spines_join() for spines_setsockopt() */
	    if(ses->r_data != NULL) {
		Alarm(PRINT, "\n!!! spines_join(): session already connected\n");
                Session_Close(ses->sess_id, SES_DISCONNECT); 
		return(NO_BUFF);
	    }
	    if(ses->type == LISTEN_SES_TYPE) {
		Alarm(PRINT, "Cannot join on a listen session\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }

	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(cmd);
	    }
	    dest_addr = cmd->dest;

	    ret = Join_Group(dest_addr, ses);
	    if(ret < 0) {
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }

	    ses->read_len = sizeof(int32);
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == LEAVE_TYPE_MSG) {
	    /* spines_leave() for spines_setsockopt() */
	    if(ses->r_data != NULL) {
		Alarm(PRINT, "\n!!! spines_leave(): session already connected\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }
	    if(ses->type == LISTEN_SES_TYPE) {
		Alarm(PRINT, "Cannot leave on a listen session\n");
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }

	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(cmd);
	    }
	    dest_addr = cmd->dest;
	    Alarm(PRINT,"LEAVE_TYPE_MESSAGE\n");
	    ret = Leave_Group(dest_addr, ses);
	    if(ret < 0) {
		Session_Close(ses->sess_id, SES_DISCONNECT);
		return(NO_BUFF);
	    }

	    ses->read_len = sizeof(int32);
	    ses->partial_len = 0;
	    ses->state = READY_LEN;
	    return(BUFF_EMPTY);
	}
	else if(*type == LOOP_TYPE_MSG) {
	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(cmd);
	    }
	    ses->multicast_loopback = (char)(cmd->dest);
	    return(BUFF_EMPTY);
        }
	else if(*type == ADD_NEIGHBOR_MSG) {
	    /* spines_add_neighbor() for spines_ioctl() */
	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(cmd);
	    }
	    dest_addr = cmd->dest; 
	    /* Fake a hello ping to initialize connection */ 
	    Process_hello_ping_packet(cmd->dest, 0);
	    return(BUFF_EMPTY);
	}
	else if(*type == TRACEROUTE_TYPE_MSG || 
            *type == EDISTANCE_TYPE_MSG  || 
            *type == MEMBERSHIP_TYPE_MSG ) { 

	    cmd = (udp_header*)(ses->data + sizeof(udp_header)+sizeof(int32));
	    if(!Same_endian(ses->endianess_type)) {
		Flip_udp_hdr(cmd);
	    }
	    dest_addr = cmd->dest;

	    buf = (char *) new_ref_cnt(PACK_BODY_OBJ); 
	    if(buf == NULL) { 
		Alarm(EXIT, "Session_UDP_Read: Cannot allocate buffer\n"); 
	    } 
            pkt_len = (int32 *)(buf);
            hdr = (udp_header *)(buf + sizeof(int32));
            hdr->source = My_Address;
            hdr->dest = My_Address;
            hdr->source_port = ses->port;
            hdr->dest_port = ses->port;
            hdr->seq_no = 0;
            hdr->len = sizeof(spines_trace);

            spines_tr = (spines_trace *)( (char *)hdr + sizeof(udp_header));
            memset(spines_tr, 0, sizeof(spines_trace));
	    if(*type == TRACEROUTE_TYPE_MSG) {
                Trace_Route(My_Address, dest_addr, spines_tr);
            } else if (*type == EDISTANCE_TYPE_MSG) {
                Trace_Group(dest_addr, spines_tr);
            } else if (*type == MEMBERSHIP_TYPE_MSG ) { 
                Get_Group_Members(dest_addr, spines_tr);
            }

            tot_bytes = 0;
            *pkt_len = sizeof(udp_header)+sizeof(spines_trace);
            while(tot_bytes < sizeof(int32)+*pkt_len) {
                ret = send(ses->ctrl_sk,  buf, sizeof(int32)+*pkt_len-tot_bytes, 0);
                if (ret < 0) {
	            if((errno == EWOULDBLOCK)||(errno == EAGAIN)) {
                        Alarm(PRINT, "Blocking\n");
                    } else {
                        Alarm(EXIT, "Problem sending through control socket\n");
                    }
                }
	        tot_bytes += ret;
            } 
            dec_ref_cnt(buf);
            return(BUFF_EMPTY);
        }
	else {
	    Alarm(PRINT, "Session unknown command: %X\n", *type);
	    return(BUFF_EMPTY);
	}
    }

    /* Ok, this is data */
    if(ses->r_data != NULL) {
	/* This is Reliable UDP Data */
        /*Alarm(PRINT,"Reliable UDP Data\n");*/
	ret = Process_Reliable_Session_Packet(ses);
	return(ret);
    }
    else {
	/* This is UDP Data*/
	hdr->source = My_Address;
	hdr->source_port = ses->port;

	if(hdr->len + sizeof(udp_header) == ses->read_len) {
	    if(!Is_mcast_addr(hdr->dest) && !Is_acast_addr(hdr->dest)) {
		/* This is not a multicast address */
		/* Is this for me ? */
		if(hdr->dest == My_Address) {
		    ret = Deliver_UDP_Data(ses->data, ses->read_len, 0);
		    return(ret);
		}

		/* Nope, it's for smbd else. See where we should forward it */
		next_hop = Get_Route(My_Address, hdr->dest);
		if((next_hop != NULL) && (hdr->ttl > 0)) {
		    if(ses->links_used == SOFT_REALTIME_LINKS) {
			ret = Forward_RT_UDP_Data(next_hop, ses->data,
						  ses->read_len);
		    }
		    else if(ses->links_used == RELIABLE_LINKS) {
			ret = Forward_Rel_UDP_Data(next_hop, ses->data,
						   ses->read_len, 0);
		    }
		    else {
			ret = Forward_UDP_Data(next_hop, ses->data,
					       ses->read_len);
		    }
		    return(ret);
		}
		else {
		    return(NO_ROUTE);
		}
	    }
	    else { /* This is multicast or anycast */
		if(Unicast_Only == 1) {
		    return(NO_ROUTE);
		}

                if((g_state = (Group_State*)Find_State(&All_Groups_by_Node, My_Address, hdr->dest)) != NULL) {

                    /* Hey, I joined this group !*/
		    if(g_state->flags & ACTIVE_GROUP) {
			/*&& ((g_state->flags & RECV_GROUP)||(!(g_state->flags & SEND_GROUP)))) {*/
                        /* Deliver immediately for Multicast */
			Deliver_UDP_Data(ses->data, ses->read_len, 0);
       		    }
		}

		ret = NO_ROUTE;
		if(((neighbors = Get_Mcast_Neighbors(hdr->source, hdr->dest)) != NULL) && (hdr->ttl > 0)) {
		    stdhash_begin(neighbors, &ngb_it);
		    while(!stdhash_is_end(neighbors, &ngb_it)) {
			next_hop = *((Node **)stdhash_it_val(&ngb_it));
			if(ses->links_used == SOFT_REALTIME_LINKS) {
			    ret = Forward_RT_UDP_Data(next_hop, ses->data,
						      ses->read_len);
			}
			else if(ses->links_used == RELIABLE_LINKS) {
			    ret = Forward_Rel_UDP_Data(next_hop, ses->data,
						   ses->read_len, 0);
			}
			else {
			    ret = Forward_UDP_Data(next_hop, ses->data,
						   ses->read_len);
			    /*Alarm(PRINT,""IPF"\n",IP(next_hop->address));*/
			}
			stdhash_it_next(&ngb_it);
		    }
		}
		return(ret);
	    }
	}
	else {
	    Alarm(PRINT, "Process_udp_data_packet: Packed data... not available yet\n");
	    Alarm(PRINT, "hdr->len: %d; sizeof(udp_header): %d; ses->read_len: %d\n",
		  hdr->len, sizeof(udp_header), ses->read_len);
	}
    }
    return(NO_ROUTE);
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
    udp_header *hdr;
    stdit h_it, it;
    Session *ses;
    int ret;
    int32 dummy_port;
    Group_State *g_state;

    hdr = (udp_header*)buff;
    dummy_port = (int32)hdr->dest_port;


    /* Check if this is a multicast message */
    if(Is_mcast_addr(hdr->dest) || Is_acast_addr(hdr->dest)) {
	/* Multicast or Anycast.... */
	g_state = (Group_State*)Find_State(&All_Groups_by_Node, My_Address,
			 hdr->dest);
	if(g_state == NULL) {
	    return(NO_ROUTE);
	}
	if((g_state->flags & ACTIVE_GROUP) == 0) {
	    return(NO_ROUTE);
	}

	/* Ok, this is best effort multicast. */
	ret = NO_ROUTE;
	stdhash_begin(&g_state->joined_sessions, &it);
	while(!stdhash_is_end(&g_state->joined_sessions, &it)) {
	    ses = *((Session **)stdhash_it_val(&it));
            if( hdr->source != My_Address || 
                (hdr->source == My_Address && ses->multicast_loopback == 1)) 
            {
                ret = Session_Deliver_Data(ses, buff, len, type, 1);
                /* Deliver to only one. */
    	        if (Is_acast_addr(hdr->dest)) {
                     break;
    	        }
            }
	    stdhash_it_next(&it);
	}
	return ret;
    }

    /* If I got here, this is not multicast */

    stdhash_find(&Sessions_Port, &h_it, &dummy_port);

    if(stdhash_is_end(&Sessions_Port, &h_it)) {
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

    ret = Session_Deliver_Data(ses, buff, len, type, 1);

    return(ret);
}






/***********************************************************/
/* int Session_Deliver_Data(Session *ses, char* buff,      */
/*                          int16u len, int32u type,       */
/*                          int flags)                     */
/*                                                         */
/* Sends the data to the application                       */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:  session where to deliver data                     */
/* buff: pointer to the UDP packet                         */
/* len:  length of the packet                              */
/* flags: when buffer is full, drop the packet (1) or      */
/*        close the session (2)                            */
/*        force sending on the tcp socket (3)              */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) status of the packet (see udp.h)                  */
/*                                                         */
/***********************************************************/

int Session_Deliver_Data(Session *ses, char* buff, int16u len, int32u type, int flags) 
{
    UDP_Cell *u_cell;
    sys_scatter scat;
    int32 total_bytes;
    int32 rm_add_size;
    int ret, flag, cnt, i, stop_flag;
    int32 sum_len, send_len;
    udp_header *u_hdr;
    Frag_Packet *frag_pkt;
    sp_time now;
    int32 *pkt_no;
    sp_time *t1, send_time, diff;
    int32 oneway_time;
    char line[120];
    

    u_hdr = (udp_header *)buff;

#if 0
    Alarm(PRINT, "src_port: %d; sess_id: %d; len: %d; len_rep: %d; seq_no: %d; frag_num: %d; frag_idx: %d\n",
	  u_hdr->source_port, u_hdr->sess_id, u_hdr->len, len, u_hdr->seq_no, 
	  u_hdr->frag_num, u_hdr->frag_idx);
#endif    
	
    if(u_hdr->frag_num > 1) {
	/* This is a fragmented packet */

	
	now = E_get_time();
	
	/* Search for other fragments of the same packet */
	flag = 0;
	cnt = 0;
	frag_pkt = ses->frag_pkts;
	while(frag_pkt != NULL) {
	    if(now.sec - frag_pkt->timestamp_sec > FRAG_TTL) {
		/* This is an old incomplete packet. Discard it */
		
		Alarm(DEBUG, "Old incomplete packet. Delete it\n");

		frag_pkt = Delete_Frag_Element(&ses->frag_pkts, frag_pkt);
		continue;
	    }
	    
	    /* This is not an expired packet */

	    cnt++;
	    if((frag_pkt->sess_id == u_hdr->sess_id)&&
	       (frag_pkt->sender == u_hdr->source)&&
	       (frag_pkt->snd_port == u_hdr->source_port)&&
	       (frag_pkt->seq_no == u_hdr->seq_no)) {

		/* This is a fragmented packet that we were looking for */

		Alarm(DEBUG, "Found the packet\n");

		if((frag_pkt->scat.num_elements != u_hdr->frag_num)||
		   (frag_pkt->scat.elements[(int)(u_hdr->frag_idx)].buf != NULL)) {

		    Alarm(DEBUG, "Corrupt packet. Delete it\n");

		    Delete_Frag_Element(&ses->frag_pkts, frag_pkt);
		    return(BUFF_DROP);
		}
		
		/* Insert the fragment into the packet */
		
		frag_pkt->scat.elements[(int)(u_hdr->frag_idx)].buf = buff;
		inc_ref_cnt(buff);
		frag_pkt->scat.elements[(int)(u_hdr->frag_idx)].len = u_hdr->len + sizeof(udp_header);
		frag_pkt->recv_elements++;
		frag_pkt->timestamp_sec = now.sec;
		flag = 1;
		break;
	    }
	    else {
		/* This is a different incomplete packet */
		/*Alarm(PRINT, "Different fragmented packet\n");*/
	    }
	    frag_pkt = frag_pkt->next;
	}

	if(flag == 0) {
	    Alarm(DEBUG, "Couldn't find a fragmented packet. Total: %d; Create a new one\n", cnt);

	    cnt++;
	    if((frag_pkt = new(FRAG_PKT)) == NULL) {
		Alarm(EXIT, "Could not allocate memory\n");
	    }
	    
	    frag_pkt->scat.num_elements = u_hdr->frag_num;

	    for(i=0; i<u_hdr->frag_num; i++) {
		frag_pkt->scat.elements[i].buf = NULL;
	    }
	    frag_pkt->scat.elements[(int)(u_hdr->frag_idx)].buf = buff;
	    inc_ref_cnt(buff);
	    frag_pkt->scat.elements[(int)(u_hdr->frag_idx)].len = u_hdr->len + sizeof(udp_header);

	    frag_pkt->recv_elements = 1;
	    frag_pkt->sess_id = u_hdr->sess_id;
	    frag_pkt->seq_no = u_hdr->seq_no;
	    frag_pkt->snd_port = u_hdr->source_port;
	    frag_pkt->sender = u_hdr->source;
	    frag_pkt->timestamp_sec = now.sec;
	    
	    /* Insert the fragmented packet into the linked list */
	    if(ses->frag_pkts != NULL) {
		ses->frag_pkts->prev = frag_pkt;
	    }
	    frag_pkt->next = ses->frag_pkts;
	    frag_pkt->prev = NULL;
	    ses->frag_pkts = frag_pkt;
	}
	    
	/* Deliver the packet if it is complete */
	if(frag_pkt->recv_elements == frag_pkt->scat.num_elements) {
	    Alarm(DEBUG, "\n!!! Packet complete !!!\n\n");

	    rm_add_size = 0;

	    /* Prepare the packet */
	    
	    sum_len = 0;
	    scat.num_elements = frag_pkt->scat.num_elements+1;
	    for(i=0; i<frag_pkt->scat.num_elements; i++) {
		if(i == 0) {
		    scat.elements[i+1].buf = frag_pkt->scat.elements[i].buf;
		    scat.elements[i+1].len = frag_pkt->scat.elements[i].len;
		}
		else {
		    scat.elements[i+1].buf = frag_pkt->scat.elements[i].buf + sizeof(udp_header) + rm_add_size;
		    scat.elements[i+1].len = frag_pkt->scat.elements[i].len - sizeof(udp_header) - rm_add_size;
		}
		sum_len += scat.elements[i+1].len;
	    }
	    u_hdr = (udp_header *)(frag_pkt->scat.elements[0].buf);
	    u_hdr->len = sum_len;
	    scat.elements[0].len = sizeof(int32);
	    scat.elements[0].buf = (char*)(&sum_len);


	    Alarm(DEBUG, "sum_len: %d; elements: %d\n", sum_len, scat.num_elements);

	    /* Deliver the packet */
	    if((ses->udp_port != -1)&&(flags != 3)) {
		/* The session communicates via UDP */
		ret = DL_send(Ses_UDP_Send_Channel,  ses->udp_addr, ses->udp_port,  &scat);
		Delete_Frag_Element(&ses->frag_pkts, frag_pkt);
		return(BUFF_EMPTY);
	    }
	    else {
		/* The session communicates via TCP */

		Alarm(DEBUG, "TCP-based session\n");

		for(i=0; i<frag_pkt->scat.num_elements; i++) {
		    /* Try to deliver all the fragments, one by one */

		    /* If there is already smthg in the buffer, put this one too */
		    if(!stdcarr_empty(&ses->rel_deliver_buff)) {

			Alarm(PRINT, "??? There is something in the buffer already: %d\n",
			      stdcarr_size(&ses->rel_deliver_buff));

			if(i == 0) {
			    if(stdcarr_size(&ses->rel_deliver_buff) >= 3*MAX_BUFF_SESS) {
				/* disconnect the session or drop the packet */
				if (flags == 1) {
				    
				    Alarm(PRINT, "=== Drop the packet\n");

				    Delete_Frag_Element(&ses->frag_pkts, frag_pkt);
				    return(BUFF_DROP);
				} 
				else if ((flags == 2)||(flags == 3)) {
				    /* disconnect */
				    Session_Close(ses->sess_id, SES_BUFF_FULL);
				    return(NO_BUFF);
				}
			    }			
			}
		    
			if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
			    Alarm(EXIT, "Deliver_UDP_Data(): Cannot allocte udp cell\n");
			}
			u_cell->len = frag_pkt->scat.elements[i].len;
			u_cell->buff = frag_pkt->scat.elements[i].buf;
			stdcarr_push_back(&ses->rel_deliver_buff, &u_cell);
			inc_ref_cnt(frag_pkt->scat.elements[i].buf);

			Alarm(PRINT, "\n\n!!! Put the fragment in the buffer !!!\n");

			continue;
		    }

		    /* There is nothing in the buffer. Try to deliver the fragment */



		    total_bytes = frag_pkt->scat.elements[i].len + sizeof(int32);
		    if(i == 0) {
			ses->sent_bytes = 0;
		    }
		    else {
			ses->sent_bytes = sizeof(udp_header) + sizeof(int32);
		    }
		    stop_flag = 0;
		    while((ses->sent_bytes < total_bytes)&&(stop_flag == 0)) {
			if(ses->sent_bytes < sizeof(int32)) {
			    scat.num_elements = 2;
			    scat.elements[0].len = sizeof(int32) - ses->sent_bytes;
			    scat.elements[0].buf = ((char*)(&sum_len)) + ses->sent_bytes;
			    scat.elements[1].len = frag_pkt->scat.elements[i].len;
			    scat.elements[1].buf = frag_pkt->scat.elements[i].buf;
			}
			else {
			    scat.num_elements = 1;
			    scat.elements[0].len = frag_pkt->scat.elements[i].len - 
				(ses->sent_bytes - sizeof(int32));
			    scat.elements[0].buf = frag_pkt->scat.elements[i].buf + 
				(ses->sent_bytes - sizeof(int32));
			}

			/* The session communicates via TCP */
			ret = DL_send(ses->sk,  My_Address, ses->port,  &scat);
			
			Alarm(DEBUG,"Session_deliver_data(): %d %d %d %d\n",
			      ret, ses->sk, ses->port, frag_pkt->scat.elements[i].len);

			if(ret < 0) {
			    Alarm(DEBUG, "Session_Deliver_Data(): write err\n");
#ifndef	ARCH_PC_WIN95
			    if((ret == -1)&&
			       ((errno == EWOULDBLOCK)||(errno == EAGAIN)))
#else
#ifndef _WIN32_WCE
				if((ret == -1)&&
				   ((errno == WSAEWOULDBLOCK)||(errno == EAGAIN)))
#else
				    int sk_errno = WSAGetLastError();
			    if((ret == -1)&&
			       ((sk_errno == WSAEWOULDBLOCK)||(sk_errno == EAGAIN)))
#endif /* Windows CE */
#endif
			    {
				if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
				    Alarm(EXIT, "Deliver_UDP_Data(): Cannot allocte udp cell\n");
				}
				u_cell->len = frag_pkt->scat.elements[i].len;
				u_cell->buff = frag_pkt->scat.elements[i].buf;
				stdcarr_push_back(&ses->rel_deliver_buff, &u_cell);
				inc_ref_cnt(frag_pkt->scat.elements[i].buf);
				
				E_attach_fd(ses->sk, WRITE_FD, Session_Write, ses->sess_id,
					    NULL, HIGH_PRIORITY );
				ses->fd_flags = ses->fd_flags | WRITE_DESC;
				stop_flag = 1;
				break;
			    }
			    else {
				Session_Close(ses->sess_id, SOCK_ERR);
				return(NO_BUFF);
			    }
			}
			if(ret == 0) {
			    Alarm(PRINT, "Error: ZERO write 1; sent: %d, total: %d\n",
				  ses->sent_bytes, total_bytes);
			}
			ses->sent_bytes += ret;
		    }
		    if(stop_flag == 0) {
			if(i == frag_pkt->scat.num_elements - 1) {	
			    ses->sent_bytes = 0;
			}
			else {
			    ses->sent_bytes = sizeof(udp_header) + sizeof(int32);
			}
		    }
		}		
	    }
	    Delete_Frag_Element(&ses->frag_pkts, frag_pkt);
	}
	if(stdcarr_empty(&ses->rel_deliver_buff)) {
	    return(BUFF_EMPTY);
	}
	else {
	    return(BUFF_OK);	
	}
    }




    /* If we got here this is not a fragmented packet. We can send it directly */

    if(ses->recv_fd_flag == 1) {
	/* This is a log-only session */
	
	pkt_no = (int32*)(buff+sizeof(udp_header)+sizeof(int32));
	if(*pkt_no == -1) {
	    Session_Close(ses->sess_id, SES_BUFF_FULL);
	    return(NO_BUFF);
	}

	t1  = (sp_time*)(buff+sizeof(udp_header)+2*sizeof(int32));
	send_time = *t1;
	now = E_get_time();
	diff = E_sub_time(now, send_time);
	oneway_time = diff.usec + diff.sec*1000000;

	sprintf(line, "%d\t%d\n", *pkt_no, oneway_time);
	
	//Alarm(PRINT, "%s", line);
	write(ses->fd, line, strlen(line));
	
	return(BUFF_EMPTY);
    }


    if((ses->udp_port == -1)||(flags == 3)) {
	/* The session communicates via TCP */
	if(!stdcarr_empty(&ses->rel_deliver_buff)) {
	    if(stdcarr_size(&ses->rel_deliver_buff) >= 3*MAX_BUFF_SESS) {
		/* disconnect the session or drop the packet */
		if (flags == 1) {
		    return(BUFF_DROP);
		} 
		else if ((flags == 2)||(flags == 3)) {
		    /* disconnect */
		    Session_Close(ses->sess_id,SES_BUFF_FULL);
		    return(NO_BUFF);
		}
	    }
	    
	    if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
		Alarm(EXIT, "Deliver_UDP_Data(): Cannot allocte udp cell\n");
	    }
	    u_cell->len = len;
	    u_cell->buff = buff;
	    stdcarr_push_back(&ses->rel_deliver_buff, &u_cell);
	    inc_ref_cnt(buff);
	    return(BUFF_OK);
	}
    }


    /* If I got up to here, the buffer is empty or we send via UDP.
     * Let's see if we can send this packet */


    total_bytes = sizeof(int32) + len;
    send_len = len;
    ses->sent_bytes = 0;
    while(ses->sent_bytes < total_bytes) {
	if(ses->sent_bytes < sizeof(int32)) {
	    scat.num_elements = 2;
	    scat.elements[0].len = sizeof(int32) - ses->sent_bytes;
	    scat.elements[0].buf = ((char*)(&send_len)) + ses->sent_bytes;
	    scat.elements[1].len = send_len;
	    scat.elements[1].buf = buff;
	}
	else {
	    scat.num_elements = 1;
	    scat.elements[0].len = send_len - (ses->sent_bytes - sizeof(int32));
	    scat.elements[0].buf = buff + (ses->sent_bytes - sizeof(int32));
	}
	if((ses->udp_port == -1)||(flags == 3)) {
	    /* The session communicates via TCP */
	    ret = DL_send(ses->sk,  My_Address, ses->port,  &scat);
	}
	else {
	    /* The session communicates via UDP */
	    ret = DL_send(Ses_UDP_Send_Channel,  ses->udp_addr, ses->udp_port,  &scat);
	}

	Alarm(DEBUG,"Session_deliver_data(): %d %d %d %d\n",ret,ses->sk,ses->port, send_len);

	if(ret < 0) {
	     Alarm(DEBUG, "Session_Deliver_Data(): write err\n");
#ifndef	ARCH_PC_WIN95
	    if((ret == -1)&&
	       ((errno == EWOULDBLOCK)||(errno == EAGAIN)))
#else
#ifndef _WIN32_WCE
	    if((ret == -1)&&
	       ((errno == WSAEWOULDBLOCK)||(errno == EAGAIN)))
#else
	    int sk_errno = WSAGetLastError();
	    if((ret == -1)&&
	       ((sk_errno == WSAEWOULDBLOCK)||(sk_errno == EAGAIN)))
#endif /* Windows CE */
#endif
	    {
		if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
		    Alarm(EXIT, "Deliver_UDP_Data(): Cannot allocte udp cell\n");
		}
		u_cell->len = len;
		u_cell->buff = buff;
		stdcarr_push_back(&ses->rel_deliver_buff, &u_cell);
		inc_ref_cnt(buff);

		E_attach_fd(ses->sk, WRITE_FD, Session_Write, ses->sess_id,
			    NULL, HIGH_PRIORITY );
		ses->fd_flags = ses->fd_flags | WRITE_DESC;
		return(BUFF_OK);
	    }
	    else {
		if(ses->r_data == NULL) {
		    Session_Close(ses->sess_id, SOCK_ERR);
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
    stdit it;
    stdit h_it;
    int32 len;
    rel_udp_pkt_add *r_add;
    Session *ses;
    char *buff;
    int32 total_bytes, data_bytes, send_bytes;
    int ret;
    udp_header *u_hdr;


    /* Find the session. This is not the session that I read packets from,
     * but rather the one I'm trying to write data to. */

    stdhash_find(&Sessions_ID, &h_it, &sess_id);
    if(stdhash_is_end(&Sessions_ID, &h_it)) {
	/* The session is gone */
        return;
    }

    ses = *((Session **)stdhash_it_val(&h_it));

    if(ses->sess_id != (unsigned int)sess_id) {
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


    while(!stdcarr_empty(&ses->rel_deliver_buff)) {
	stdcarr_begin(&ses->rel_deliver_buff, &it);

	u_cell = *((UDP_Cell **)stdcarr_it_val(&it));
	buff = u_cell->buff;
	u_hdr = (udp_header*)buff;

	/*	get_ref_cnt(buff); */
	len = u_cell->len;

	if(ses->r_data != NULL) {
            if (ses->sent_bytes == 0) {
                ses->sent_bytes = sizeof(udp_header) + sizeof(rel_udp_pkt_add) + sizeof(int32);
            }

	    r_add = (rel_udp_pkt_add*)(buff + sizeof(udp_header));
	    data_bytes = len - r_add->ack_len;
	    total_bytes = sizeof(int32) + data_bytes;
	    Alarm(DEBUG, "Reliable session !!!  %d : %d\n", r_add->data_len, r_add->ack_len);
	}
	else {
	    data_bytes = len;
	    total_bytes = sizeof(int32) + len;
	}
	if((u_hdr->frag_num > 1)&&(u_hdr->frag_idx == 0)) {
	    send_bytes = u_hdr->len;
	}
	else {
	    send_bytes = data_bytes;
	}
	while(ses->sent_bytes < total_bytes) {
	    if(ses->sent_bytes < sizeof(int32)) {
		scat.num_elements = 2;
		scat.elements[0].len = sizeof(int32) - ses->sent_bytes;
		scat.elements[0].buf = ((char*)(&send_bytes)) + ses->sent_bytes;
		scat.elements[1].len = data_bytes;
		scat.elements[1].buf = buff;
	    }
	    else {
		scat.num_elements = 1;
		scat.elements[0].len = data_bytes - (ses->sent_bytes - sizeof(int32));
		scat.elements[0].buf = buff + (ses->sent_bytes - sizeof(int32));
	    }
	    ret = DL_send(ses->sk,  My_Address, ses->port,  &scat);

	    Alarm(PRINT,"Session_Write(): %d %d %d %d %d\n", ret, ses->sk, ses->port, len, ses->sent_bytes);


	    if(ret < 0) {
		Alarm(DEBUG, "Session_WRITE(): write err\n");
#ifndef	ARCH_PC_WIN95
		if((ret == -1)&&
		   ((errno == EWOULDBLOCK)||(errno == EAGAIN)))
#else
#ifndef _WIN32_WCE
		if((ret == -1)&&
		   ((errno == WSAEWOULDBLOCK)||(errno == EAGAIN)))
#else
		int sk_errno = WSAGetLastError();
		if((ret == -1)&&
		   ((sk_errno == WSAEWOULDBLOCK)||(sk_errno == EAGAIN)))
#endif /* Windows CE */
#endif
		{
		    E_attach_fd(ses->sk, WRITE_FD, Session_Write, ses->sess_id,
				NULL, HIGH_PRIORITY );
		    ses->fd_flags = ses->fd_flags | WRITE_DESC;
		    return;
		}
		else {
		    if(ses->r_data == NULL) {
			Session_Close(ses->sess_id, SOCK_ERR);
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
	if(ses->r_data == NULL && u_hdr->frag_num > 1 && (u_hdr->frag_idx < u_hdr->frag_num -1)) {
	    Alarm(PRINT, "Session_Write: Not the last Fragmented Packet\n");
	    ses->sent_bytes = sizeof(udp_header) + sizeof(int32);
	}
	else {
	    ses->sent_bytes = 0;
	}
	dec_ref_cnt(buff);
	dispose(u_cell);
	stdcarr_pop_front(&ses->rel_deliver_buff);
    }

    /* Send an ack if the sender was blocked */
    if(ses->r_data != NULL) {
	if(stdcarr_size(&ses->rel_deliver_buff) < MAX_BUFF_SESS/2) {
	    Alarm(DEBUG, "Session_Write(): sending ack: %d\n",
		  stdcarr_size(&ses->rel_deliver_buff));
	    E_queue(Ses_Send_Ack, ses->sess_id, NULL, zero_timeout);
	}
    }


    /* Check if this is a disconnected reliable session */
    if(ses->r_data != NULL) {
	if(ses->r_data->connect_state == DISCONNECT_LINK) {
	    if(ses->r_data->recv_tail == ses->r_data->last_seq_sent) {
		if(stdcarr_empty(&ses->rel_deliver_buff)) {
		    Disconnect_Reliable_Session(ses);
		}
	    }
	}
    }
}



/***********************************************************/
/* void Ses_Send_ID(Session* ses)                          */
/*                                                         */
/* Sends to a session its own ID + port it is assigned     */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:     session                                        */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Ses_Send_ID(Session *ses) 
{
    char *buf;
    int32 *ses_id;
    int32 *vport;
    udp_header *u_hdr;

    buf = (char *) new_ref_cnt(PACK_BODY_OBJ);
    if(buf == NULL) {
        Alarm(EXIT, "Session_UDP_Read: Cannot allocate buffer\n");
    }

    u_hdr = (udp_header*)buf;
    u_hdr->source = 0;
    u_hdr->dest = 0;
    u_hdr->source_port = 0;
    u_hdr->dest_port = 0;
    u_hdr->len = 2 * sizeof(int32); /* sess ID + virtual port */
    u_hdr->seq_no = 0;
    u_hdr->frag_num = 1;
    u_hdr->frag_idx = 0;
    u_hdr->sess_id = 0;

    ses_id = (int32*)(buf+sizeof(udp_header));
    *ses_id = ses->sess_id;

    vport   = (int32 *)(buf+sizeof(udp_header)+sizeof(ses->sess_id));
    *vport  = ses->port;

    Session_Deliver_Data(ses, buf, sizeof(udp_header)+(2*sizeof(int32)), 0, 3);
    dec_ref_cnt(buf);
}




/***********************************************************/
/* void Ses_Send_ERR(int address, int port)                */
/*                                                         */
/* Sends to a SOCK_DGRAM client that it should be closed   */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* address:   address of the client                        */
/* port:      port of the client                           */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Ses_Send_ERR(int address, int port) 
{
    sys_scatter scat;
    udp_header u_hdr;
    int32 len;

    len = sizeof(udp_header);
    scat.num_elements = 2;
    scat.elements[0].len = sizeof(int32);
    scat.elements[0].buf = (char*)&len;
    scat.elements[1].len = sizeof(udp_header);
    scat.elements[1].buf = (char*)&u_hdr;
   
    u_hdr.dest = -1;

    DL_send(Ses_UDP_Send_Channel, address, port, &scat);
}


/***********************************************************/
/* void Session_UDP_Read(int sk, int dmy, void *dmy_p)     */
/*                                                         */
/* Receive data from a DGRAM socket                        */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      socket                                         */
/* dmy:     not used                                       */
/* dmy_p:   not used                                       */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Session_UDP_Read(int sk, int dmy, void * dmy_p) 
{
    int received_bytes;
    Session *ses;
    stdit it;
    int32 sess_id, rnd_num;
    udp_header *u_hdr;
    char *tmp_buf;
    int i, processed_bytes, bytes_to_send;



    Ses_UDP_Pack.num_elements = 52;
    Ses_UDP_Pack.elements[0].len = sizeof(int32);
    Ses_UDP_Pack.elements[0].buf = (char *)&sess_id;
    Ses_UDP_Pack.elements[1].len = sizeof(int32);
    Ses_UDP_Pack.elements[1].buf = (char *)&rnd_num;
    Ses_UDP_Pack.elements[2].len = MAX_SPINES_MSG+sizeof(udp_header);
    Ses_UDP_Pack.elements[2].buf = frag_buf[0]                   ;
    for(i=1; i<50; i++) {
    	Ses_UDP_Pack.elements[i+2].len = MAX_SPINES_MSG;
    	Ses_UDP_Pack.elements[i+2].buf = frag_buf[i]+sizeof(udp_header);
    }

    received_bytes = DL_recv(sk, &Ses_UDP_Pack);  
    received_bytes -= (2*sizeof(int32));

    u_hdr = (udp_header*)frag_buf[0];
    stdhash_find(&Sessions_ID, &it, &sess_id);
    if(stdhash_is_end(&Sessions_ID, &it)) {
	/* The session is gone */
	Alarm(PRINT, "The session is gone\n");
	Ses_Send_ERR(u_hdr->source, u_hdr->source_port);
	for(i=0; i<50; i++) {
	    dec_ref_cnt(frag_buf[i]);
	}
	return;
    }

    ses = *((Session **)stdhash_it_val(&it));
    if(ses->rnd_num != rnd_num) {
	/* The session is gone */
	Alarm(PRINT, "The session is gone\n");
	Ses_Send_ERR(u_hdr->source, u_hdr->source_port);
	for(i=0; i<50; i++) {
	    dec_ref_cnt(frag_buf[i]);
	}
	return;
    }


    /* This is valid data for this session. It should be processed */
    /* Replace the session data, with the packet received, and process it */
    /* as it would have been received via TCP */

    ses->seq_no++;
    if(ses->seq_no >= 10000) {
	ses->seq_no = 0;
    }
    ses->frag_num = u_hdr->len/MAX_SPINES_MSG;
    if(u_hdr->len%MAX_SPINES_MSG != 0) {
	ses->frag_num++;
    }
    ses->frag_idx = 0;

    dec_ref_cnt(ses->data);
    processed_bytes = sizeof(udp_header);
    i = 0;
    while(processed_bytes < received_bytes) {
	if(received_bytes - processed_bytes <= MAX_SPINES_MSG) {
	    bytes_to_send = received_bytes - processed_bytes;
	}
	else {
	    bytes_to_send = MAX_SPINES_MSG;
	}

	tmp_buf = frag_buf[i];

	u_hdr = (udp_header*)tmp_buf;

	if(ses->frag_num > 1) {
	    if(ses->frag_idx == 0) {
		memcpy((void*)(&ses->save_hdr), (void*)u_hdr, sizeof(udp_header));
	    }
	    else {
		memcpy((void*)u_hdr, (void*)(&ses->save_hdr), sizeof(udp_header));
	    }
	    u_hdr->len = bytes_to_send;
	}
	u_hdr->seq_no = ses->seq_no;
	u_hdr->frag_num = ses->frag_num;
	u_hdr->frag_idx = ses->frag_idx;
	
	Alarm(DEBUG, "snd udp seq_no: %d; frag_num: %d; frag_idx: %d; len: %d\n",
	      u_hdr->seq_no, u_hdr->frag_num, u_hdr->frag_idx, u_hdr->len);
	
	ses->data = tmp_buf;
	ses->read_len = u_hdr->len + sizeof(udp_header);

	/* Process the packet */
	Process_Session_Packet(ses);

	/*
	 * if(get_ref_cnt(ses->data) > 1) {
	 *    Alarm(PRINT, "I'm here\n");
	 *    dec_ref_cnt(ses->data);
	 * }
	 */

	ses->frag_idx++;
	i++;
	processed_bytes += bytes_to_send;
    }

    /*    for(i=ses->frag_num; i<50; i++) {*/
    for(i=0; i<ses->frag_num; i++) {
	dec_ref_cnt(frag_buf[i]);
	frag_buf[i] = new_ref_cnt(PACK_BODY_OBJ);
	if(frag_buf[i] == NULL) {
	    Alarm(EXIT, "Cannot allocate memory\n");
	}
    }

    if((ses->data = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
	Alarm(EXIT, "Session_Read(): Cannot allocte packet_body\n");
    }
}




void Block_Session(struct Session_d *ses)
{
    int ret, ioctl_cmd;

    /* set file descriptor to blocking */
    ioctl_cmd = 0;
#ifdef ARCH_PC_WIN95
    ret = ioctlsocket(ses->sk, FIONBIO, (void*) &ioctl_cmd);
#else
    ret = ioctl(ses->sk, FIONBIO, &ioctl_cmd);
#endif

    if(ses->fd_flags & READ_DESC) {
	E_detach_fd(ses->sk, READ_FD);
	ses->fd_flags = ses->fd_flags ^ READ_DESC;
    }

    /*
     *if(ses->fd_flags & EXCEPT_DESC) {
     *	E_detach_fd(ses->sk, EXCEPT_FD);
     *	ses->fd_flags = ses->fd_flags ^ EXCEPT_DESC;
     *}
     */

    /* set file descriptor to non blocking */
    ioctl_cmd = 1;
#ifdef ARCH_PC_WIN95
    ret = ioctlsocket(ses->sk, FIONBIO, (void*) &ioctl_cmd);
#else
    ret = ioctl(ses->sk, FIONBIO, &ioctl_cmd);
#endif

}


void Block_All_Sessions(void) {
    stdit it;
    Session *ses;

    Alarm(DEBUG, "Block_All_Sessions\n");

    stdhash_begin(&Sessions_ID, &it);
    while(!stdhash_is_end(&Sessions_ID, &it)) {
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
    ret = ioctlsocket(ses->sk, FIONBIO, (void*) &ioctl_cmd);
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
    ret = ioctlsocket(ses->sk, FIONBIO, (void*) &ioctl_cmd);
#else
    ret = ioctl(ses->sk, FIONBIO, &ioctl_cmd);
#endif

}


void Resume_All_Sessions(void) {
    stdit it;
    Session *ses;

    Alarm(DEBUG, "Resume_All_Sessions\n");

    stdhash_begin(&Sessions_ID, &it);
    while(!stdhash_is_end(&Sessions_ID, &it)) {
        ses = *((Session **)stdhash_it_val(&it));
	if(ses->rel_blocked == 0) {
	    Resume_Session(ses);
	}

	stdhash_it_next(&it);
    }
}



void Session_Flooder_Send(int sesid, void *dummy)
{
    stdit it;
    Session *ses;
    udp_header *hdr;
    int32 *pkt_no, *msg_size;
    char *buf;
    sp_time *t1, now, next_delay;
    long long int duration_now, int_delay; 
    double rate_now;
    int i;
 

    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_is_end(&Sessions_ID, &it)) {
	/* The session is gone */
        return;
    }
    ses = *((Session **)stdhash_it_val(&it));
    if(ses->sess_id != (unsigned int)sesid) {
	/* There's another session here, and it just uses the same socket */
	Alarm(PRINT, "different session: %d != %d\n", sesid, ses->sess_id);
	return;
    }

    Alarm(DEBUG,""IPF" port: %d; rate: %d; size: %d; num: %d\n",
	  IP(ses->Sendto_address), ses->Sendto_port, ses->Rate, 
	  ses->Packet_size, ses->Num_packets);

    /* Prepare the packet */
    ses->seq_no++;
    ses->Sent_packets++;
    now = E_get_time();	

    hdr = (udp_header*)ses->data;
    hdr->source = My_Address;
    hdr->source_port = ses->port;
    hdr->dest = ses->Sendto_address;
    hdr->dest_port = ses->Sendto_port;
    hdr->len = ses->Packet_size;
    hdr->seq_no = ses->seq_no;
    hdr->frag_num = (int16u)ses->frag_num;
    hdr->frag_idx = (int16u)ses->frag_idx;
    hdr->sess_id = (int16u)(ses->sess_id & 0x0000ffff);

    buf = ses->data+sizeof(udp_header);
    msg_size = (int32*)buf;
    pkt_no = (int32*)(buf+sizeof(int32));
    t1 = (sp_time*)(buf+2*sizeof(int32));
    *pkt_no = ses->Sent_packets;
    *msg_size = ses->Packet_size;
    *t1 = now;

    ses->read_len = ses->Packet_size + sizeof(udp_header);

    Process_Session_Packet(ses);    
    
    if(get_ref_cnt(ses->data) > 1) {
	dec_ref_cnt(ses->data);
	if((ses->data = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
	    Alarm(EXIT, "Session_Flooder_Send(): Cannot allocte packet_body\n");
	}
    }


    if(ses->Sent_packets == ses->Num_packets) {
	*pkt_no = -1;
	for(i=0; i<10; i++) {
	    ses->seq_no++;
	    hdr = (udp_header*)ses->data;
	    hdr->source = My_Address;
	    hdr->source_port = ses->port;
	    hdr->dest = ses->Sendto_address;
	    hdr->dest_port = ses->Sendto_port;
	    hdr->len = ses->Packet_size;
	    hdr->seq_no = ses->seq_no;
	    hdr->frag_num = (int16u)ses->frag_num;
	    hdr->frag_idx = (int16u)ses->frag_idx;
	    hdr->sess_id = (int16u)(ses->sess_id & 0x0000ffff);
	    
	    buf = ses->data+sizeof(udp_header);
	    msg_size = (int32*)buf;
	    pkt_no = (int32*)(buf+sizeof(int32));
	    t1 = (sp_time*)(buf+2*sizeof(int32));
	    *pkt_no = -1;
	    *msg_size = ses->Packet_size;
	    *t1 = now;

	    ses->read_len = ses->Packet_size + sizeof(udp_header);

	    Process_Session_Packet(ses);

	    if(get_ref_cnt(ses->data) > 1) {
		dec_ref_cnt(ses->data);
		if((ses->data = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
		    Alarm(EXIT, "Session_Flooder_Send(): Cannot allocte packet_body\n");
		}
	    }
	}
	Session_Close(sesid, SES_BUFF_FULL);
	return;
    }
    duration_now  = (now.sec - ses->Start_time.sec);
    duration_now *= 1000000;
    duration_now += now.usec - ses->Start_time.usec;

    rate_now = ses->Packet_size;
    rate_now = rate_now * ses->Sent_packets * 8 * 1000;
    rate_now = rate_now/duration_now;
    
    next_delay.sec = 0;
    next_delay.usec = 0;	    
    if(rate_now > ses->Rate) {
	int_delay = ses->Packet_size;
	int_delay = int_delay * ses->Sent_packets * 8 * 1000;
	int_delay = int_delay/ses->Rate; 
	int_delay = int_delay - duration_now;
	
	if(int_delay > 0) {
	    next_delay.sec = int_delay/1000000;
	    next_delay.usec = int_delay%1000000;
	}
    }
    E_queue(Session_Flooder_Send, sesid, NULL, next_delay);   
}
