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
#include <stdlib.h>
#include <string.h>


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
#include "udp.h"
#include "reliable_udp.h"
#include "session.h"
#include "route.h"
#include "reliable_session.h"
#include "hello.h"

/* Global variables */

extern int32     My_Address;
extern stdhash   Sessions_ID;
extern stdhash   Sessions_Port;
extern stdhash   Rel_Sessions_Port;
extern stdhash   Sessions_Sock;
extern int16     Link_Sessions_Blocked_On;


/* Local consts */

static const sp_time zero_timeout       = {     0,    0};
static const sp_time one_sec_timeout    = {     1,    0};
static const sp_time disconnect_timeout = {    30,    0};


/***********************************************************/
/* int Init_Reliable_Session(Session* ses, int32 address,  */
/*                           int16u port)                  */
/*                                                         */
/* Initializes a reliable session                          */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:     Session that needs to be made reliable         */
/* address: Other side's address                           */
/* port:    Other side's port                              */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) status of the action                              */
/*                                                         */
/***********************************************************/


int Init_Reliable_Session(Session *ses, int32 address, int16u port)
{
    int32u i;
    Reliable_Data *r_data;


    if(ses->port == 0) {
        Alarm(PRINT, "Init_Reliable_Session(): port not defined\n");
        Session_Close(ses->sk, PORT_IN_USE);
        return(NO_BUFF);
    }

    ses->type = RELIABLE_SES_TYPE;
    ses->rel_otherside_addr = address;
    ses->rel_otherside_port = port;
    ses->rel_hello_cnt = 0;


    if((r_data = (Reliable_Data*) new(RELIABLE_DATA))==NULL)
	Alarm(EXIT, "Process_Session_Packet: Cannot allocte reliable_data object\n");
    r_data->flags = UNAVAILABLE_LINK;
    r_data->seq_no = 0;
    stdcarr_construct(&(r_data->msg_buff), sizeof(Buffer_Cell*));
    for(i=0;i<MAX_WINDOW;i++) {
	r_data->window[i].buff = NULL;
	r_data->window[i].data_len = 0;

	r_data->recv_window[i].flag = EMPTY_CELL;
	r_data->recv_window[i].data.len = 0;
	r_data->recv_window[i].data.buff = NULL;
    }
    r_data->head = 0;
    r_data->tail = 0;
    r_data->recv_head = 0;
    r_data->recv_tail = 0;
    r_data->nack_buff = NULL;
    r_data->nack_len = 0;
    r_data->scheduled_ack = 0;
    r_data->scheduled_timeout = 0;
    r_data->timeout_multiply = 1;
    r_data->rtt = 0;
    r_data->congestion_flag = 0;
    r_data->ack_window = 1;
    r_data->unacked_msgs = 0;
    r_data->last_ack_sent = 0;

    /* Congestion control */
    r_data->window_size = 10;

 
    r_data->max_window = MAX_CG_WINDOW/2;
    r_data->ssthresh = MAX_CG_WINDOW/2;

    ses->r_data = r_data;
    
    return(BUFF_EMPTY);
}




/***********************************************************/
/* int Init_Reliable_Connect(Session* ses, int32 address,  */
/*                           int16u port)                  */
/*                                                         */
/* Starts a reliable session connect                       */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:     Session that needs to be made reliable         */
/* address: Other side's address                           */
/* port:    Other side's port                              */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1  success                                       */
/*       -1  failure                                       */
/*                                                         */
/***********************************************************/

int Init_Reliable_Connect(Session *ses, int32 address, int16u port)
{
    stdhash_it it;
    int i;

    for(i= 30000; i< 65535; i++) {
	stdhash_find(&Sessions_Port, &it, &i);
	if(stdhash_it_is_end(&it)) {
	    break;
	}
    }
    if(i == 65535) {
	return(-1);
    }
    
    ses->port = (int16u)i;
    stdhash_insert(&Sessions_Port, &it, &i, &ses);

    Init_Reliable_Session(ses, address, port);

    if(ses->r_data == NULL) {
	return(-1);
    }
    ses->rel_orig_port = port;
    ses->r_data->flags = CONNECT_WAIT_LINK;
    
    
    /* send the first packet of the handshake */

    Ses_Send_Rel_Hello(ses->sess_id, NULL);

    return(1);
}



/***********************************************************/
/* int Accept_Rel_Session(Session *ses, udp_header *cmd,   */
/*                        char *buf)                       */
/*                                                         */
/* Handles an accept message from the session.             */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:     the session that sends the accept              */
/* cmd:     command sent by the session                    */
/* buf:     info sent by the connecting (initiator) session*/
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int)  1 accept successful                              */
/*       -1 accept unsuccessful                            */
/*                                                         */
/***********************************************************/


int Accept_Rel_Session(Session *ses, udp_header *cmd, char *buf)
{
    ses_hello_packet *hello_pkt;
    stdhash_it it;
    int i, orig_port;

    Alarm(DEBUG, "\n\n !!!!!!!!!!ACCEPT!!!!!!!! Session ID: %d\n", ses->sess_id);

    Alarm(DEBUG, "source: %d\ndest: %d\nsource_port: %d\ndest_port: %d\nlen: %d\nseq_no: %d\n\n",
	  cmd->source,
	  cmd->dest,
	  cmd->source_port,
	  cmd->dest_port,
	  cmd->len,
	  cmd->seq_no);

    if(cmd->len != sizeof(rel_udp_pkt_add) + sizeof(ses_hello_packet))
	Alarm(EXIT, "Accept_Rel_Session: corrupt packet\n");
    
    hello_pkt = (ses_hello_packet*)(buf+sizeof(rel_udp_pkt_add));

    Alarm(DEBUG, "HELLO:\ntype: %d\nseq_no: %d\nmy_sess_id: %d\nmy_port: %d\norig_port: %d\n\n",
	  hello_pkt->type,
	  hello_pkt->seq_no,
	  hello_pkt->my_sess_id,
	  hello_pkt->my_port,
	  hello_pkt->orig_port);


    for(i= 30000; i< 65535; i++) {
	stdhash_find(&Sessions_Port, &it, &i);
	if(stdhash_it_is_end(&it)) {
	    break;
	}
    }
    if(i == 65535) {
	return(-1);
    }

    ses->port = (int16u)i;
    stdhash_insert(&Sessions_Port, &it, &i, &ses);

    Init_Reliable_Session(ses, cmd->dest, cmd->dest_port);
    orig_port = hello_pkt->orig_port;
    stdhash_insert(&Rel_Sessions_Port, &it, &orig_port, &ses);

    if(ses->r_data == NULL) {
	return(-1);
    }
    ses->r_data->flags = ACCEPT_WAIT_LINK;
    ses->rel_orig_port = hello_pkt->orig_port;
    ses->rel_otherside_id = hello_pkt->my_sess_id;
    
    
    /* send the second packet of the handshake */

    Ses_Send_Rel_Hello(ses->sess_id, NULL);

    return(1);
	    
}



/***********************************************************/
/* void Ses_Send_Rel_Hello(int sesid, void* dummy)         */
/*                                                         */
/* Sends an a session level hello msg.                     */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sesid:     ID of the session that sends the ack         */
/* dummy:     Not used                                     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Ses_Send_Rel_Hello(int sesid, void* dummy) 
{
    udp_header *u_hdr;
    rel_udp_pkt_add *r_add;
    ses_hello_packet *hello_data;
    char *send_buff;
    Node* next_hop;
    Session *ses;
    stdhash_it it;


    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_it_is_end(&it)) {
	/* The session is gone */
        return;
    }

    ses = *((Session **)stdhash_it_val(&it));

    if(ses->r_data == NULL) 
	Alarm(EXIT, "Ses_Send_Rel_Hello: Not a reliable sesion\n");
    

    if(ses->rel_hello_cnt > 4*DEAD_LINK_CNT) {
	Session_Close(ses->sk, SOCK_ERR);
	return;
    }

    next_hop = Get_Route(My_Address, ses->rel_otherside_addr);
    if(next_hop == NULL) {
	/* I don't have a route to the destination. 
	   It might be temporary */
	E_queue(Ses_Send_Rel_Hello, ses->sess_id, NULL, one_sec_timeout);    
	return;
    }
 
    Alarm(DEBUG, "Sending Ses_Rel_Hello: ses: %d - %d\n", ses->sess_id, ses->type);

    
    if((send_buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
	Alarm(EXIT, "Ses_Send_Ack: Cannot allocte packet_body object\n");
    }

    u_hdr = (udp_header*)send_buff;
    u_hdr->source = My_Address;
    u_hdr->source_port = ses->port;
    u_hdr->dest = ses->rel_otherside_addr;
    u_hdr->dest_port = ses->rel_otherside_port;
    u_hdr->len = sizeof(rel_udp_pkt_add) + sizeof(ses_hello_packet);

    r_add = (rel_udp_pkt_add*)(send_buff + sizeof(udp_header));
    r_add->type = Set_endian(HELLO_TYPE);
    r_add->data_len = sizeof(ses_hello_packet);
    r_add->ack_len = 0;

    hello_data = (ses_hello_packet*)(send_buff + sizeof(udp_header) + sizeof(rel_udp_pkt_add));
    hello_data->type = Set_endian(HELLO_TYPE);
    hello_data->my_sess_id = ses->sess_id;

    if(ses->r_data->flags == CONNECT_WAIT_LINK) {
	hello_data->my_port = ses->port;
	hello_data->orig_port = ses->rel_otherside_port;
	hello_data->seq_no = 0;
	Alarm(DEBUG, "send_hello: CONNECT_WAIT_LINK\n");
    }
    else if(ses->r_data->flags == ACCEPT_WAIT_LINK) {
	hello_data->my_port = ses->port;
	hello_data->orig_port = ses->rel_orig_port;
	hello_data->seq_no = 1;
	Alarm(DEBUG, "send_hello: ACCEPT_WAIT_LINK\n");
    }
    else if(ses->r_data->flags == CONNECTED_LINK) {
	hello_data->my_port = ses->port;
	hello_data->orig_port = ses->rel_orig_port;
	if(ses->client_stat == SES_CLIENT_ON) {
	    hello_data->seq_no = 2;
	    Alarm(DEBUG, "send_hello: CONNECTED_LINK; INIT\n");
	}
	else {
	    ses->r_data->last_seq_sent = ses->r_data->seq_no;
	    r_add->type = Set_endian(HELLO_DISCNCT_TYPE);
	    hello_data->seq_no = ses->r_data->seq_no;
	    Alarm(DEBUG, "send_hello: CONNECTED_LINK; DISCONNECT\n");
	}
    }
    else if(ses->r_data->flags == DISCONNECT_LINK) {
	hello_data->my_port = ses->port;
	hello_data->orig_port = ses->rel_orig_port;
	r_add->type = Set_endian(HELLO_CLOSE_TYPE);
	hello_data->seq_no = 0;
	Alarm(DEBUG, "send_hello: CONNECTED_LINK; CLOSE\n");
    }
    else {
	Alarm(EXIT, "Ses_Send_Rel_Hello: Invalid state -> BUG!!!\n");
    }

    Alarm(DEBUG, "rel_hello_cnt: %d\n\n", ses->rel_hello_cnt);

    ses->rel_hello_cnt++;

    /* Send the Packet */

    if(ses->links_used == RELIABLE_LINKS) {
	Forward_Rel_UDP_Data(next_hop, send_buff, 
			     u_hdr->len+sizeof(udp_header), 0);
    }
    else {
	Forward_UDP_Data(next_hop, send_buff, 
			 u_hdr->len+sizeof(udp_header));
    }

    dec_ref_cnt(send_buff);

    /* Schedule a timeout for the hello message */

    E_queue(Ses_Send_Rel_Hello, ses->sess_id, NULL, one_sec_timeout);    
}





