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


#ifndef ARCH_PC_WIN95

#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>

#else

#include <winsock.h>

#endif


#include <stdlib.h>
#include <string.h>


#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/data_link.h"
#include "util/memory.h"
#include "stdutil/src/stdutil/stdhash.h"
#include "stdutil/src/stdutil/stddll.h"

#include "objects.h"
#include "net_types.h"
#include "node.h"
#include "link.h"
#include "network.h"
#include "reliable_datagram.h"
#include "state_flood.h"
#include "link_state.h"
#include "hello.h"
#include "protocol.h"
#include "session.h"
#include "multicast.h"
#include "route.h"
#include "kernel_routing.h"

#ifdef SPINES_SSL
/* openssl */
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <assert.h>
#endif

/* Global variables */

extern int16u Port;
extern int16 Num_Initial_Nodes;
extern int16 Num_Discovery_Addresses;
extern int32 Address[MAX_NEIGHBORS];
extern int32 My_Address;
extern int32 Discovery_Address[MAX_DISCOVERY_ADDR];
extern channel Local_Recv_Channels[MAX_LINKS_4_EDGE];
extern sys_scatter Recv_Pack[MAX_LINKS_4_EDGE];
extern sp_time Up_Down_Interval;
extern int network_flag;
extern Prot_Def Edge_Prot_Def;
extern Prot_Def Groups_Prot_Def;
extern stdhash  Monitor_Params;
extern int Schedule_Set_Route;
extern Route* All_Routes;
extern int16 Num_Nodes;
extern int   Accept_Monitor;
extern stdhash All_Nodes;
extern int16 KR_Flags;

/* Statistics */
extern long64 total_received_bytes;
extern long64 total_received_pkts;
extern long64 total_udp_pkts;
extern long64 total_udp_bytes;
extern long64 total_rel_udp_pkts;
extern long64 total_rel_udp_bytes;
extern long64 total_link_ack_pkts;
extern long64 total_link_ack_bytes;
extern long64 total_hello_pkts;
extern long64 total_hello_bytes;
extern long64 total_link_state_pkts;
extern long64 total_link_state_bytes;
extern long64 total_group_state_pkts;
extern long64 total_group_state_bytes;


/* Message delay */
stdhash Pkt_Queue;
int pkt_idx;


/* openssl */
#ifdef SPINES_SSL
extern stdhash SSL_Recv_Queue[MAX_LINKS_4_EDGE];
extern stddll SSL_Resend_Queue;
extern SSL_CTX *ctx_server;
extern SSL_CTX *ctx_client;
extern BIO *bio_tmp;
extern char *Public_Key;
extern char *Private_Key;
extern char *Passphrase;
extern char *CA_Cert;
int Debug_SSL = 0;
#endif
extern int Security;

static const sp_time short_timeout = {0, 50000};  /* 50 milliseconds */
static const sp_time zero_timeout = {0, 0};