/***********************************************************/
/* void Close_Reliable_Session(Session* ses)               */
/*                                                         */
/* Closes a reliable session                               */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:  Session that was reliable                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* none                                                    */
/*                                                         */
/***********************************************************/


void Close_Reliable_Session(Session* ses)
{
    int32u i;
    Reliable_Data *r_data;
    stdcarr_it c_it;
    stdhash_it h_it;
    Buffer_Cell *buf_cell;
    char *msg;
    int32 dummy_port;
    Session *tmp_ses;

    
    E_dequeue(Ses_Send_Rel_Hello, ses->sess_id, NULL);
    E_dequeue(Ses_Delay_Close, ses->sess_id, NULL);

    if(ses->r_data == NULL)
	return;

    r_data = ses->r_data;

    /* Remove data from the window */
    for(i=r_data->tail; i<=r_data->head; i++) {
	if(r_data->window[i%MAX_WINDOW].buff != NULL)
	    dec_ref_cnt(r_data->window[i%MAX_WINDOW].buff);
    }

    for(i=r_data->recv_tail; i<=r_data->recv_head; i++) {
	if(r_data->recv_window[i%MAX_WINDOW].data.buff != NULL) {
	    dec_ref_cnt(r_data->recv_window[i%MAX_WINDOW].data.buff);
	    r_data->recv_window[i%MAX_WINDOW].data.buff = NULL;
	}
    }

    /* Remove data from the queue */
    while(!stdcarr_empty(&(r_data->msg_buff))) {
	stdcarr_begin(&(r_data->msg_buff), &c_it);
	
	buf_cell = *((Buffer_Cell **)stdcarr_it_val(&c_it));
	msg = buf_cell->buff;
	dec_ref_cnt(msg);
	dispose(buf_cell);
	stdcarr_pop_front(&(r_data->msg_buff));
    }
    stdcarr_destruct(&(r_data->msg_buff));
    
    if(r_data->nack_buff != NULL) {
	E_dequeue(Ses_Send_Nack_Retransm, ses->sess_id, NULL);
	dispose(r_data->nack_buff);
	r_data->nack_len = 0;
    }

    if(r_data->scheduled_timeout == 1) {
	E_dequeue(Ses_Reliable_Timeout, ses->sess_id, NULL);
    }
    r_data->scheduled_timeout = 0;

    if(r_data->scheduled_ack == 1) {
	E_dequeue(Ses_Send_Ack, ses->sess_id, NULL);
    }
    r_data->scheduled_ack = 0;

    E_dequeue(Ses_Try_to_Send, ses->sess_id, NULL);
    
    dispose(r_data);

    dummy_port = (int32)ses->rel_orig_port;
    stdhash_find(&Rel_Sessions_Port, &h_it, &dummy_port);
    while(!stdhash_it_is_end(&h_it)) {
	tmp_ses = *((Session **)stdhash_it_val(&h_it));
	if(tmp_ses->sess_id == ses->sess_id) {
	    stdhash_erase(&h_it);
	    break;
	}
	stdhash_it_keyed_next(&h_it); 
    }
}



/***********************************************************/
/* void Disconnect_Reliable_Session(Session* ses)          */
/*                                                         */
/* Starts the processof disconnecting  a reliable session  */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:  Session that was reliable                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* none                                                    */
/*                                                         */
/***********************************************************/


void Disconnect_Reliable_Session(Session* ses)
{
    stdcarr_it c_it;
    UDP_Cell *u_cell;
    char *buff;

    /* Close the connection to the client */

    if(ses->r_data == NULL)
	return;

    if(ses->fd_flags & READ_DESC)
	E_detach_fd(ses->sk, READ_FD);
    
    if(ses->fd_flags & EXCEPT_DESC)
	E_detach_fd(ses->sk, EXCEPT_FD);
    
    if(ses->fd_flags & WRITE_DESC)
	E_detach_fd(ses->sk, WRITE_FD);
    

    DL_close_channel(ses->sk);    
    Alarm(PRINT, "session closed: %d\n", ses->sk);

    while(!stdcarr_empty(&ses->udp_deliver_buff)) {
	stdcarr_begin(&ses->udp_deliver_buff, &c_it);
	
	u_cell = *((UDP_Cell **)stdcarr_it_val(&c_it));
	buff = u_cell->buff;
	
	dec_ref_cnt(buff);
	dispose(u_cell);
	stdcarr_pop_front(&ses->udp_deliver_buff);
    }
    stdcarr_destruct(&ses->udp_deliver_buff);
    

    /* Set the flags for a disconnected (orphan for now) session */
    ses->client_stat = SES_CLIENT_ORPHAN;

    Ses_Send_Rel_Hello(ses->sess_id, NULL);   

    E_queue(Ses_Delay_Close, ses->sess_id, NULL, disconnect_timeout);
}


/***********************************************************/
/* void Ses_Delay_Close(int sesid, void* dummy)            */
/*                                                         */
/* Close a reliable session after a timeout   .            */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sesid:     ID of the session that sends the ack         */
/* dummy:     Not used                                     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Ses_Delay_Close(int sesid, void* dummy) 
{
    Session *ses;
    stdhash_it it;

    Alarm(DEBUG, "Ses_Delay_Close, timeout on session: %d\n", sesid);
    
    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_it_is_end(&it)) {
	/* The session is gone */
	Alarm(DEBUG, "Session: %d is gone !!!\n\n", sesid);
        return;
    }

    ses = *((Session **)stdhash_it_val(&it));

    Session_Close(ses->sk, SES_DELAY_CLOSE);
}

/***********************************************************/
/* int Process_Reliable_Session_Packet(Session* ses)       */
/*                                                         */
/* Processes a packet just received from a reliable session*/
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:  Session                                           */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* status of the processing (buffers)                      */
/*                                                         */
/***********************************************************/


int Process_Reliable_Session_Packet(Session *ses)
{
    udp_header *u_hdr;
    rel_udp_pkt_add *r_add;
    Reliable_Data *r_data;
    reliable_tail *r_tail;
    int ret;


    r_data = ses->r_data;
    if(r_data == NULL) {
	Alarm(EXIT, "Process_Reliable_Sess_Pkt: No reliable data struct !");
    }

    u_hdr = (udp_header*)ses->data;
    r_add = (rel_udp_pkt_add*)(ses->data + sizeof(udp_header));

    u_hdr->source = My_Address;
    u_hdr->source_port = ses->port;
    u_hdr->dest = ses->rel_otherside_addr;
    u_hdr->dest_port = ses->rel_otherside_port;


    /* Setting the reliability tail of the packet */
    r_tail = (reliable_tail*)(ses->data+ses->total_len);
    r_tail->seq_no = r_data->seq_no++;
    r_tail->cummulative_ack = r_data->recv_tail;
    
    r_add->ack_len = sizeof(reliable_tail);
    r_add->type = Set_endian(REL_UDP_DATA_TYPE);
    u_hdr->len += r_add->ack_len;

    if(u_hdr->dest == My_Address) {
 	ret = Deliver_Rel_UDP_Data(ses->data, ses->total_len + r_add->ack_len, 0);
    }
    else {
	ret = Reliable_Ses_Send(ses); 
    }
    
    return(ret);
}


/***********************************************************/
/* int Check_Double_Connect(char* buff, int16u len,        */
/*                          int32u type)                   */ 
/*                                                         */
/* Checks for a double connect request for a reliable sess */
/* connection                                              */
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
/* (int) 1 if this is a connect request for a connection   */
/*         already established                             */
/*       0 otherwise                                       */
/*                                                         */
/***********************************************************/

int Check_Double_Connect(char *buff, int16u len, int32u type)
{
    ses_hello_packet *sh_pkt;
    udp_header *hdr;
    Session *tmp_ses;
    stdhash_it h_it;
    int32 dummy_port;


    if(len != sizeof(ses_hello_packet) + sizeof(udp_header) + sizeof(rel_udp_pkt_add))
	Alarm(EXIT, "Check_Double_Connect: packet too small\n");


    hdr = (udp_header*)buff;
    sh_pkt = (ses_hello_packet*)(buff + sizeof(udp_header) + sizeof(rel_udp_pkt_add));

    Alarm(DEBUG, "Check_Double_Connect\n\n\n");

  
    dummy_port = (int32)sh_pkt->orig_port;
    stdhash_find(&Rel_Sessions_Port, &h_it, &dummy_port);
    while(!stdhash_it_is_end(&h_it)) {
	tmp_ses = *((Session **)stdhash_it_val(&h_it));
	if((tmp_ses->rel_otherside_addr == hdr->source)&&
	   (tmp_ses->rel_otherside_id == sh_pkt->my_sess_id)) {
	    return(1);;
	}
	stdhash_it_keyed_next(&h_it); 
    }
    return(0);
}




/***********************************************************/
/* int Deliver_Rel_UDP_Data(char* buff, int16u len,        */
/*                          int32u type)                   */ 
/*                                                         */
/* Delivers reliable UDP data to the application           */
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

int Deliver_Rel_UDP_Data(char* buff, int16u len, int32u type) {
    UDP_Cell *u_cell;
    udp_header *hdr;
    rel_udp_pkt_add *r_add;
    reliable_tail *r_tail;
    stdhash_it h_it;
    int flag;
    Session *ses;
    Reliable_Data *r_data;
    int ret, tmp_ret;
    int32 dummy_port;
    sp_time short_timeout;
    

    hdr = (udp_header*)buff;
    dummy_port = (int32)hdr->dest_port;
    stdhash_find(&Sessions_Port, &h_it, &dummy_port);

    if(stdhash_it_is_end(&h_it)) {
	return(NO_ROUTE);
    }
    ses = *((Session **)stdhash_it_val(&h_it));

    r_data = ses->r_data;

    r_add = (rel_udp_pkt_add*)(buff + sizeof(udp_header));

    r_tail = (reliable_tail*)(buff+sizeof(udp_header) + 
			      sizeof(rel_udp_pkt_add) + r_add->data_len);

    
    if(r_data == NULL)
	return(BUFF_DROP);


    if(Is_hello(r_add->type)) {
	Alarm(DEBUG, "Got it. Hello type !!!\n");

	if((r_data->flags != CONNECT_WAIT_LINK)&&
	   (r_data->flags != ACCEPT_WAIT_LINK))
	    return(BUFF_DROP);

	Process_Rel_Ses_Hello(ses, buff, len);

	return(BUFF_DROP);
    }
    else if(Is_hello_discnct(r_add->type)) {
	Alarm(DEBUG, "Got it. DISCONNECT Hello type !!!\n");

	if(r_data->flags != CONNECTED_LINK)
	    return(BUFF_DROP);

	Process_Rel_Ses_Hello(ses, buff, len);

	return(BUFF_DROP);
    }
    else if(Is_hello_close(r_add->type)) {
	Alarm(DEBUG, "Got it. CLOSE Hello type !!!\n");

	if(r_data->flags != CONNECTED_LINK)
	    return(BUFF_DROP);

	Process_Rel_Ses_Hello(ses, buff, len);

	return(BUFF_DROP);
    }


    if(r_data->flags != CONNECTED_LINK)
	return(BUFF_DROP);


    /* See if we need the packet */

    /* Here we process the ack, and if we don't need it return */
    flag = Process_Sess_Ack(ses, (char*)r_tail, r_add->ack_len, r_add->type, type);


    /* This is a data (or ack) packet. This means that the connection is already 
     * established so no need for Session Hello msgs anymore */
    if((ses->client_stat == SES_CLIENT_ON)&&(ses->rel_hello_cnt >= 0)) {
	ses->rel_hello_cnt = -1;
	E_dequeue(Ses_Send_Rel_Hello, ses->sess_id, NULL);    
    }


    /* If the session with the client was closed, we just drop the packet */
    if(ses->client_stat != SES_CLIENT_ON)
	return(BUFF_DROP);

 
    if(flag == -1) { /* It's only an ack packet... */
	return(BUFF_DROP);
    }


    Alarm(DEBUG, "Got pkt: %d : %d\n", r_tail->seq_no, r_data->recv_tail);

    /* Close the session if there is no room for the packet in the buffer */
    if(!stdcarr_empty(&ses->udp_deliver_buff)) {
	if(stdcarr_size(&ses->udp_deliver_buff) >= MAX_BUFF_SESS) {
	    Disconnect_Reliable_Session(ses);
	    Alarm(DEBUG, "Session closed due to buffer overflow\n");
	    return(NO_BUFF);
	}
    }


    /* We should send an acknowledge for this message. 
     * So, let's schedule it 
     */

    /*Alarm(PRINT, "ack_window: %d; unacked_msgs: %d; last_ack_sent: %d\n",
     *	  r_data->ack_window, r_data->unacked_msgs, r_data->last_ack_sent);
     */

    if(r_data->scheduled_ack == 1) {
	E_dequeue(Ses_Send_Ack, (int)ses->sess_id, NULL);
    }

    r_data->scheduled_ack = 1;

    if(r_data->unacked_msgs >= r_data->ack_window - 1) {
	E_queue(Ses_Send_Ack, ses->sess_id, NULL, zero_timeout);
    }
    else {
	short_timeout.usec = r_data->rtt;
	if(short_timeout.usec < 10000)
	    short_timeout.usec = 10000;
	short_timeout.sec  = short_timeout.usec/1000000;
	short_timeout.usec = short_timeout.usec%1000000;
	E_queue(Ses_Send_Ack, ses->sess_id, NULL, short_timeout);

	Alarm(DEBUG, "Deliver_Rel_UDP_Data() unacked: %d; ack_window: %d\n",
	      r_data->unacked_msgs, r_data->ack_window);
    }

    if(flag == 0) {
	Alarm(DEBUG, "Session retransmission received: %d : %d\n", 
	      r_tail->seq_no, r_data->recv_tail);
	return(BUFF_DROP);
    }


    /* put the packet in the window. If it is out of order, return. */
    
    r_data->recv_window[r_tail->seq_no%MAX_WINDOW].data.len = len;
    r_data->recv_window[r_tail->seq_no%MAX_WINDOW].data.buff = buff;
    r_data->recv_window[r_tail->seq_no%MAX_WINDOW].flag = RECVD_CELL;
    inc_ref_cnt(buff);

    if(r_tail->seq_no != r_data->recv_tail) {
	return(BUFF_OK);
    }



    /* Since we got up to here, this packet should be delivered, 
       it is in order. */
    /* Deliver everything that is in order, from this packet on. */


    /* First, deliver the packet */

    /* However, if there is a buffer already, just stick the packet in the buffer*/
    /* The buffer is not completely full, we've checked already earlier */

    Alarm(DEBUG, "In order deliver pkt: %d\n", r_data->recv_tail);

    if(!stdcarr_empty(&ses->udp_deliver_buff)) {

	/* Put the packet in the buffer */
	if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
	    Alarm(EXIT, "Deliver_Rel_UDP_Data(): Cannot allocte udp cell\n");
	}
	u_cell->len = len;
	u_cell->buff = buff;
	stdcarr_push_back(&ses->udp_deliver_buff, &u_cell);
	inc_ref_cnt(buff);

	/* Take it out of the window */
	dec_ref_cnt(r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff);
	r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.len = 0;
	r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff = NULL;
	r_data->recv_window[r_data->recv_tail%MAX_WINDOW].flag = EMPTY_CELL;
	r_data->recv_tail++;

	return(BUFF_OK);
    }


    Alarm(DEBUG, "Direct deliver pkt: %d\n", r_tail->seq_no);


    /* Deliver the packet and take it out of the window */
    ret = Net_Rel_Sess_Send(ses, buff, len);
    if(ret == NO_BUFF) {
	/* The session has been closed */
	return(ret);
    }

    r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.len = 0;
    r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff = NULL;
    r_data->recv_window[r_data->recv_tail%MAX_WINDOW].flag = EMPTY_CELL;
    r_data->recv_tail++;
    dec_ref_cnt(buff);


    /* Then, deliver the rest of the in-order packets from the window, if any */

    while(r_data->recv_tail < r_data->recv_head) {
	if(r_data->recv_window[r_data->recv_tail%MAX_WINDOW].flag != RECVD_CELL)
	    break;

	Alarm(DEBUG, "In order deliver pkt: %d\n", r_data->recv_tail);

	/* If there is a buffer already, just stick the packet in the buffer*/
	if(!stdcarr_empty(&ses->udp_deliver_buff)) {
	    if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
		Alarm(EXIT, "Deliver_Rel_UDP_Data(): Cannot allocte udp cell\n");
	    }
	    u_cell->len = r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.len;
	    u_cell->buff = r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff;
	    stdcarr_push_back(&ses->udp_deliver_buff, &u_cell);
	    inc_ref_cnt(buff);

	    /* Take the packet from the window */
	    dec_ref_cnt(r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff);
	    r_data->recv_window[r_data->recv_tail%MAX_WINDOW].flag = EMPTY_CELL;
	    r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.len = 0;
	    r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff = NULL;
	    r_data->recv_tail++;

	    continue;
	}


	tmp_ret = Net_Rel_Sess_Send(ses, 
		   r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff,
		   r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.len);

	if(tmp_ret == NO_BUFF) {
	    /* The session was closed */
	    return(tmp_ret);
	}

	/* Take the packet from the window */
	dec_ref_cnt(r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff);
	r_data->recv_window[r_data->recv_tail%MAX_WINDOW].flag = EMPTY_CELL;
	r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.len = 0;
	r_data->recv_window[r_data->recv_tail%MAX_WINDOW].data.buff = NULL;
	r_data->recv_tail++;

    }

    /* Return what happened with our initial packet */


    if(r_data->connect_state == DISCONNECT_LINK) {
	if(r_data->recv_tail == r_data->last_seq_sent) {
	    if(stdcarr_empty(&ses->udp_deliver_buff)) {
		Disconnect_Reliable_Session(ses);
	    }
	}
    }
    return(ret);

}