#ifdef SPINES_SSL
/* debug functions */
char *ip2str(int ip)
{
    char *str;
    if (!(str = (char *) new(SSL_IP_BUFFER)))
        Alarm(EXIT, "ip2str: cannot allocate buffer\n");
    
    sprintf(str, "%d.%d.%d.%d", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return str;
}

void print_err(int err)
{
    switch (err) {
        case SSL_ERROR_NONE: printf("SSL_ERROR_NONE"); break;
        case SSL_ERROR_ZERO_RETURN: printf("SSL_ERROR_ZERO_RETURN"); break;
        case SSL_ERROR_WANT_READ: printf("SSL_ERROR_WANT_READ"); break;
        case SSL_ERROR_WANT_WRITE: printf("SSL_ERROR_WANT_WRITE"); break;
        case SSL_ERROR_WANT_CONNECT: printf("SSL_ERROR_WANT_CONNECT"); break;
        case SSL_ERROR_WANT_ACCEPT: printf("SSL_ERROR_WANT_ACCEPT"); break;
        case SSL_ERROR_WANT_X509_LOOKUP: printf("SSL_ERROR_WANT_X509_LOOKUP"); break;
        case SSL_ERROR_SYSCALL: printf("SSL_ERROR_SYSCALL"); break;
        case SSL_ERROR_SSL: printf("SSL_ERROR_SSL"); break;
        default: printf("Unknown error code"); break;
    }
    printf("\n");
}
#endif


void	Flip_pack_hdr(packet_header *pack_hdr)
{
    pack_hdr->type	  = Flip_int32( pack_hdr->type );
    pack_hdr->sender_id	  = Flip_int32( pack_hdr->sender_id );
    pack_hdr->data_len	  = Flip_int16( pack_hdr->data_len );
    pack_hdr->ack_len	  = Flip_int16( pack_hdr->ack_len );
    pack_hdr->seq_no	  = Flip_int16( pack_hdr->seq_no );
}

void	Flip_rel_tail(char *buff, int ack_len)
{
    reliable_tail *r_tail;
    int i;
    int32 *nack;
    
    r_tail = (reliable_tail*)buff;
    r_tail->seq_no          = Flip_int32(r_tail->seq_no);
    r_tail->cummulative_ack = Flip_int32(r_tail->cummulative_ack);

    for(i=sizeof(reliable_tail); i<ack_len; i+=sizeof(int32)) {
	nack = (int32*)(buff+i);
	*nack = Flip_int32(*nack);
    }
}



/***********************************************************/
/* void Init_Network(void)                                 */
/*                                                         */
/* First thing that gets called. Initializes the network   */
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

void Init_Network(void) 
{
    network_flag = 1;
    total_received_bytes = 0;
    total_received_pkts = 0;
    total_udp_pkts = 0;
    total_udp_bytes = 0;
    total_rel_udp_pkts = 0;
    total_rel_udp_bytes = 0;
    total_link_ack_pkts = 0;
    total_link_ack_bytes = 0;
    total_hello_pkts = 0;
    total_hello_bytes = 0;
    total_link_state_pkts = 0;
    total_link_state_bytes = 0;
    total_group_state_pkts = 0;
    total_group_state_bytes = 0;

    Num_Nodes = 0;
    All_Routes = NULL;
    Schedule_Set_Route = 0;

    KR_Init();

    Init_My_Node();
    Init_Recv_Channel(CONTROL_LINK);
    Init_Recv_Channel(UDP_LINK);
    Init_Recv_Channel(RELIABLE_UDP_LINK);
    Init_Recv_Channel(REALTIME_UDP_LINK);

    Init_Nodes();
    Init_Connections();
    Init_Session();
    Resend_States(0, &Edge_Prot_Def);
    State_Garbage_Collect(0, &Edge_Prot_Def);
    Resend_States(0, &Groups_Prot_Def);
    State_Garbage_Collect(0, &Groups_Prot_Def);
    /* Uncomment next line to print periodical route updates */
    Print_Edges(0, NULL); 

    pkt_idx = 1;
    stdhash_construct(&Pkt_Queue, sizeof(int32), sizeof(struct Delayed_Packet_d), 
                      NULL, NULL, 0);

}


/***********************************************************/
/* void Init_Recv_Channel(int16 mode)                      */
/*                                                         */
/* Initializes a receiving socket for a link type          */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* mode: type of the link                                  */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void Init_Recv_Channel(int16 mode) 
{
    int ret, i;
    if(mode == CONTROL_LINK) {
	Local_Recv_Channels[mode] = DL_init_channel( RECV_CHANNEL, (int16)(Port+mode), 0, My_Address );
	E_attach_fd(Local_Recv_Channels[mode], READ_FD, Net_Recv, mode, 
		    NULL, HIGH_PRIORITY );
	/* If doing autodiscovery on a multicast address */
	for(i=0;i<Num_Discovery_Addresses;i++) {
	    if (Discovery_Address[i] != 0) { 
		ret = DL_init_channel( RECV_CHANNEL | NO_LOOP, (int16)(Port+mode), Discovery_Address[i], My_Address ); 
		E_attach_fd(ret, READ_FD, Net_Recv, mode, NULL, MEDIUM_PRIORITY );
	    }
	}
    }
    if((mode == UDP_LINK)||(mode == RELIABLE_UDP_LINK)||(mode == REALTIME_UDP_LINK)) {
        Local_Recv_Channels[mode] = DL_init_channel( RECV_CHANNEL, (int16)(Port+mode), 0, My_Address );
	E_attach_fd(Local_Recv_Channels[mode], READ_FD, Net_Recv, mode, 
		    NULL, MEDIUM_PRIORITY );
    }
    Recv_Pack[mode].num_elements = 2;
    Recv_Pack[mode].elements[0].len = sizeof(packet_header);
    Recv_Pack[mode].elements[0].buf = (char *) new_ref_cnt(PACK_HEAD_OBJ);
    Recv_Pack[mode].elements[1].len = sizeof(packet_body);
    Recv_Pack[mode].elements[1].buf = (char *) new_ref_cnt(PACK_BODY_OBJ);
}


/***********************************************************/
/* void Net_Recv(channel sk, int mode, void * dummy_p)     */
/*                                                         */
/* Called by the event system to receive data from socket  */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      socket                                         */
/* mode:    type of the link                               */
/* dummy_p: not used                                       */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

void    Net_Recv(channel sk, int mode, void * dummy_p) 
{
    Read_UDP(sk, mode, &Recv_Pack[mode]);
}

/***********************************************************/
/* void Init_My_Node(void)                                 */
/*                                                         */
/* Initializes the IP address of the daemon                */
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

void Init_My_Node(void) 
{
    struct hostent  *host_ptr;
    char machine_name[256];
    int i;

    if(My_Address == -1) { /* No local address was given in the command line */
	gethostname(machine_name,sizeof(machine_name)); 
	host_ptr = gethostbyname(machine_name);
	
	if(host_ptr == NULL)
	    Alarm( EXIT, "Init_My_Node: could not get my ip address (my name is %s)\n",
		   machine_name );
	if (host_ptr->h_addrtype != AF_INET)
	    Alarm(EXIT, "Init_My_Node: Sorry, cannot handle addr types other than IPv4\n");
	if (host_ptr->h_length != 4)
	    Alarm(EXIT, "Conf_init: Bad IPv4 address length\n");
	
        memcpy(&My_Address, host_ptr->h_addr, sizeof(struct in_addr));
	My_Address = ntohl(My_Address);
    }
    for(i=0;i<Num_Initial_Nodes;i++) {
        if(Address[i] == My_Address)
	    Alarm(EXIT, "Oops ! I cannot conect to myself...\n");
    }
  
}

#ifdef SPINES_SSL
/***********************************************************/
/* void new_ssl_client(int sock, struct sockaddr peer)     */
/*                                                         */
/* Called by Read_UDP when no SSL connection is found      */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sock:    socket                                         */
/* peer:    the address of the client                      */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* a SSL connection                                        */
/*                                                         */
/***********************************************************/

SSL *new_ssl_client(int sock, struct sockaddr peer)
{
    SSL *ssl;
    EVP_PKEY *priv_key;
    BIO *bio_write;
    
    FILE *f;
    
    ssl = SSL_new(ctx_server);
    assert(ssl);
    SSL_CTX_set_mode(ctx_server, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_read_ahead(ctx_server, 1);
    
    SSL_set_accept_state(ssl);
        SSL_set_options(ssl, SSL_OP_COOKIE_EXCHANGE);

    if (!SSL_use_certificate_file(ssl, Public_Key, SSL_FILETYPE_PEM)) {
        printf("SSL_use_certificate_file err\n");
        ERR_print_errors_fp(stderr);
        exit(1);
    }   
    
    /* load private key */
    if (!(f = fopen(Private_Key, "r"))) {
        printf("Cannot open %s\n", Private_Key);
        exit(1);
    }   
    
    if (Passphrase)
        priv_key = PEM_read_PrivateKey(f, NULL, NULL, Passphrase);
    else
        priv_key = PEM_read_PrivateKey(f, NULL, NULL, NULL);
    
    if (!priv_key) {
        printf("private key  err\n");
	ERR_print_errors_fp(stderr);
	exit(1);
    }   
    
    if (!SSL_use_PrivateKey(ssl, priv_key)) {
        printf("SSL_use_PrivateKey err\n");
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    /* This part is disable due to a bug in DTLS... */
    /*
    if (!SSL_CTX_load_verify_locations(ctx, CA_Cert, NULL)) {
        printf("SSL_CTX_load_verify_locations err\n");
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    */
    bio_write = BIO_new_dgram(sock, BIO_NOCLOSE);
    assert(bio_write);

    BIO_dgram_set_peer(bio_write, &peer);

    SSL_set_bio(ssl, NULL, bio_write);

    return ssl;
}
#endif

/***********************************************************/
/* void Read_UDP(channel sk, int mode, sys_scatter *scat)  */
/*                                                         */
/* Receives data from a socket                             */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* sk:      socket                                         */
/* mode:    type of the link                               */
/* scat:    scatter to receive data into                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

int    Read_UDP(channel sk, int mode, sys_scatter *scat)
{
    int	received_bytes;
    packet_header *pack_hdr;
    sp_time now = {0, 0};
    sp_time diff;
    int32 sender;
    stdit it;
    double chance = 100.0;
    double test = 0.0;
    double p1, p01, p11;
    Lk_Param *lkp = NULL;
    Delayed_Packet dpkt;
    stdit pkt_it;
    long long tokens;
    int total_pkt_bytes;
    int32 type;


#ifdef SPINES_SSL
    struct sockaddr_in peer;
    SSL *ssl;
    BIO *bio_read;

    unsigned char *pt;
    char pseudo_scat[MAX_PACKET_SIZE];
    int err, ret, total_len, i, tmp;
    int num_bytes, real_size; // hack 4 openssl
    int new_client;

    int sender_id, linkid;
	stdit it_node;    
    
    Node *nd;

    
    
    if (!Security) 
#endif
 	received_bytes = DL_recv(sk, scat);
#ifdef SPINES_SSL
    else {

        /********************************************/
	/* openssl
	 * if there is no SSL * create a new one
	 */

	if (Debug_SSL)
		Alarm(PRINT, "--------------------------------------\n");
	received_bytes = DL_recv_enh(sk, scat, &peer);
	stdhash_find(&SSL_Recv_Queue[mode], &it, &(peer.sin_addr.s_addr));
	new_client = 0;

	if (Debug_SSL)
		print_SSL_Recv_Queue(mode);
	if (stdhash_is_end(&SSL_Recv_Queue[mode], &it)) {
	    /* new ssl "client" */
	    ssl = new_ssl_client(sk, *((struct sockaddr *)&peer));	
            if (Debug_SSL) 
		    printf("Read_UDP: new client: %s:%d\n",
			   ip2str(ntohl(peer.sin_addr.s_addr)),
			   ntohs(peer.sin_port));

	    stdhash_insert(&SSL_Recv_Queue[mode], &it, &(peer.sin_addr.s_addr), &ssl);
	    new_client = 1;
	    if (Debug_SSL)
		    print_SSL_Recv_Queue(mode);
	} else {
            if (Debug_SSL)
		    printf("Read_UDP: client found: %s:%d\n",
			   ip2str(ntohl(peer.sin_addr.s_addr)),
			   ntohs(peer.sin_port));
	    ssl = *((SSL **) stdhash_it_val(&it));
	}

	real_size = received_bytes;
	/* pack the scat struct in a buffer */
	for (i = 0, total_len=0; i < scat->num_elements; i++) {
	    num_bytes = scat->elements[i].len < real_size ? scat->elements[i].len : real_size;
 
	    if (Debug_SSL) {
		    printf("Read_UDP: scat elem %d: %d\n", i, scat->elements[i].len);
		    printf("Read_UDP: real size %d: %d\n", i, real_size);
            }
	    memcpy(&pseudo_scat[total_len], scat->elements[i].buf, num_bytes);
	    total_len += num_bytes;
	    real_size -= num_bytes;
	}
	if (Debug_SSL)
		Alarm(PRINT, "Read_UDP: total_len = %d\n", total_len);

	SSL3_RECORD *rr = (SSL3_RECORD *) &pseudo_scat;
	if (Debug_SSL)
		Alarm(PRINT, "Read_UDP: rr->type = %d\n", (rr->type) & 0xFF);
	if (new_client) {
	    /* the client is new but no handshake received
	     * the packet cannot be decrypted, so it's ignored and the client is erased hoping
	     * that he will send a tls handshake at some point
	     */

	    /* ignore the packet if no tls handshake received */
	    if ((rr->type & 0xFF) != SSL3_RT_HANDSHAKE) {
		if (Debug_SSL)
		        Alarm(PRINT, "Read_UDP: handshake error: helloclient expected... Packet ignored\n");
		// stdhash_find(&SSL_Recv_Queue[mode], &it, &peer);
		stdhash_find(&SSL_Recv_Queue[mode], &it, &(peer.sin_addr.s_addr));
		if (!stdhash_is_end(&SSL_Recv_Queue[mode], &it)) {
		    if (Debug_SSL)
			Alarm(PRINT, "client found - erasing it\n");
		    stdhash_erase(&SSL_Recv_Queue[mode], &it);
		    if (Debug_SSL)
			print_SSL_Recv_Queue(mode);
		} else {
		    if (Debug_SSL)
			    Alarm(PRINT, "client not found - internal error!\n");
		}

		/* yet another hack
		 * if the server crashes and comes back, he receives encrypted packets from the client
		 * and, if he only ignores them, he will never know about the client; to fix this
		 * I check if there is a node for the client and if not, I added it...
		 */

		if (Debug_SSL)
			print_All_Nodes();
		// stdhash_find(&All_Nodes, &it_node, &(peer.sin_addr.s_addr)); // todo: check if ntohl required or not
		tmp = ntohl(peer.sin_addr.s_addr);
		stdhash_find(&All_Nodes, &it_node, &tmp);

		if (stdhash_is_end(&All_Nodes, &it_node)) {
		    sender_id = ntohl(peer.sin_addr.s_addr);
		    if (Debug_SSL)
			    Alarm(PRINT, "Read_UDP: client doesn't exist in All_Nodes - create it\n");
		    Create_Node(sender_id, NEIGHBOR_NODE | NOT_YET_CONNECTED_NODE);
		    if (Debug_SSL)
			    print_All_Nodes();
		    
		    stdhash_find(&All_Nodes, &it_node, &sender_id);
		    if (stdhash_is_end(&All_Nodes, &it_node))
		         Alarm(EXIT, "Read_UDP; could not create node\n");

		    nd = *((Node **)stdhash_it_val(&it_node));
		    nd->last_time_heard = E_get_time();
		    nd->counter = 0;        
		        
		    linkid = Create_Link(sender_id, CONTROL_LINK);
    		    E_queue(Send_Hello, linkid, NULL, zero_timeout);
		} else {
		    if (Debug_SSL)
			    Alarm(PRINT, "Read_UDP: client exists in All_Nodes - continue\n");
		}
		return 0; 
	    }	
	} else {
	    /* the client went down and came back so he sent a helloclient message
	     * instead of application data
	     * a new client is created and ALL tls connections for sending must be reseted
	     */
	    if ((!SSL_in_init(ssl)) && ((rr->type & 0xFF) == SSL3_RT_HANDSHAKE)) {
		if (Debug_SSL)
		        Alarm(PRINT, "Read_UDP: helloclient received... \n");
		/* the client must be searched by the IP because he may come back with another (local) port
		 * all instances must be elimitated...
		 */
		stdhash_find(&SSL_Recv_Queue[mode], &it, &(peer.sin_addr.s_addr));
		if (!stdhash_is_end(&SSL_Recv_Queue[mode], &it)) {
		    if (Debug_SSL)
			    Alarm(PRINT, "client found - replacing it\n");
		    stdhash_erase(&SSL_Recv_Queue[mode], &it);
                
		    ssl = new_ssl_client(sk, *((struct sockaddr *)&peer));
		    if (Debug_SSL)
			    printf("Read_UDP: new client: %s:%d\n",
				   ip2str(ntohl(peer.sin_addr.s_addr)),
				   ntohs(peer.sin_port));
		    // stdhash_insert(&SSL_Recv_Queue[mode], &it, &peer, &ssl);
		    stdhash_insert(&SSL_Recv_Queue[mode], &it, &(peer.sin_addr.s_addr), &ssl);

		    /* since the peer went down I should reset all outgoing ssl connections
		     * (the peer cannot decrypt them)
		     */

				
		    // Remove_send_SSL(ntohl(peer.sin_addr.s_addr));
		} else {
		    if (Debug_SSL)
			    Alarm(PRINT, "client not found - internal error!\n");
		}
	    }
	}

	/* send the data to the ssl through a buffer */
	bio_read = BIO_new_mem_buf(pseudo_scat, total_len);
	assert(bio_read);
	BIO_set_mem_eof_return(bio_read, -1);

	assert(ssl);
	ssl->rbio = bio_read;

	if (!(pt = (unsigned char *) new(SSL_PKT_BUFFER)))
	    Alarm(EXIT, "Read_UDP: cannot allocate buffer\n");

	/* SSL_read to get the plain text data */
	ret = SSL_read(ssl, pt, MAX_PACKET_SIZE);
	err = SSL_get_error(ssl, ret);

	BIO_free(ssl->rbio);
	ssl->rbio = bio_tmp;

	if (ret <= 0) {
 	    if (Debug_SSL) {
		    printf("Read_UDP: SSL_read error: ");
		    print_err(err);
                    ERR_print_errors_fp(stderr);
            }

	    // if there is an error than reset the connection...
	    if (err == SSL_ERROR_SSL) {
	        stdhash_find(&SSL_Recv_Queue[mode], &it, &(peer.sin_addr.s_addr));
		if (!stdhash_is_end(&SSL_Recv_Queue[mode], &it)) {
		    if (Debug_SSL)
			    Alarm(PRINT, "Read_UDP: client found - deleting it [invalid data!]\n");
		    stdhash_erase(&SSL_Recv_Queue[mode], &it);
		} 
		dispose(pt);
		return 0;
	    }
	    // exit(1);
	} else {
	    if (Debug_SSL)
		    printf("Read_UDP: %d bytes received: \n", ret);
	    /* convert it back to a scat struct ... */
	    real_size = ret;
	    total_len = 0;
	    for (i = 0; i < scat->num_elements; i++) {
	        num_bytes = scat->elements[i].len < real_size ? scat->elements[i].len : real_size;
		memset(scat->elements[i].buf, 0 , scat->elements[i].len);
		memcpy(scat->elements[i].buf, pt + total_len, num_bytes);
		real_size -= num_bytes;
		total_len += num_bytes;
	    }
	    received_bytes = ret;
	    dispose(pt);
	}
	
    } 

    /* end of openssl stuff */
    /********************************************/
#endif    
    pack_hdr = (packet_header *)scat->elements[0].buf;
    type = pack_hdr->type;
    if(!Same_endian(type)) {
	Flip_pack_hdr(pack_hdr);
	if(pack_hdr->ack_len >= sizeof(reliable_tail)) {
	    /* The packet has a relieble tail */
	    Flip_rel_tail(scat->elements[1].buf + pack_hdr->data_len, pack_hdr->ack_len);
	}
    }
   



    if (Discovery_Address != 0 && pack_hdr->sender_id == My_Address) {
	/* I can hear my own discovery packets.  Discard */
	return(0);
    }
    if(Accept_Monitor == 1) {
	/* Check for monitor injected losses */
	sender = pack_hdr->sender_id;
	stdhash_find(&Monitor_Params, &it, &sender);
	if (!stdhash_is_end(&Monitor_Params, &it)) {
	    
	    lkp = (Lk_Param*)stdhash_it_val(&it);
	    
	    Alarm(DEBUG, "loss: %d; burst: %d; was_loss %d; test: %5.3f; chance: %5.3f\n",
		  lkp->loss_rate, lkp->burst_rate, lkp->was_loss, test, chance);
	    
	    now = E_get_time();

	    if(lkp->bandwidth > 0) {
		diff = E_sub_time(now, lkp->last_time_add);
		if(diff.sec > 10) {
		    tokens = 10000000;
		}
		else {
		    tokens = diff.sec*1000000;
		}
		tokens += diff.usec;
		tokens *=lkp->bandwidth;
		tokens /= 1000000;
		tokens += lkp->bucket;
		if(tokens > BWTH_BUCKET) {
		    tokens = BWTH_BUCKET;
		}
		lkp->bucket = (int32)tokens;
		lkp->last_time_add = now;
		/* Emulate mbuf size */
                total_pkt_bytes = received_bytes + received_bytes%256;
                /* 64 bytes for UDP header */
                total_pkt_bytes += 64; 

                if(lkp->bucket <= MAX_PACKET_SIZE) {
		    Alarm(DEBUG, "Dropping message: "IPF" -> "IPF"\n", IP(sender), IP(My_Address));
		    return(0);
		}
		else {
		    lkp->bucket -= total_pkt_bytes*8;
		}
	    }

	    srand(now.usec);
	    
	    chance = rand();
	    chance /= RAND_MAX;
	    
	    p1  = lkp->loss_rate;
	    p1  = p1/1000000.0;
	    
	    if(lkp->burst_rate == 0) {
		p01 = p11 = p1;
	    }
	    else if((lkp->burst_rate == 1000000)|| /* burst rate of 100% or */
		    (lkp->loss_rate == 1000000)) { /* loss rate of 100% */
		p01 = p11 = 1.0;
	    }
	    else {
		p11 = lkp->burst_rate;
		p11 = p11/1000000.0;
		p01 = p1*(1-p11)/(1-p1);	      
	    }
	    
	    if(lkp->was_loss > 0) {
		test = p11;
	    }
	    else {
		test = p01;
	    }
	} 
    }


    if((network_flag == 1)&&(chance >= test)) {
	if(lkp != NULL) {
	    if((lkp->delay.sec != 0)||(lkp->delay.usec != 0)) {
		dpkt.header = scat->elements[0].buf;
		dpkt.header_len = scat->elements[0].len;
		dpkt.buff = scat->elements[1].buf;
		dpkt.buf_len = received_bytes - scat->elements[0].len;
		dpkt.type = type;
		dpkt.schedule_time = E_add_time(now, lkp->delay);

		stdhash_insert(&Pkt_Queue, &pkt_it, &pkt_idx, &dpkt);
		
		scat->elements[0].buf = (char *) new_ref_cnt(PACK_HEAD_OBJ);
		scat->elements[1].buf = (char *) new_ref_cnt(PACK_BODY_OBJ);
		E_queue(Proc_Delayed_Pkt, pkt_idx, NULL, lkp->delay);
		pkt_idx++;		
	    }
	    else {
		Prot_process_scat(scat, received_bytes, mode, type);
	    }
	}
	else {
	    Prot_process_scat(scat, received_bytes, mode, type);
	}

	total_received_bytes += received_bytes;
	total_received_pkts++;
	if(lkp != NULL) {
	    lkp->was_loss = 0;
	}
    }
    else {
	received_bytes = 0;
	if(lkp != NULL) {
	    lkp->was_loss = 1;
	}
    }
    return received_bytes;
}


void Up_Down_Net(int dummy_int, void *dummy_p)
{
    network_flag = 1 - network_flag;
    E_queue(Up_Down_Net, 0, NULL, Up_Down_Interval);
}

void Graceful_Exit(int dummy_int, void *dummy_p)
{
    Alarm(PRINT, "\n\n\nUDP\t%9lld\t%9lld\n", total_udp_pkts, total_udp_bytes);
    Alarm(PRINT, "REL_UDP\t%9lld\t%9lld\n", total_rel_udp_pkts, total_rel_udp_bytes);
    Alarm(PRINT, "ACK\t%9lld\t%9lld\n", total_link_ack_pkts, total_link_ack_bytes);
    Alarm(PRINT, "HELLO\t%9lld\t%9lld\n", total_hello_pkts, total_hello_bytes);
    Alarm(PRINT, "LINK_ST\t%9lld\t%9lld\n", total_link_state_pkts, total_link_state_bytes);
    Alarm(PRINT, "GRP_ST\t%9lld\t%9lld\n", total_group_state_pkts, total_group_state_bytes);
    Alarm(PRINT,  "TOTAL\t%9lld\t%9lld\n", total_received_pkts, total_received_bytes);
    exit(1);
}

void Proc_Delayed_Pkt(int idx, void *dummy_p)
{
    int received_bytes;
    sys_scatter scat;
    stdit pkt_it;
    Delayed_Packet *dpkt;
    sp_time now, diff;
    int32 type;

    stdhash_find(&Pkt_Queue, &pkt_it, &idx);
    if(stdhash_is_end(&Pkt_Queue, &pkt_it)) {
	return;
    }
    dpkt = (Delayed_Packet *)stdhash_it_val(&pkt_it);

    now = E_get_time();
    diff = E_sub_time(now, dpkt->schedule_time);
    if((diff.usec > 5000)||(diff.sec > 0)) {
	Alarm(DEBUG, "\n\nDelay error: %d.%06d sec\n\n", 
	      diff.sec, diff.usec);
    }

    scat.num_elements = 2;
    scat.elements[0].len = dpkt->header_len;
    scat.elements[0].buf = dpkt->header;
    scat.elements[1].len = dpkt->buf_len;
    scat.elements[1].buf = dpkt->buff;
    type = dpkt->type;
    


    received_bytes = scat.elements[0].len + scat.elements[1].len;

    Prot_process_scat(&scat, received_bytes, 0, type);

    dec_ref_cnt(scat.elements[0].buf);
    dec_ref_cnt(scat.elements[1].buf);

    stdhash_erase(&Pkt_Queue, &pkt_it);
}

#ifdef SPINES_SSL
SSL *new_ssl_server(int sock, struct sockaddr peer)
{
    SSL *ssl;
    BIO *bio_write;

    /* trying to read from this BIO always returns retry */
    BIO_set_mem_eof_return(bio_tmp, -1);

    ssl = SSL_new(ctx_client);
    assert(ssl);
    SSL_set_connect_state(ssl);

    bio_write = BIO_new_dgram(sock, BIO_NOCLOSE);
    assert(bio_write);
    BIO_dgram_set_peer(bio_write, &peer);

    SSL_set_bio(ssl, bio_tmp, bio_write);

    return ssl;
}

/* DL_send replacement */
int DL_send_SSL(channel chan, int mode, int32 address, int16 port, sys_scatter *scat)
{
    stdit it, it2;
    struct sockaddr_in *peer;
    static char pseudo_scat[MAX_PACKET_SIZE];
    int i, total_len, ret, err;
    SSL *ssl;
    // struct send_item *si;

    /* Check that the scatter passed is small enough to be a valid system scatter */
    assert(scat->num_elements <= ARCH_SCATTER_SIZE);

    if (!(peer = (struct sockaddr_in *) new(SSL_SOCKADDR_IN)))
      Alarm(EXIT, "DL_send_SSL: cannot allocate buffer\n");

    peer->sin_family  = AF_INET;
    peer->sin_addr.s_addr = htonl(address);
    peer->sin_port = htons(port);
	
    total_len = 0;
    for (i = 0, total_len=0; i < scat->num_elements; i++) {
        memcpy(&pseudo_scat[total_len], scat->elements[i].buf, scat->elements[i].len);
        total_len += scat->elements[i].len;
    }
   
    if (Debug_SSL) 
	    Alarm(PRINT, "--------------------------------------\n");	

//    for (ret = -10, num_try = 0; ret < 0 && num_try < 10; num_try++) {

    /* openssl
     * if there is not SSL * then create a new one
     */
    if (Debug_SSL)
	    print_SSL_Recv_Queue(mode);
    stdhash_find(&SSL_Recv_Queue[mode], &it, &(peer->sin_addr.s_addr));

    if (stdhash_is_end(&SSL_Recv_Queue[mode], &it)) {
        /* new ssl "server", create a new entry in SSL_Send_Queue */
        ssl = new_ssl_server(chan, *((struct sockaddr *)peer));
	if (Debug_SSL)
		printf("DL_send_SSL: new server: %s:%d\n",
		     ip2str(ntohl(peer->sin_addr.s_addr)),
		     ntohs(peer->sin_port));
	
	stdhash_insert(&SSL_Recv_Queue[mode], &it, &(peer->sin_addr.s_addr), &ssl);
	// print SSL_Send_Queue	
	i = 0;
	stdhash_begin(&SSL_Recv_Queue[mode], &it2);
	while (!stdhash_is_end(&SSL_Recv_Queue[mode], &it2)) {
	    i++;
	    stdhash_it_next(&it2);
	}
	if (Debug_SSL) {
		printf("SSL_Recv_Queue stdhash size: %d\n", i);
		print_SSL_Recv_Queue(mode);
	}
			
	// schedule a timeout event
	if (Debug_SSL) {
		Alarm(PRINT, "Handshake_Timeout event\n");
		Alarm(PRINT, "DL_send_SSL: debug: %X\n", peer);
	}
	E_queue(Handshake_Timeout, (int) peer, 0, short_timeout);
    } else {
	if (Debug_SSL)
        	printf("DL_send_SSL: server found %s:%d\n",
			ip2str(ntohl(peer->sin_addr.s_addr)), 
			ntohs(peer->sin_port));
	
	ssl = *((SSL **) stdhash_it_val(&it));
    }

    /* check this later
       if (SSL_in_init(ssl)) {
       Alarm(PRINT, "DL_send_SSL: in handshake... - sending aborted\n");
       return 0;
       }
    */

    /* openssl */
    ret = SSL_write(ssl, pseudo_scat, total_len);
    err = SSL_get_error(ssl, ret);
        
    if (ret < 0) {
	if (Debug_SSL) {
	        printf("DL_send_SSL: SSL_write error: ");
		print_err(err);
		ERR_print_errors_fp(stderr);
	}
	
	if (err == SSL_ERROR_WANT_READ) {
	    if (Debug_SSL)
		    Alarm(PRINT, "DURING HANDSHAKE\n");
	    /* buffer the packet and schedule an event for retrying */
	    /*
	      si = (struct send_item *) malloc(sizeof(struct send_item));
	      si->ssl = ssl;	
	      si->size = total_len;
	      memcpy(si->pseudo_scat, pseudo_scat, total_len);
	      
	      if (stddll_push_back(&SSL_Resend_Queue, si)) {
	      Alarm(PRINT, "stddll_push_back failed\n");
	      exit(1);
	      } else
	      Alarm(PRINT, "DL_send_SSL: stddll_push_back ok\n");
	      
	      E_queue(Resend_SSL, 0, NULL, short_timeout);
	    */
	}
    } else
	if (Debug_SSL) 
	        printf("DL_send_SSL: %d bytes sent\n", ret);
/*
    if (ret < 0) {
#ifndef _WIN32_WCE
        send_errormsg = strerror(errno);
#endif
        Alarm(DATA_LINK, "DL_send: delaying after failure in send to %d.%d.%d.%d, ret is %d\n",
	      IP1(address), IP2(address), IP3(address), IP4(address), ret);
	select( 0, 0, 0, 0, (struct timeval *)&select_delay);
   }
*/

    if (ret < 0) {
	if (Debug_SSL)
		Alarm( PRINT, "DL_send_SSL: error ssending %d bytes on channel %d to address %d.%d.%d.%d\n",
				total_len, chan, IP1(address), IP2(address), IP3(address), IP4(address) );
    } else if (ret < total_len){
	if (Debug_SSL)
	        Alarm( PRINT, "DL_send_SSL: partial sending %d out of %d\n", ret,total_len);
    } else
	if (Debug_SSL)
		Alarm( PRINT, "DL_send_SSL: sent a message of %d bytes to (%d.%d.%d.%d,%d) on channel %d\n",
			ret,IP1(address), IP2(address),IP3(address), IP4(address),port,chan);

    if (Debug_SSL)
	Alarm(PRINT, "--------------------------------------\n");
    return(ret);
}

/* this function is temporary not used */
void Resend_SSL(int dummy_int, void* p_data)
{
    stdit list_it;
    struct send_item *si;
    int ret, err;

    if (Debug_SSL)
	    Alarm(PRINT, "Resend_SSL\n");
    stddll_begin(&SSL_Resend_Queue, &list_it);
    while (!stddll_is_end(&SSL_Resend_Queue, &list_it)) {
        si = ((struct send_item *) stddll_it_val(&list_it));
	
	if (si) {
	    ret = SSL_write(si->ssl, si->pseudo_scat, si->size);
    	    err = SSL_get_error(si->ssl, ret);
	    
	    if (ret > 0) {
	        stddll_erase(&SSL_Resend_Queue, &list_it);
	    } else if (err == SSL_ERROR_WANT_READ) {
		if (Debug_SSL) {
		        printf("Resend_SSL: SSL_write error: ");
    		        print_err(err);
			ERR_print_errors_fp(stderr);
			stddll_it_next(&list_it);
		}
	    }
    	}
    }

    stddll_begin(&SSL_Resend_Queue, &list_it);
    if (!stddll_is_end(&SSL_Resend_Queue, &list_it)) {
        E_queue(Resend_SSL, 0, NULL, short_timeout);
    }
}

/***********************************************************/
/* void Handshake_Timeout(int param, void* dummy)          */
/*                                                         */
/* Called by the event system if no response is received   */
/* for the Handshake message                               */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* param:   the address of the peer                        */
/* dummy:   not used                                       */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/
void Handshake_Timeout(int param, void* dummy)
{
    stdit it;
    SSL *ssl;
    struct sockaddr_in *peer = (struct sockaddr_in *) param;

    if (Debug_SSL) {
	    Alarm(PRINT, "Handshake_Timeout\n");
	    Alarm(PRINT, "Handshake_Timeout: debug: %X\n", param);
	    Alarm(PRINT, "Handshake_Timeout: debug: %X\n", peer);
    }
    
    // stdhash_find(&SSL_Send_Queue, &it, peer);

    // small hack to find to find the mode....
    if (Debug_SSL)
	    Alarm(PRINT, "Handshake_Timeout: port = %d\n", ntohs(peer->sin_port) - Port);
    stdhash_find(&SSL_Recv_Queue[ntohs(peer->sin_port) - Port], &it, &(peer->sin_addr.s_addr));
    if (stdhash_is_end(&SSL_Recv_Queue[ntohs(peer->sin_port) - Port], &it)) {
	if (Debug_SSL)
	        printf("Handshake_Timeout: server not found: %s:%d\n", ip2str(ntohl(peer->sin_addr.s_addr)), ntohs(peer->sin_port));
    } else {
	if (Debug_SSL)
	        printf("Handshake_Timeout: server found %s:%d\n", ip2str(ntohl(peer->sin_addr.s_addr)), ntohs(peer->sin_port));
	ssl = *((SSL **) stdhash_it_val(&it));
	if (SSL_in_init(ssl)) {
	    if (Debug_SSL)
		    printf("Handshake_Timeout: handshake aborted\n");
	    stdhash_erase(&SSL_Recv_Queue[ntohs(peer->sin_port) - Port], &it); 
	} else {
	    if (Debug_SSL)
		    printf("Handshake_Timeout: handshake done\n");
	}
    }
}

/* debug functions */
void print_SSL_Recv_Queue(int mode)
{
    stdit it;
    int address;

    Alarm(PRINT, "\t\t\t=== print_SSL_Recv_Queue[%d] in ===\n", mode);
    stdhash_begin(&SSL_Recv_Queue[mode], &it);

    while (!stdhash_is_end(&SSL_Recv_Queue[mode], &it)) {
        address = *((int *) stdhash_it_key(&it));
	Alarm(PRINT, "\t\t\t\t%s\n", ip2str(ntohl(address)));
	stdhash_it_next(&it);
    }

    Alarm(PRINT, "\t\t\t=== print_SSL_Recv_Queue[%d] out ===\n", mode);
}

void print_All_Nodes()
{
    stdit it;
    int address;
    
    Alarm(PRINT, "\t\t\t=== print_All_Nodes in ===\n");
    stdhash_begin(&All_Nodes, &it);

    while (!stdhash_is_end(&All_Nodes, &it)) {
        address = *((int *) stdhash_it_key(&it));
	Alarm(PRINT, "\t\t\t\t%s\n", ip2str(address));
	stdhash_it_next(&it);
    }

    Alarm(PRINT, "\t\t\t=== print_All_Nodes out ===\n");
}
#endif