/***********************************************************/
/* void Process_Rel_Ses_Hello(Session *ses, char* buff     */
/*                            int len)                     */
/*                                                         */
/* Processes a reliable session HELLO packet               */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:  session that got the HELLO msg                    */
/* buff: pointer to the HELLO msg                          */
/* len:  length of the HELLO msg                           */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Process_Rel_Ses_Hello(Session *ses, char *buff, int len)
{
    Reliable_Data *r_data;
    ses_hello_packet *sh_pkt;
    rel_udp_pkt_add *r_add;

    if(len != sizeof(ses_hello_packet) + sizeof(udp_header) + sizeof(rel_udp_pkt_add))
	return;

    r_data = ses->r_data;
    if(r_data == NULL)
	return;

    r_add = (rel_udp_pkt_add*)(buff + sizeof(udp_header));
    sh_pkt = (ses_hello_packet*)(buff + sizeof(udp_header) + sizeof(rel_udp_pkt_add));

    Alarm(DEBUG, "Process_Rel_Ses_Hello\n\n\n");

    if(ses->rel_orig_port != sh_pkt->orig_port)
	return;


    if(Is_hello(r_add->type)) {
	ses->rel_hello_cnt = 0;
	
	if(r_data->flags == CONNECT_WAIT_LINK) {
	    ses->rel_otherside_port = sh_pkt->my_port;
	    ses->rel_otherside_id = sh_pkt->my_sess_id;
	    
	    r_data->flags = CONNECTED_LINK;
	    
	    Alarm(DEBUG, "Session Link Connected \n");
	    
	    Ses_Send_Rel_Hello(ses->sess_id, NULL);
	    
	    Net_Rel_Sess_Send(ses, buff, len);
	}
	else if(r_data->flags == ACCEPT_WAIT_LINK) {
	    r_data->flags = CONNECTED_LINK;
	    Alarm(DEBUG, "Session Link Connected \n");
	    Net_Rel_Sess_Send(ses, buff, len);
	}
    }
    else if(Is_hello_discnct(r_add->type)) {
	Alarm(DEBUG, "I'm here... DISCONNECT Hello type !!!\n");

	Alarm(DEBUG, "hello seq_no: %d; recv_tail: %d; recv_head: %d\n", 
	      sh_pkt->seq_no,
	      r_data->recv_tail,
	      r_data->recv_head);

	if(r_data->connect_state == DISCONNECT_LINK) {	
	    Alarm(DEBUG, "Duplicate Hello Disconnect !\n");
	    return;
	}
	if(ses->client_stat == SES_CLIENT_ON) {
	    r_data->connect_state = DISCONNECT_LINK;	
	    if(sh_pkt->seq_no == r_data->recv_tail) {
		Disconnect_Reliable_Session(ses);
	    }
	    else {	    
		r_data->last_seq_sent = sh_pkt->seq_no;
		Alarm(DEBUG , "\n\n\n!!! I'm not ready yet !!!\n\n\n");		
	    }
	}
	else {
	    r_data->flags = DISCONNECT_LINK;
	    Ses_Send_Rel_Hello(ses->sess_id, NULL);
	    Session_Close(ses->sk, SES_DELAY_CLOSE);
	}
    }
    else if(Is_hello_close(r_add->type)) {
	Alarm(DEBUG, "I'm here... CLOSE Hello type !!!\n");
	if(r_data->connect_state != DISCONNECT_LINK) {	
	    Alarm(DEBUG, "Duplicate Hello Close !\n");
	    return;
	}
	Session_Close(ses->sk, SES_DELAY_CLOSE);
    }
}



/***********************************************************/
/* int Net_Rel_Sess_Send(Session *ses, char* buff,         */
/*                       int16u len)                       */ 
/*                                                         */
/* Delivers reliable UDP data to the application           */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:  session to deliver the packet to                  */
/* buff: pointer to the UDP packet                         */
/* len:  length of the packet                              */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) status of the packet (see udp.h)                  */
/*                                                         */
/***********************************************************/


int Net_Rel_Sess_Send(Session *ses, char *buff, int16u len)
{
    int16u total_bytes, data_bytes;
    rel_udp_pkt_add *r_add;
    sys_scatter scat;
    UDP_Cell *u_cell;
    int ret;



    if(ses->client_stat == SES_CLIENT_OFF)
	return(BUFF_DROP);

    r_add = (rel_udp_pkt_add*)(buff + sizeof(udp_header));

    data_bytes = len - r_add->ack_len;
    total_bytes = 2 + data_bytes;
    ses->sent_bytes = 0;

    while(ses->sent_bytes < total_bytes) {
	if(ses->sent_bytes < 2) {
	    scat.num_elements = 2;
	    scat.elements[0].len = 2 - ses->sent_bytes;
	    scat.elements[0].buf = ((char*)(&data_bytes)) + ses->sent_bytes;
	    scat.elements[1].len = data_bytes;
	    scat.elements[1].buf = buff;
	}
	else {
	    scat.num_elements = 1;
	    scat.elements[0].len = data_bytes - (ses->sent_bytes - 2);
	    scat.elements[0].buf = buff + (ses->sent_bytes - 2);
	}
	ret = DL_send(ses->sk,  My_Address, ses->port,  &scat );
	if(ret < 0) {
	    
	    Alarm(DEBUG, "Net_Rel_Sess_Send(): write err\n");

	    
#ifndef	ARCH_PC_WIN95
	    if((ret == -1)&&
	       ((errno == EWOULDBLOCK)||(errno == EAGAIN))) 
#else
	    if((ret == -1)&&
			((errno == WSAEWOULDBLOCK)||(errno == EAGAIN))) 
#endif	
	    {
			
		if((u_cell = (UDP_Cell*) new(UDP_CELL))==NULL) {
		    Alarm(EXIT, "Deliver_UDP_Data(): Cannot allocte udp cell\n");
		}
		u_cell->len = len;
		u_cell->buff = buff;
		stdcarr_push_back(&ses->udp_deliver_buff, &u_cell);
		inc_ref_cnt(buff);
		
		E_attach_fd(ses->sk, WRITE_FD, Session_Write, ses->sess_id, 
			    NULL, LOW_PRIORITY );
		ses->fd_flags = ses->fd_flags | WRITE_DESC;
		return(BUFF_OK);
	    } 
	    else {
		Disconnect_Reliable_Session(ses);
		return(NO_BUFF);
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
/* int Process_Sess_Ack(Session *ses, char *buff,          */
/*                      int16u ack_len, int32 ses_type,    */
/*                      int32u net_type)                   */
/*                                                         */
/* Processes an ACK between two reliable sessions          */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:       destination session                          */
/* buff:      buffer cointaining the ACK                   */
/* ack_len:   length of the ACK                            */
/* ses_type:  type of the message (session level)          */
/* net_type:  type of the message (network level)          */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) 1 if the packet that came with the ack contians   */
/*         any useful data                                 */
/*       0 if the packet has data that we already know     */
/*	   (retransm)                                      */
/*      -1 if this is an ack packet                        */
/*                                                         */
/***********************************************************/

int Process_Sess_Ack(Session *ses, char* buf, int16u ack_len, 
		     int32u ses_type, int32u net_type)
{
    Reliable_Data *r_data;
    reliable_tail *r_tail;
    int32u i, ack_window;
    sp_time timeout_val, now, diff;
    int32u rtt_estimate;
    double old_window;
    int congestion_action = 0;
    
    
    r_data = ses->r_data;
    
    now = E_get_time();

    r_data->congestion_flag = net_type & ECN_DATA_MASK;

    if((!(net_type & ECN_ACK_MASK))&&(!(net_type & ECN_DATA_MASK))) {
	/* Normal operation. No intermediary congestion */
	congestion_action = 0;
    }
    else if((net_type & ECN_ACK_T3)||(net_type & ECN_DATA_T3)) {
	/* Something is realy congested, on buffer is overflown 
	 * We should reset the congestion window */
	congestion_action = 1;
    }
    else if((net_type & ECN_ACK_T2)||(net_type & ECN_DATA_T2)) {
	/* At least one buffer is half-way full. 
	 * We should half the congestion window */
	congestion_action = 2;
    }
    else if((net_type & ECN_ACK_T1)||(net_type & ECN_DATA_T1)) {
	/* At least one buffer has some packets in it. 
	 * We should not increase the the congestion window further */
	congestion_action = 3;
    }
    if(congestion_action > 0)
	Alarm(DEBUG, "action: %d\n", congestion_action);



    if((congestion_action == 0)||(congestion_action == 3)) {
	ack_window = ses_type & ACK_INTERVAL_MASK;
	ack_window = ack_window >> 12;
	ack_window++;	
	r_data->ack_window = ack_window;
    }
    else {
	r_data->ack_window = 1;
    }



    if(ack_len > sizeof(reliable_tail)) {
	/* We also have NACKs here... */
	Alarm(DEBUG, "We also have NACKs here...\n");
	if(r_data->nack_len + ack_len > sizeof(packet_body)) {
	    Alarm(DEBUG, "nack_len: %d; ack_len: %d\n", r_data->nack_len, ack_len);
	    Alarm(EXIT, "WOW !!! a lot of nacks here.... definitely a bug\n");
	}
	if(r_data->nack_buff == NULL) {
	    if((r_data->nack_buff = (char*) new(PACK_BODY_OBJ))==NULL) {
		Alarm(EXIT, "Process_Sess_Ack(): Cannot allocte pack_body object\n");
	    }	
	    else
		Alarm(DEBUG, "nack_buff not empty; nack_len: %d\n", r_data->nack_len);
	}
	if(r_data->nack_len + ack_len - sizeof(reliable_tail) <
	   sizeof(packet_body)) {
	    memcpy(r_data->nack_buff+r_data->nack_len, buf+sizeof(reliable_tail), 
		   ack_len-sizeof(reliable_tail));
	    r_data->nack_len += ack_len - sizeof(reliable_tail);
	    Alarm(DEBUG, "nack_len: %d\n", r_data->nack_len);
	}
	Alarm(DEBUG, "queueing nacks...\n");
	E_queue(Ses_Send_Nack_Retransm, ses->sess_id, NULL, zero_timeout);
    }

    /* Check the cummulative acknowledgement */
    r_tail = (reliable_tail*)buf;


    Alarm(DEBUG, "Got SES ack for %d; tail: %d\n", r_tail->cummulative_ack,
	  r_data->tail);

    if(r_tail->cummulative_ack > r_data->head) {
        /* This is from another movie...  got an ack for a packet
	 * I haven't sent yet... ignore the packet. */

	Alarm(DEBUG, "r_tail->cummulative_ack: %d; r_data->head: %d\n",
	      r_tail->cummulative_ack, r_data->head);

        return(0);
    }


    if(r_tail->cummulative_ack > r_data->tail) {
	if((r_data->window[(r_tail->cummulative_ack-1)%MAX_WINDOW].buff != NULL)&&
	   (r_data->window[(r_tail->cummulative_ack-1)%MAX_WINDOW].resent == 0)) {
	    diff = E_sub_time(now, r_data->window[(r_tail->cummulative_ack-1)%MAX_WINDOW].timestamp);
	    rtt_estimate = diff.sec * 1000000 + diff.usec;
	    if(r_data->rtt == 0) {
		r_data->rtt = rtt_estimate;
	    }
	    else {
		r_data->rtt = 0.2*rtt_estimate + 0.8*r_data->rtt;
	    }
	}
	for(i=r_data->tail; i<r_tail->cummulative_ack; i++) {
	    if(r_data->window[i%MAX_WINDOW].buff != NULL) {
		dec_ref_cnt(r_data->window[i%MAX_WINDOW].buff);
		r_data->window[i%MAX_WINDOW].buff = NULL;
		r_data->window[i%MAX_WINDOW].data_len = 0;
	    }
	    else
		Alarm(EXIT, "Process_Sess_Ack(): Reliability failure\n");
	    
	    r_data->tail++;


	    old_window = r_data->window_size;
	    
	    /* Congestion control */
	    if(congestion_action == 0) {
		if(!stdcarr_empty(&(r_data->msg_buff))) { 
		    /* there are other packets waiting, so it makes sense to increase the window */
		    if(r_data->window_size < r_data->ssthresh) {
			/* Slow start */
			r_data->window_size += 1;
		    }
		    else {
			/* Congestion avoidance */
			r_data->window_size += 1/r_data->window_size;
		    }
		    if(r_data->window_size > r_data->max_window) {
			r_data->window_size = r_data->max_window;
		    }
		}
	    }
	    else if(congestion_action == 1) {
		r_data->ssthresh /= 2;
		if(r_data->ssthresh < 1) {
		    r_data->ssthresh = 1;
		}
		r_data->window_size = 1;
		congestion_action = 3;
	    }
	    else if(congestion_action == 2) {
		r_data->ssthresh /= 2;
		if(r_data->ssthresh < 1) {
		    r_data->ssthresh = 1;
		}
		r_data->window_size /= 2;
		if(r_data->window_size < 1) {
		    r_data->window_size = 1;
		}
		congestion_action = 3;
	    }
	    
	    if(r_data->window_size != old_window)
		Alarm(DEBUG, "SES window adjusted: %5.3f\n", r_data->window_size);

	}		    
	/* This was a fresh brand new ack. See if it freed some window slots
	 * and we can send some more stuff */	

	E_queue(Ses_Try_to_Send, ses->sess_id, NULL, zero_timeout);

    }

    /* Reset the timeout exponential back-off */
    r_data->timeout_multiply = 1;

    /* Cancel the previous timeout */
    if(r_data->scheduled_timeout == 1) {
        E_dequeue(Ses_Reliable_Timeout, ses->sess_id, NULL);
	r_data->scheduled_timeout = 0;
    }

    /*See if we need another timeout */
    if(r_data->head > r_data->tail) {
	Alarm(DEBUG, "+++ Another timeout ! tail: %d, head: %d\n",
	      r_data->tail, r_data->head);
   
	timeout_val.sec = (r_data->rtt*5)/1000000;
	timeout_val.usec = (r_data->rtt*5)%1000000;
	
	if(timeout_val.sec == 0 && timeout_val.usec == 0) {
	    timeout_val.sec = 1;
	}
	if(timeout_val.sec == 0 && timeout_val.usec < 2000) {
	    timeout_val.usec = 2000;
	    }
	
	Alarm(DEBUG, "---timeout sec: %d; usec: %d\n",
	      timeout_val.sec, timeout_val.usec);

	E_queue(Ses_Reliable_Timeout, ses->sess_id, NULL, timeout_val);
	
	r_data->scheduled_timeout = 1;
    }
    else {
    	Alarm(DEBUG, "+++ No need for a timeout ! tail: %d, head: %d\n",
	      r_data->tail, r_data->head);
    }

    if(Is_link_ack(ses_type)) /* This is an ack packet... */
        return(-1);


    /* Now look at the receiving window */

    if((r_tail->seq_no > r_data->recv_tail)&&
       (r_tail->seq_no - r_data->recv_tail >= MAX_WINDOW)) {
	/* We have more than MAX_WINDOW packets to be reordered.
	 * Just drop the packet, as we can not order it. */

	Alarm(DEBUG, "more than %d packets to be reordered: %d - %d\n", 
	      MAX_WINDOW, r_tail->seq_no, r_data->recv_tail);

	return(0);
    }


    if((r_data->recv_window[r_tail->seq_no%MAX_WINDOW].flag == RECVD_CELL)||
       (r_tail->seq_no < r_data->recv_tail))  {
	/* We already got this message (and probably processed it also)
	 * That's it, we already processed this message, therefore return 0 */
	return(0);
    }


    if(r_tail->seq_no >= r_data->recv_head) {
        r_data->recv_head = r_tail->seq_no + 1;
    }

    if(r_data->last_ack_sent < r_tail->seq_no)
	r_data->unacked_msgs += r_tail->seq_no - (r_data->last_ack_sent - 1);

    /*Alarm(PRINT, "!!! last_ack_sent: %d; seq_no: %d; unacked_msgs: %d\n",
     *	  r_data->last_ack_sent, r_tail->seq_no, r_data->unacked_msgs);
     */

    /* Ok, this is fresh stuff. We should consider it. */
    r_data->recv_window[r_tail->seq_no%MAX_WINDOW].flag = RECVD_CELL;
    
    
    return(1);
}




/***********************************************************/
/* int Reliable_Ses_Send(Session* ses)                     */
/*                                                         */
/* Sends a packet just received from a reliable session    */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:  Session                                           */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (int) status of the buffer                              */
/*                                                         */
/***********************************************************/

int Reliable_Ses_Send(Session* ses) 
{
    udp_header *u_hdr;
    rel_udp_pkt_add *r_add;
    Reliable_Data *r_data;
    Buffer_Cell *buf_cell;
    reliable_tail *r_tail;
    Node* next_hop;
    char *send_buff;
    int16u data_len, ack_len;
    int ret;
    sp_time now, timeout_val, tmp_time, sum_time;
    char *p_nack;
    int32u i;


    now = E_get_time();

    send_buff = ses->data;
    r_data = ses->r_data;
    if(r_data == NULL) {
	Alarm(EXIT, "Reliable_Sess_Send: No reliable data struct !");
    }

    u_hdr = (udp_header*)send_buff;
    r_add = (rel_udp_pkt_add*)(send_buff + sizeof(udp_header));
    r_tail = (reliable_tail*)(send_buff + sizeof(udp_header) + 
			      sizeof(rel_udp_pkt_add) + r_add->data_len);
    data_len = sizeof(udp_header) + sizeof(rel_udp_pkt_add) + r_add->data_len;


    next_hop = Get_Route(My_Address, u_hdr->dest);
    if(next_hop == NULL) {
	return(NO_ROUTE);
    }

    /* See if we can free some space in the buffer/window */
    /*
     * Ses_Send_Much(ses); 
     * 
     */ 

    /* If there is no more room in the window, or the connection is not valid yet, 
     * stick the message in the sending buffer */

    if((r_data->head - r_data->tail >= r_data->window_size)||
       (!stdcarr_empty(&r_data->msg_buff))||
       (!(r_data->flags & CONNECTED_LINK))) {
	if((buf_cell = (Buffer_Cell*) new(BUFFER_CELL))==NULL) {
	    Alarm(EXIT, "Reliable_Send_Control_Msg(): Cannot allocte buffer cell\n");
	}
	buf_cell->data_len = data_len;
	buf_cell->pack_type = r_add->type;
	buf_cell->buff = send_buff;
	stdcarr_push_back(&r_data->msg_buff, &buf_cell);
	inc_ref_cnt(send_buff);
	
	if(stdcarr_size(&r_data->msg_buff) > MAX_BUFF_LINK) {
	    ses->rel_blocked = 1;
	    if(Link_Sessions_Blocked_On == -1) {
		Alarm(DEBUG, "session block\n");
		Block_Session(ses);
	    }
	}

	return(BUFF_OK);
    }   


    /* If I got here it means that I have some space in the window, 
     * so I can go ahead and send the packet */
    if(r_data->head > r_tail->seq_no)
	Alarm(EXIT, "Reliable_Send_Msg(): sending a packet with a smaller seq_no\n");
    
    r_data->window[r_tail->seq_no%MAX_WINDOW].data_len = data_len;
    r_data->window[r_tail->seq_no%MAX_WINDOW].pack_type = r_add->type;
    r_data->window[r_tail->seq_no%MAX_WINDOW].buff = send_buff;
    r_data->window[r_tail->seq_no%MAX_WINDOW].timestamp = now;
    r_data->window[r_tail->seq_no%MAX_WINDOW].resent = 0;
    r_data->head = r_tail->seq_no+1;
    inc_ref_cnt(send_buff);


    /* If there is already an ack to be sent on this link, cancel it, 
     * as this packet will contain the ack info. */
    if(r_data->scheduled_ack == 1) {
	r_data->scheduled_ack = 0;
	E_dequeue(Ses_Send_Ack, ses->sess_id, NULL);
	Alarm(DEBUG, "Ack optimization successfull !!!\n");    
    }


    /* Add NACKs to the reliable tail */
    ack_len = sizeof(reliable_tail); 
    p_nack = (char*)r_tail;
    p_nack += ack_len;
    if(ses->links_used != RELIABLE_LINKS) {
	for(i=r_data->recv_tail; i<r_data->recv_head; i++) {
	    if(ack_len+data_len > sizeof(packet_body) - sizeof(int32))
		break;
	    if(r_data->recv_window[i%MAX_WINDOW].flag == EMPTY_CELL) {
		if(r_data->recv_head - i > 4) {
		    *((int32*)p_nack) = i;
		    p_nack += sizeof(int32);
		    ack_len += sizeof(int32);
		r_data->recv_window[i%MAX_WINDOW].flag = NACK_CELL;
		r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
		Alarm(DEBUG, "NACK sent: %d\n", i);
		}
	    }
	    else if(r_data->recv_window[i%MAX_WINDOW].flag == NACK_CELL) {
		if(r_data->rtt == 0) {
		    tmp_time.sec  = 1;
		    tmp_time.usec = 0;
		}
		else {
		    tmp_time.sec  = r_data->rtt*2/1000000;
		    tmp_time.usec = r_data->rtt*2%1000000;
		}
		sum_time = E_add_time(r_data->recv_window[i%MAX_WINDOW].nack_sent,
				      tmp_time);
		if((sum_time.sec < now.sec)||
		   ((sum_time.sec == now.sec)&&(sum_time.usec < now.usec))) {
		    *((int32*)p_nack) = i;
		    p_nack += sizeof(int32);
		    ack_len += sizeof(int32);
		    r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
		    Alarm(DEBUG, "%%% NACK sent again: %d !\n", i);
		}
	    }
	}
    }

    u_hdr->len += ack_len - sizeof(reliable_tail);
    r_add->ack_len = ack_len;
    r_add->type = Set_endian(REL_UDP_DATA_TYPE);
    
    /* Send the Packet */

    if(ses->links_used == RELIABLE_LINKS) {
	ret = Forward_Rel_UDP_Data(next_hop, send_buff, 
				   u_hdr->len+sizeof(udp_header), 0);
    }
    else {
	ret = Forward_UDP_Data(next_hop, send_buff, 
			       u_hdr->len+sizeof(udp_header));
    }
    
  
    /* Setting the timeout */
    
    if(r_data->scheduled_timeout == 1) {
        E_dequeue(Ses_Reliable_Timeout, ses->sess_id, NULL);
    }

    timeout_val.sec = (r_data->rtt*5)/1000000;
    timeout_val.usec = (r_data->rtt*5)%1000000;
    
    if(timeout_val.sec == 0 && timeout_val.usec == 0) {
	timeout_val.sec = 1;
    }
    if(timeout_val.sec == 0 && timeout_val.usec < 2000) {
	    timeout_val.usec = 2000;
    }
    
    timeout_val.sec  *= r_data->timeout_multiply;
    timeout_val.usec *= r_data->timeout_multiply;
    timeout_val.sec += timeout_val.usec/1000000;
    timeout_val.usec = timeout_val.usec%1000000;
    
    if(timeout_val.sec > 1)
	timeout_val.sec = 1;
    
    
    Alarm(DEBUG, "---timeout sec: %d; usec: %d\n",
	  timeout_val.sec, timeout_val.usec);

    E_queue(Ses_Reliable_Timeout, ses->sess_id, NULL, timeout_val);

    r_data->scheduled_timeout = 1;

    return ret;
}



/***********************************************************/
/* void Ses_Send_Much(Session *ses)                        */
/*                                                         */
/* Tries to send anything in the buffer (if there is room  */
/* available in the window)                                */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses:    Session refering to                             */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Ses_Send_Much(Session *ses) 
{
    udp_header *u_hdr;
    rel_udp_pkt_add *r_add;
    Reliable_Data *r_data;
    Buffer_Cell *buf_cell;
    reliable_tail *r_tail;
    Node* next_hop;
    char *send_buff;
    int16u data_len, ack_len;
    int ret;
    sp_time now, timeout_val, tmp_time, sum_time;
    char *p_nack;
    int32u i, ack_window_mask;
    stdcarr_it it;


    next_hop = Get_Route(My_Address, ses->rel_otherside_addr);
    if(next_hop == NULL) {
	/* I don't have a route to the destination. 
	   It might be temporary */
	return;
    }
    
    now = E_get_time();

    r_data = ses->r_data;
    if(r_data == NULL) {
	Alarm(EXIT, "Reliable_Sess_Send: No reliable data struct !");
    }


    /* Return if the link is not valid yet */
    if(!(r_data->flags & CONNECTED_LINK))
	return;

    /* See if we have room in the window to send anything */
    if(r_data->head - r_data->tail >= r_data->window_size) {
	return;
    }
	
    /* Return if the buffer is empty */
    if(stdcarr_empty(&(r_data->msg_buff))) {
	return;
    }


    /* If there is already an ack to be sent on tihs link, cancel it, 
       as this packet will contain the ack info. */
    if(r_data->scheduled_ack == 1) {
	r_data->scheduled_ack = 0;
	E_dequeue(Ses_Send_Ack, ses->sess_id, NULL);
	Alarm(DEBUG, "Ack optimization successfull !!!\n");
    }

    /* If we got up to here, we do have room in the window. Send what we can */
    while(r_data->head - r_data->tail < r_data->window_size) {
	/* Stop if the buffer is empty */
	if(stdcarr_empty(&(r_data->msg_buff))) {
	    break;
	}

	/* Take the first packet from the buffer (queue) and put it into the window */
	stdcarr_begin(&(r_data->msg_buff), &it);
	buf_cell = *((Buffer_Cell **)stdcarr_it_val(&it));
	stdcarr_pop_front(&(r_data->msg_buff));
	    
	data_len = buf_cell->data_len;
	ack_len = sizeof(reliable_tail);
	send_buff = buf_cell->buff;

	u_hdr = (udp_header*)send_buff;
	r_add = (rel_udp_pkt_add*)(send_buff + sizeof(udp_header));
	r_tail = (reliable_tail*)(send_buff + data_len);
	    
	/* Discard the cell from the buffer */
	dispose(buf_cell);


	/* Set the cummulative ack */
	r_tail->cummulative_ack = r_data->recv_tail;

	if(r_data->head > r_tail->seq_no)
	    Alarm(EXIT, "Ses_Send_Much(): smaller seq_no: %d than head: %d\n",
		  r_tail->seq_no, r_data->head);
	    
	r_data->window[r_tail->seq_no%MAX_WINDOW].data_len  = data_len;
	r_data->window[r_tail->seq_no%MAX_WINDOW].buff      = send_buff;
	r_data->window[r_tail->seq_no%MAX_WINDOW].timestamp = now;
	r_data->window[r_tail->seq_no%MAX_WINDOW].resent = 0;
	r_data->head = r_tail->seq_no+1;


	
	/* Add NACKs to the reliable tail */
	ack_len = sizeof(reliable_tail); 
	p_nack = (char*)r_tail;
	p_nack += ack_len;
	if(ses->links_used != RELIABLE_LINKS) {
	    for(i=r_data->recv_tail; i<r_data->recv_head; i++) {
		if(ack_len+data_len > sizeof(packet_body) - sizeof(int32))
		    break;
		if(r_data->recv_window[i%MAX_WINDOW].flag == EMPTY_CELL) {
		    if(r_data->recv_head - i > 4) {
			*((int32*)p_nack) = i;
			p_nack += sizeof(int32);
			ack_len += sizeof(int32);
			r_data->recv_window[i%MAX_WINDOW].flag = NACK_CELL;
			r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
			Alarm(DEBUG, "NACK sent: %d !\n", i);
		    }
		}
		if(r_data->recv_window[i%MAX_WINDOW].flag == NACK_CELL) {
		    if(r_data->rtt == 0) {
			tmp_time.sec  = 1;
			tmp_time.usec = 0;
		    }
		    else {
			tmp_time.sec  = r_data->rtt*2/1000000;
			tmp_time.usec = r_data->rtt*2%1000000;
		    }
		    sum_time = E_add_time(r_data->recv_window[i%MAX_WINDOW].nack_sent,
					  tmp_time);
		    if((sum_time.sec < now.sec)||
		       ((sum_time.sec == now.sec)&&(sum_time.usec < now.usec))) {
			*((int32*)p_nack) = i;
			p_nack += sizeof(int32);
			ack_len += sizeof(int32);
			r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
			Alarm(DEBUG, "%%% NACK sent again: %d !\n", i);
		    }
		}
	    }
	}


	u_hdr->len += ack_len - sizeof(reliable_tail);
	r_add->ack_len = ack_len;

	if(ses->links_used == RELIABLE_LINKS) {
	    ack_window_mask = r_data->window_size/4;

	    if(ack_window_mask > stdcarr_size(&r_data->msg_buff)) 
		ack_window_mask = stdcarr_size(&r_data->msg_buff);
	    if(ack_window_mask < 1)
		ack_window_mask = 1;
	    if(ack_window_mask > 16)
		ack_window_mask = 16;
	    
	    ack_window_mask--;	    

	    ack_window_mask = ack_window_mask << 12;
	    
	    r_add->type = REL_UDP_DATA_TYPE | ack_window_mask;
	    r_add->type = Set_endian(r_add->type);
	}
	else {
	    r_add->type = Set_endian(REL_UDP_DATA_TYPE);
	}

	/* Send the Packet */
	
	if(ses->links_used == RELIABLE_LINKS) {
	    ret = Forward_Rel_UDP_Data(next_hop, send_buff, 
				 u_hdr->len+sizeof(udp_header), 0);
	}
	else {
	    ret = Forward_UDP_Data(next_hop, send_buff, 
				   u_hdr->len+sizeof(udp_header));
	}
    
    }

    if(ses->rel_blocked == 1) {
	if(stdcarr_size(&(r_data->msg_buff)) < MAX_BUFF_LINK/4) {
	    ses->rel_blocked = 0;
	    if(Link_Sessions_Blocked_On == -1) {
		Alarm(DEBUG, "session unblock\n");
		Resume_Session(ses);
	    }
	}
    }

    if(r_data->scheduled_timeout == 1) {
         E_dequeue(Ses_Reliable_Timeout, ses->sess_id, NULL);
    }

    timeout_val.sec = (r_data->rtt*5)/1000000;
    timeout_val.usec = (r_data->rtt*5)%1000000;
    
    if(timeout_val.sec == 0 && timeout_val.usec == 0) {
	timeout_val.sec = 1;
    }
    if(timeout_val.sec == 0 && timeout_val.usec < 2000) {
	    timeout_val.usec = 2000;
    }
    
    timeout_val.sec  *= r_data->timeout_multiply;
    timeout_val.usec *= r_data->timeout_multiply;
    timeout_val.sec += timeout_val.usec/1000000;
    timeout_val.usec = timeout_val.usec%1000000;
    
    if(timeout_val.sec > 1)
	timeout_val.sec = 1;
    
    
    Alarm(DEBUG, "---timeout sec: %d; usec: %d\n",
	  timeout_val.sec, timeout_val.usec);

    E_queue(Ses_Reliable_Timeout, ses->sess_id, NULL, timeout_val);

    r_data->scheduled_timeout = 1;
}




/***********************************************************/
/* void Ses_Send_Ack(int sesid, void* dummy)               */
/*                                                         */
/* Sends an ACK                                            */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sesid:     ID of the session that sends the ack         */
/* dummy:     Not used                                     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Ses_Send_Ack(int sesid, void* dummy) 
{
    udp_header *u_hdr;
    rel_udp_pkt_add *r_add;
    char *send_buff;
    Reliable_Data *r_data;
    reliable_tail *r_tail;
    Node* next_hop;
    int16u data_len, ack_len;
    Session *ses;
    stdhash_it it;
    sp_time now, tmp_time, sum_time;
    char *p_nack;
    int32u ack_congestion_flag;
    int32u i;


    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_it_is_end(&it)) {
	/* The session is gone */
        return;
    }

    ses = *((Session **)stdhash_it_val(&it));

    if(ses->r_data == NULL) 
	Alarm(EXIT, "Ses_Send_Ack: Not a reliable sesion\n");
    
    r_data = ses->r_data;
    

    Alarm(DEBUG, "Sending ACK: %d\n", r_data->recv_tail);


    next_hop = Get_Route(My_Address, ses->rel_otherside_addr);
    if(next_hop == NULL) {
	/* I don't have a route to the destination. 
	   It might be temporary */
	return;
    }
 
    now = E_get_time();
    
    if(!(r_data->flags & CONNECTED_LINK))
	Alarm(EXIT, "Ses_Send_Ack: Link not valid yet\n");
    
    r_data->scheduled_ack = 0;

    if((send_buff = (char*) new_ref_cnt(PACK_BODY_OBJ))==NULL) {
	Alarm(EXIT, "Ses_Send_Ack: Cannot allocte packet_body object\n");
    }

    u_hdr = (udp_header*)send_buff;
    u_hdr->source = My_Address;
    u_hdr->source_port = ses->port;
    u_hdr->dest = ses->rel_otherside_addr;
    u_hdr->dest_port = ses->rel_otherside_port;
    u_hdr->len = sizeof(rel_udp_pkt_add);

    r_add = (rel_udp_pkt_add*)(send_buff + sizeof(udp_header));
    r_add->type = Set_endian(LINK_ACK_TYPE);
    r_add->data_len = 0;
    r_add->ack_len = sizeof(reliable_tail);
    u_hdr->len += r_add->ack_len;

    /* Setting the reliability tail of the ack */
    r_tail = (reliable_tail*)(send_buff + sizeof(udp_header) + 
			      sizeof(rel_udp_pkt_add));
    r_tail->seq_no = r_data->seq_no;
    r_tail->cummulative_ack = r_data->recv_tail;

    r_data->last_ack_sent = r_data->recv_tail;
    r_data->unacked_msgs = 0;
    

    data_len = 0;
    ack_len = sizeof(reliable_tail); 
    /* Add NACKs to the reliable tail */
    p_nack = (char*)r_tail;
    p_nack += ack_len;

    if(ses->links_used != RELIABLE_LINKS) {
	for(i=r_data->recv_tail; i<r_data->recv_head; i++) {
	    if(ack_len+data_len > sizeof(packet_body) - sizeof(int32))
		break;
	    if(r_data->recv_window[i%MAX_WINDOW].flag == EMPTY_CELL) {
		if(r_data->recv_head - i > 4) {
		    *((int32*)p_nack) = i;
		    p_nack += sizeof(int32);
		    ack_len += sizeof(int32);
		    r_data->recv_window[i%MAX_WINDOW].flag = NACK_CELL;
		    r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
		    Alarm(DEBUG, "NACK sent: %d !\n", i);
		}
	    }
	    else if(r_data->recv_window[i%MAX_WINDOW].flag == NACK_CELL) {
		if(r_data->rtt == 0) {
		    tmp_time.sec  = 1;
		    tmp_time.usec = 0;
		}
		else {
		    tmp_time.sec  = r_data->rtt*2/1000000;
		    tmp_time.usec = r_data->rtt*2%1000000;
		}
		sum_time = E_add_time(r_data->recv_window[i%MAX_WINDOW].nack_sent,
				      tmp_time);
		if((sum_time.sec < now.sec)||
		   ((sum_time.sec == now.sec)&&(sum_time.usec < now.usec))) {
		    *((int32*)p_nack) = i;
		    p_nack += sizeof(int32);
		    ack_len += sizeof(int32);
		    r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
		    Alarm(DEBUG, "%%% NACK sent again: %d !\n", i);
		}
	    }
	}
    }

    u_hdr->len += ack_len - sizeof(reliable_tail);
    r_add->ack_len = ack_len;

    ack_congestion_flag = r_data->congestion_flag << 2;

    /* Send the Packet */

    if(ses->links_used == RELIABLE_LINKS) {
	Forward_Rel_UDP_Data(next_hop, send_buff, 
				   u_hdr->len+sizeof(udp_header), ack_congestion_flag);
    }
    else {
	Forward_UDP_Data(next_hop, send_buff, 
			       u_hdr->len+sizeof(udp_header));
    }

    dec_ref_cnt(send_buff);
}




/***********************************************************/
/* void Ses_Reliable_Timeout(int sesid, void* dummy)       */
/*                                                         */
/* Handles a timeout                                       */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* ses_id:    ID of the link to send on                    */
/* dummy:     Not used                                     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Ses_Reliable_Timeout(int sesid, void *dummy) 
{
    udp_header *u_hdr;
    rel_udp_pkt_add *r_add;
    char *send_buff;
    Reliable_Data *r_data;
    reliable_tail *r_tail;
    int32u pack_type;
    Node* next_hop;
    int16u data_len, ack_len;
    Session *ses;
    stdhash_it it;
    sp_time now, timeout_val, tmp_time, sum_time;
    char *p_nack;
    int32u i, cur_seq;

    
    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_it_is_end(&it)) {
	/* The session is gone */
        return;
    }
    ses = *((Session **)stdhash_it_val(&it));

    if(ses->r_data == NULL) 
	Alarm(EXIT, "Ses_Reliable_Timeout: Not a reliable sesion\n");
    
    r_data = ses->r_data;
    

    next_hop = Get_Route(My_Address, ses->rel_otherside_addr);
    if(next_hop == NULL) {
	/* I don't have a route to the destination. 
	   It might be temporary */
	return;
    }
 
    now = E_get_time();
    
    if(!(r_data->flags & CONNECTED_LINK))
	Alarm(EXIT, "Ses_Reliable_Timeout: Link not valid yet\n");

    /* First see if we have anything in the window */
    if(r_data->head == r_data->tail) {
	Alarm(DEBUG, "Ses_Reliable_Timeout: Nothing to send ! tail=head=%d\n",
	      r_data->head);
	r_data->scheduled_timeout = 0;
	return;
    }
	
    Alarm(DEBUG, "SES_REL_TIMEOUT: tail: %d; head:%d\n", r_data->tail, r_data->head);
 
    /* Congestion control */
    r_data->ssthresh = r_data->window_size/2;
    if(r_data->ssthresh < 1)
	r_data->ssthresh = 1;
    r_data->window_size = 1;

    Alarm(DEBUG, "SES window adjusted: %5.3f timeout\n", r_data->window_size);


    /* If there is already an ack to be sent on this link, cancel it, 
       as this packet will contain the ack info. */
    if(r_data->scheduled_ack == 1) {
	r_data->scheduled_ack = 0;
	E_dequeue(Ses_Send_Ack, ses->sess_id, NULL);
	Alarm(DEBUG, "Ack optimization successfull !!!\n");
    }

    

    /* If we got up to here, we do have smthg in the window. */

    for(cur_seq=r_data->tail; cur_seq<r_data->head; cur_seq++) {

	data_len  = r_data->window[cur_seq%MAX_WINDOW].data_len;
	ack_len   = sizeof(reliable_tail);
	pack_type = r_data->window[cur_seq%MAX_WINDOW].pack_type;
	send_buff = r_data->window[cur_seq%MAX_WINDOW].buff;
	r_data->window[cur_seq%MAX_WINDOW].resent = 1;
		
	u_hdr = (udp_header*)send_buff;
	r_add = (rel_udp_pkt_add*)(send_buff + sizeof(udp_header));
	
	
	r_tail = (reliable_tail*)(send_buff + data_len);
	Alarm(DEBUG, "SES: ((( tail: %d; seq_no: %d\n",
	      r_data->tail, r_tail->seq_no);
	
	
	/* Set the cummulative ack */
	r_tail->cummulative_ack = r_data->recv_tail;
	
	
	/* Add NACKs to the reliable tail */
	ack_len = sizeof(reliable_tail); 
	p_nack = (char*)r_tail;
	p_nack += ack_len;
	
	if(ses->links_used != RELIABLE_LINKS) {
	    for(i=r_data->recv_tail; i<r_data->recv_head; i++) {
		if(ack_len+data_len > sizeof(packet_body) - sizeof(int32))
		    break;
		if(r_data->recv_window[i%MAX_WINDOW].flag == EMPTY_CELL) {
		    if(r_data->recv_head - i > 4) {
			*((int32*)p_nack) = i;
			p_nack += sizeof(int32);
			ack_len += sizeof(int32);
			r_data->recv_window[i%MAX_WINDOW].flag = NACK_CELL;
			r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
			Alarm(DEBUG, "NACK sent: %d !\n", i);
		    }
		}
		if(r_data->recv_window[i%MAX_WINDOW].flag == NACK_CELL) {
		    if(r_data->rtt == 0) {
			tmp_time.sec  = 1;
			tmp_time.usec = 0;
		    }
		    else {
			tmp_time.sec  = r_data->rtt*2/1000000;
			tmp_time.usec = r_data->rtt*2%1000000;
		    }
		    sum_time = E_add_time(r_data->recv_window[i%MAX_WINDOW].nack_sent,
					  tmp_time);
		    if((sum_time.sec < now.sec)||
		       ((sum_time.sec == now.sec)&&(sum_time.usec < now.usec))) {
			*((int32*)p_nack) = i;
			p_nack += sizeof(int32);
			ack_len += sizeof(int32);
			r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
			Alarm(DEBUG, "%%% NACK sent again: %d !\n", i);
		    }
		}
	    }
	}
	
	
	r_add->ack_len = ack_len;
	u_hdr->len = sizeof(rel_udp_pkt_add) + r_add->data_len + r_add->ack_len;
	r_add->type = Set_endian(REL_UDP_DATA_TYPE);
	
	
	/* Send the Packet */
	
	if(ses->links_used == RELIABLE_LINKS) {
	    Forward_Rel_UDP_Data(next_hop, send_buff, 
				 u_hdr->len+sizeof(udp_header), 0);
	}
	else {
	    Forward_UDP_Data(next_hop, send_buff, 
			     u_hdr->len+sizeof(udp_header));
	}
    }

    timeout_val.sec = (r_data->rtt*5)/1000000;
    timeout_val.usec = (r_data->rtt*5)%1000000;
    
    if(timeout_val.sec == 0 && timeout_val.usec == 0) {
	timeout_val.sec = 1;
    }
    if(timeout_val.sec == 0 && timeout_val.usec < 2000) {
	timeout_val.usec = 2000;
    }
    
    /* Increase the timeout exponentially */
    r_data->timeout_multiply *= 2;
    
    if(r_data->timeout_multiply > 100)
	r_data->timeout_multiply = 100;
    
    Alarm(DEBUG, "\n\n! ! Ses_timeout_multiply: %d\n\n", r_data->timeout_multiply);
    
    timeout_val.sec  *= r_data->timeout_multiply;
    timeout_val.usec *= r_data->timeout_multiply;
    timeout_val.sec += timeout_val.usec/1000000;
    timeout_val.usec = timeout_val.usec%1000000;
    
    if(timeout_val.sec > 1)
	timeout_val.sec = 1;
    
    Alarm(DEBUG, "---timeout sec: %d; usec: %d\n",
	  timeout_val.sec, timeout_val.usec);

    E_queue(Ses_Reliable_Timeout, (int)ses->sess_id, NULL, timeout_val);
}




/***********************************************************/
/* void Ses_Send_Nack_Retransm(int sesid, void* dummy)     */
/*                                                         */
/* Answers to a NACK at the session level                  */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sesid:     ID of the session to send on                 */
/* dummy:     Not used                                     */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Ses_Send_Nack_Retransm(int sesid, void *dummy) 
{
    udp_header *u_hdr;
    rel_udp_pkt_add *r_add;
    char *send_buff;
    Reliable_Data *r_data;
    reliable_tail *r_tail;
    int32u pack_type;
    Node* next_hop;
    int16u data_len, ack_len;
    Session *ses;
    stdhash_it it;
    sp_time now, tmp_time, sum_time;
    char *p_nack;
    int j;
    int32u i, nack_seq;

    
    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_it_is_end(&it)) {
	/* The session is gone */
        return;
    }
    ses = *((Session **)stdhash_it_val(&it));

    if(ses->r_data == NULL) 
	Alarm(EXIT, "Ses_Send_Nack_Retr: Not a reliable sesion\n");
    
    r_data = ses->r_data;
    

    next_hop = Get_Route(My_Address, ses->rel_otherside_addr);
    if(next_hop == NULL) {
	/* I don't have a route to the destination. 
	   It might be temporary */
	return;
    }
 
    now = E_get_time();
    
    if(!(r_data->flags & CONNECTED_LINK))
	Alarm(EXIT, "Ses_Send_Nack_Retransm: Link not valid yet\n");

    if((r_data->nack_len == 0)||(r_data->nack_buff == NULL)) {
	Alarm(DEBUG, "Send_Nack_Retransm: Oops, nothing to resend here\n");
	return;
    }

    if(r_data->head == r_data->tail) {
	Alarm(DEBUG, "Send_Nack_Retransm: Nothing to send ! tail=head=%d\n",
	      r_data->head);
	r_data->scheduled_timeout = 0;
	return;
    }
	
    /* Congestion control */
    r_data->ssthresh = r_data->window_size/2;
    if(r_data->ssthresh < 1) {
	r_data->ssthresh = 1;
    }
    r_data->window_size = r_data->window_size/2;
    if(r_data->window_size < 1) {
	r_data->window_size = 1;
    }

    Alarm(DEBUG, "SES window adjusted: %5.3f\n", r_data->window_size);


    /* If there is already an ack to be sent on tis link, cancel it, 
       as these packets will contain the ack info. */
    if(r_data->scheduled_ack == 1) {
	r_data->scheduled_ack = 0;
	E_dequeue(Ses_Send_Ack, (int)ses->sess_id, NULL);
	Alarm(DEBUG, "Ack optimization successfull !!!\n");
    }

    /* Check each nack individually */
	
    for(j=0; j<r_data->nack_len; j += sizeof(int32)) {
	nack_seq = *((int32u*)(r_data->nack_buff+j));
	if(r_data->window[nack_seq%MAX_WINDOW].buff == NULL)
	    continue;

	Alarm(DEBUG, "NACK Resending: %d\n", nack_seq);

	data_len  = r_data->window[nack_seq%MAX_WINDOW].data_len;
	ack_len   = sizeof(reliable_tail);
	pack_type = r_data->window[nack_seq%MAX_WINDOW].pack_type;
	send_buff = r_data->window[nack_seq%MAX_WINDOW].buff;
	r_data->window[nack_seq%MAX_WINDOW].resent = 1;

	u_hdr = (udp_header*)send_buff;
	r_add = (rel_udp_pkt_add*)(send_buff + sizeof(udp_header));
	r_tail = (reliable_tail*)(send_buff + data_len);
	
	/* Set the cummulative ack */
	r_tail->cummulative_ack = r_data->recv_tail;
	    
	/* Add NACKs to the reliable tail */
	p_nack = (char*)r_tail;
	p_nack += ack_len;
	
	if(ses->links_used != RELIABLE_LINKS) {
	    for(i=r_data->recv_tail; i<r_data->recv_head; i++) {
		if(ack_len+data_len > sizeof(packet_body) - sizeof(int32))
		    break;
		if(r_data->recv_window[i%MAX_WINDOW].flag == EMPTY_CELL) {
		    if(r_data->recv_head - i > 4) {
			*((int32*)p_nack) = i;
			p_nack += sizeof(int32);
			ack_len += sizeof(int32);
			r_data->recv_window[i%MAX_WINDOW].flag = NACK_CELL;
			r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
			Alarm(DEBUG, "NACK sent: %d !\n", i);
		    }
		}
		if(r_data->recv_window[i%MAX_WINDOW].flag == NACK_CELL) {
		    if(r_data->rtt == 0) {
			tmp_time.sec  = 1;
			tmp_time.usec = 0;
		    }
		    else {
			tmp_time.sec  = r_data->rtt*2/1000000;
			tmp_time.usec = r_data->rtt*2%1000000;
		    }
		    sum_time = E_add_time(r_data->recv_window[i%MAX_WINDOW].nack_sent,
					  tmp_time);
		    if((sum_time.sec < now.sec)||
		       ((sum_time.sec == now.sec)&&(sum_time.usec < now.usec))) {
			*((int32*)p_nack) = i;
			p_nack += sizeof(int32);
			ack_len += sizeof(int32);
			r_data->recv_window[i%MAX_WINDOW].nack_sent = now;
			Alarm(DEBUG, "%%% NACK sent again: %d !\n", i);
		    }
		}
	    }
	}
	r_add->ack_len = ack_len;
	u_hdr->len = sizeof(rel_udp_pkt_add) + r_add->data_len + r_add->ack_len;
	r_add->type = Set_endian(REL_UDP_DATA_TYPE);
	

	/* Send the Packet */
	
	if(ses->links_used == RELIABLE_LINKS) {
	    Forward_Rel_UDP_Data(next_hop, send_buff, 
				 u_hdr->len+sizeof(udp_header), 0);
	}
	else {
	    Forward_UDP_Data(next_hop, send_buff, 
			     u_hdr->len+sizeof(udp_header));
	}
       
    }

    dispose(r_data->nack_buff);
    r_data->nack_buff = NULL;
    r_data->nack_len = 0;
}


void Ses_Try_to_Send(int sesid, void* dummy) 
{
    Session *ses;
    stdhash_it it;

    
    stdhash_find(&Sessions_ID, &it, &sesid);
    if(stdhash_it_is_end(&it)) {
	/* The session is gone */
        return;
    }
    ses = *((Session **)stdhash_it_val(&it));
    Ses_Send_Much(ses);
}



