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



#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>


#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/memory.h"
#include "util/data_link.h"
#include "stdutil/stdhash.h"
#include "stdutil/stdcarr.h"
#include "stdutil/stddll.h"

#include "net_types.h"
#include "node.h"
#include "link.h"
#include "state_flood.h"
#include "network.h"
#include "udp.h"
#include "reliable_udp.h"
#include "session.h"
#include "objects.h"
#include "link_state.h"
#include "route.h"
#include "multicast.h"
#include "kernel_routing.h"

#include "errno.h"

#ifdef	ARCH_PC_WIN95
#include	<winsock.h>
WSADATA		WSAData;
#else
#include <sys/ioctl.h>
#include <net/if.h>
#endif	/* ARCH_PC_WIN95 */


#ifdef SPINES_SSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#endif

/* Global Variables */

/* Startup */
int16u	 Port;
int32    Address[MAX_NEIGHBORS];
int32    My_Address;
int32    Discovery_Address[MAX_DISCOVERY_ADDR];

/* Nodes and direct links */
int16    Num_Initial_Nodes;
int16    Num_Discovery_Addresses;
Node*    Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
int16    Num_Neighbors;
int16    Num_Nodes;
stdhash  All_Nodes;
Link*    Links[MAX_LINKS];
channel  Local_Recv_Channels[MAX_LINKS_4_EDGE];  
channel  Ses_UDP_Send_Channel;
channel  Ses_UDP_Recv_Channel;
sys_scatter Recv_Pack[MAX_LINKS_4_EDGE];
Route*   All_Routes;

stdhash  Monitor_Params;
int      Accept_Monitor;
int      Wireless;

/* Sessions */
stdhash  Sessions_ID;
stdhash  Sessions_Port;
stdhash  Rel_Sessions_Port;
stdhash  Sessions_Sock;
int16    Link_Sessions_Blocked_On; 
stdhash  Neighbors;


/* Link State */
stdhash  All_Edges;
stdhash  Changed_Edges;
Prot_Def Edge_Prot_Def = {
    Edge_All_States, 
    Edge_All_States_by_Dest, 
    Edge_Changed_States, 
    Edge_State_type,
    Edge_State_header_size,
    Edge_Cell_packet_size,
    Edge_Is_route_change,
    Edge_Is_state_relevant,
    Edge_Set_state_header,
    Edge_Set_state_cell,
    Edge_Process_state_header,
    Edge_Process_state_cell,   
    Edge_Destroy_State_Data  
};


/* Multicast */
stdhash  All_Groups_by_Node; 
stdhash  All_Groups_by_Name; 
stdhash  Changed_Group_States;
Prot_Def Groups_Prot_Def = {
    Groups_All_States, 
    Groups_All_States_by_Name, 
    Groups_Changed_States, 
    Groups_State_type,
    Groups_State_header_size,
    Groups_Cell_packet_size,
    Groups_Is_route_change,
    Groups_Is_state_relevant,
    Groups_Set_state_header,
    Groups_Set_state_cell,
    Groups_Process_state_header,
    Groups_Process_state_cell,   
    Groups_Destroy_State_Data  
};

#ifdef SPINES_SSL
/* openssl */
sys_scatter Recv_Pack_Sender[MAX_LINKS_4_EDGE];
stdhash SSL_Recv_Queue[MAX_LINKS_4_EDGE];
stddll SSL_Resend_Queue;
SSL_CTX *ctx_server;
SSL_CTX *ctx_client;
BIO *bio_tmp;
char    *Public_Key;
char    *Private_Key;
char    *CA_Cert;
char    *Passphrase;
#endif
int     Security;

/* Params */
int      network_flag;
int      Route_Weight;
sp_time  Up_Down_Interval;
sp_time  Time_until_Exit;
int      Minimum_Window;
int      Fast_Retransmit;
int      Stream_Fairness;
int      Schedule_Set_Route;
int      Unicast_Only;
int16    KR_Flags;

/* Statistics */
long64 total_received_bytes;
long64 total_received_pkts;
long64 total_udp_pkts;
long64 total_udp_bytes;
long64 total_rel_udp_pkts;
long64 total_rel_udp_bytes;
long64 total_link_ack_pkts;
long64 total_link_ack_bytes;
long64 total_hello_pkts;
long64 total_hello_bytes;
long64 total_link_state_pkts;
long64 total_link_state_bytes;
long64 total_group_state_pkts;
long64 total_group_state_bytes;


/* Static Variables */
static void 	Usage(int argc, char *argv[]);
static void     Init_Memory_Objects(void);
int32           Get_Interface_ip(char *iface);

/***********************************************************/
/* int main(int argc, char* argv[])                        */
/*                                                         */
/* Main function. Here it all begins...                    */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* argc, argv: standard, input parameters                  */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

int main(int argc, char* argv[]) 
{

#ifdef ARCH_PC_WIN95
	int ret;
#endif

    Alarm( PRINT, "/===========================================================================\\\n");
    Alarm( PRINT, "| Spines                                                                    |\n");
    Alarm( PRINT, "| Copyright (c) 2003 - 2007 Johns Hopkins University                        |\n"); 
    Alarm( PRINT, "| All rights reserved.                                                      |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| Spines is licensed under the Spines Open-Source License.                  |\n");
    Alarm( PRINT, "| You may only use this software in compliance with the License.            |\n");
    Alarm( PRINT, "| A copy of the License can be found at http://www.spines.org/LICENSE.txt   |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| Creators:                                                                 |\n");
    Alarm( PRINT, "|    Yair Amir                 yairamir@cs.jhu.edu                          |\n");
    Alarm( PRINT, "|    Claudiu Danilov           claudiu@cs.jhu.edu                           |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| Major Contributors:                                                       |\n");
    Alarm( PRINT, "|    John Lane                 johnlane@cs.jhu.edu                          |\n");
    Alarm( PRINT, "|    Raluca Musaloiu-Elefteri  ralucam@cs.jhu.edu                           |\n");
    Alarm( PRINT, "|    Nilo Rivera               nrivera@cs.jhu.edu                           |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| WWW:     www.spines.org      www.dsn.jhu.edu                              |\n");
    Alarm( PRINT, "| Contact: spines@spines.org                                                |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| Version 3.0, Built May 31, 2007                                           |\n"); 
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| This product uses software developed by Spread Concepts LLC for use       |\n");
    Alarm( PRINT, "| in the Spread toolkit. For more information about Spread,                 |\n");
    Alarm( PRINT, "| see http://www.spread.org                                                 |\n");
    Alarm( PRINT, "\\===========================================================================/\n\n");

    Usage(argc, argv);
    //Alarm_set_types(DEBUG); 

#ifdef SPINES_SSL
    if (Security) {
	// openssl
	ERR_load_crypto_strings();
	ERR_load_EVP_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	OpenSSL_add_all_algorithms();

	// server context
	if (!(ctx_server = SSL_CTX_new(DTLSv1_server_method())))
		Alarm(EXIT, "main: cannot allocate ctx_server\n");

	// client context
	if (!(ctx_client = SSL_CTX_new(DTLSv1_client_method())))
		Alarm(EXIT, "main: cannot allocate ctx_client\n");
   
 	SSL_CTX_set_mode(ctx_client, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_read_ahead(ctx_client, 1);

	if (!SSL_CTX_load_verify_locations(ctx_client, CA_Cert, NULL)) {
		printf("SSL_CTX_load_verify_locations err\n");
		ERR_print_errors_fp(stderr);
		exit(1);
	}
	
	if (!(bio_tmp = BIO_new(BIO_s_mem())))
		Alarm(EXIT, "main: canot allocate bio_tmp");
		BIO_set_mem_eof_return(bio_tmp, -1);
	}
#endif
    
#ifdef	ARCH_PC_WIN95    
    ret = WSAStartup( MAKEWORD(1,1), &WSAData );
    if( ret != 0 )
        Alarm( EXIT, "r: winsock initialization error %d\n", ret );
#endif	/* ARCH_PC_WIN95 */

    E_init();
   
    Init_Memory_Objects();

    Init_Network();

    if(Up_Down_Interval.sec != 0)
	E_queue(Up_Down_Net, 0, NULL, Up_Down_Interval);

    if(Time_until_Exit.sec != 0)
	E_queue(Graceful_Exit, 0, NULL, Time_until_Exit);

    E_handle_events();

    return(1);
}



/***********************************************************/
/* void Init_Memory_Objects(void)                          */
/*                                                         */
/* Initializes memory                                      */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

static void Init_Memory_Objects(void)
{
    /* initilize memory object types  */
    Mem_init_object_abort(PACK_HEAD_OBJ, sizeof(packet_header), 100, 1);
    Mem_init_object_abort(PACK_BODY_OBJ, sizeof(packet_body), 200, 20);
    Mem_init_object_abort(SYS_SCATTER, sizeof(sys_scatter), 100, 1);
    Mem_init_object_abort(TREE_NODE, sizeof(Node), 30, 10);
    Mem_init_object_abort(DIRECT_LINK, sizeof(Link), 10, 1);
    Mem_init_object_abort(OVERLAY_EDGE, sizeof(Edge), 50, 10);
    Mem_init_object_abort(OVERLAY_ROUTE, sizeof(Route), 900, 10);
    Mem_init_object_abort(CHANGED_STATE, sizeof(Changed_State), 50, 1);
    Mem_init_object_abort(STATE_CHAIN, sizeof(State_Chain), 200, 1);
    Mem_init_object_abort(MULTICAST_GROUP, sizeof(Group_State), 200, 1);
    Mem_init_object_abort(BUFFER_CELL, sizeof(Buffer_Cell), 300, 1);
    Mem_init_object_abort(FRAG_PKT, sizeof(Frag_Packet), 300, 1);
    Mem_init_object_abort(UDP_CELL, sizeof(UDP_Cell), 300, 1);
    Mem_init_object_abort(CONTROL_DATA, sizeof(Control_Data), 10, 1);
    Mem_init_object_abort(RELIABLE_DATA, sizeof(Reliable_Data), 30, 1);
    Mem_init_object_abort(REALTIME_DATA, sizeof(Realtime_Data), 10, 0);
    Mem_init_object_abort(SESSION_OBJ, sizeof(Session), 30, 0);
    Mem_init_object_abort(STDHASH_OBJ, sizeof(stdhash), 100, 0);
#ifdef SPINES_SSL
    Mem_init_object(SSL_IP_BUFFER, 15, 10000, 0);
    Mem_init_object(SSL_PKT_BUFFER, MAX_PACKET_SIZE, 10000, 0);
    Mem_init_object(SSL_SOCKADDR_IN, sizeof(struct sockaddr_in), 10000, 0);   
#endif
}


/***********************************************************/
/* int32 Get_Interface_ip(char *iface)                     */
/*                                                         */
/* Get the IP address for device name (i.e. eth0)          */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* *iface: string with the if name                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* ip address for the interface/device name                */
/*                                                         */
/***********************************************************/
int32 Get_Interface_ip(char *iface)
{
    int sk;
    int32 addr;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(struct ifreq));
    if((sk = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
        ifr.ifr_addr.sa_family = AF_INET;
        strcpy(ifr.ifr_name, iface);

        if (ioctl(sk, SIOCGIFADDR, &ifr) == 0) {
            addr = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr;
        } else { 
	    Alarm(PRINT, "Get_Interface_ip() SIOCGIFADDR problem (root?)\n");
	    return -1;
        }
    } else { 
	Alarm(PRINT, "Get_Interface_ip() socket error. (root?)\n");
	return -1;
    }
    close(sk);
    return ntohl(addr);
}



/***********************************************************/
/* void Usage(int argc, char* argv[])                      */
/*                                                         */
/* Parses command line parameters                          */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* argc, argv: standard command line parameters            */
/*                                                         */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* NONE                                                    */
/*                                                         */
/***********************************************************/

static  void    Usage(int argc, char *argv[])
{
    char ip_str[16];
    int i1, i2, i3, i4;
    int cnt = 0;
    int j, tmp, ret;

    /* Setting defaults values */
    Port = 8100;
    Address[0] = -1;
    My_Address = -1;
    Route_Weight = DISTANCE_ROUTE;
    network_flag = 1;
    Minimum_Window = 1;
    Fast_Retransmit = 0;
    Stream_Fairness = 0;
    Up_Down_Interval.sec  = 0;
    Up_Down_Interval.usec = 0;
    Time_until_Exit.sec  = 0;
    Time_until_Exit.usec = 0;
    Accept_Monitor = 0;
    Unicast_Only = 0;
    KR_Flags = 0;

#ifdef SPINES_SSL
    /* openssl */
    Security = 0;
    Wireless = 0;
    Public_Key = NULL;
    Private_Key = NULL;
    Passphrase = NULL;
    CA_Cert = NULL;
#endif
    Num_Discovery_Addresses = 0;

    while(--argc > 0) {
        argv++;
	if(!strncmp(*argv, "-mw", 3)) {
	    sscanf(argv[1], "%d", (int*)&Minimum_Window);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-fr", 3)) {
	    Fast_Retransmit = 1;
	}else if(!strncmp(*argv, "-sf", 3)) {
	    Stream_Fairness = 1;
	}else if(!strncmp(*argv, "-m", 2)) {
	    Accept_Monitor = 1;
	}else if(!strncmp(*argv, "-U", 2)) {
	    Unicast_Only = 1;
	}else if(!strncmp(*argv, "-W", 2)) {
        Wireless = 1;
#ifdef SPINES_SSL
	}else if (!strncmp(*argv, "-secure", 7)) {
	    Security = 1;
	}else if(!strncmp(*argv, "-pub", 4)) {
	    Public_Key = strdup(argv[1]);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-priv", 5)) {
	    Private_Key = strdup(argv[1]);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-pass", 5)) {
	    Passphrase = strdup(argv[1]);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-cacert", 7)) {
	    CA_Cert = strdup(argv[1]);
	    argc--; argv++;
#endif
	}else if(!strncmp(*argv, "-p", 2)) {
	    sscanf(argv[1], "%d", (int*)&tmp);
	    Port = (int16u)tmp;
	    argc--; argv++;
	}else if(!strncmp(*argv, "-u", 2)) {
	    sscanf(argv[1], "%d", (int*)&Up_Down_Interval.sec);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-x", 2)) {
	    sscanf(argv[1], "%d", (int*)&Time_until_Exit.sec);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-l", 2)) {
	    sscanf(argv[1], "%s", ip_str);
	    ret = sscanf( ip_str ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	    if (ret == 4) { 
		My_Address = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
	    } else { 
		My_Address = Get_Interface_ip(ip_str);
	    } 
	    Alarm(PRINT,"My Address = "IPF"\n",IP(My_Address));
	    argc--; argv++;
	}else if(!strncmp(*argv, "-d", 2)) {
	    sscanf(argv[1], "%s", ip_str);
	    sscanf(ip_str ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	    Discovery_Address[Num_Discovery_Addresses++] = 
		( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
	    argc--; argv++;
	    if (Num_Discovery_Addresses > MAX_DISCOVERY_ADDR) {
		Alarm(EXIT, "too many discovery addresses...\n");
	    }
	}else if((!strncmp(*argv, "-w", 2))&&(!strncmp(*(argv+1), "distance", 8))) {
	    Route_Weight = DISTANCE_ROUTE;
	    argc--; argv++;
	}else if((!strncmp(*argv, "-w", 2))&&(!strncmp(*(argv+1), "latency", 7))) {
	    Route_Weight = LATENCY_ROUTE;
	    argc--; argv++;
	}else if((!strncmp(*argv, "-w", 2))&&(!strncmp(*(argv+1), "loss", 4))) {
	    Route_Weight = LOSSRATE_ROUTE;
	    argc--; argv++;
	}else if((!strncmp(*argv, "-w", 2))&&(!strncmp(*(argv+1), "explat", 3))) {
	    Route_Weight = AVERAGE_ROUTE;
	    argc--; argv++;
	}else if((!strncmp(*argv, "-a", 2)) && (argc > 1) && (cnt < 255)) {
	    sscanf(argv[1], "%s", ip_str);
	    ret = sscanf( ip_str ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	    if (ret == 4) { 
		Address[cnt] = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
	    } else { 
		Address[cnt] = Get_Interface_ip(ip_str);
	    }
	    /* Check for duplicates */
	    for(j=0; j<cnt; j++) {
	        if(Address[j] == Address[cnt]) {
		    cnt--;
		    break;
  	        }
	    }
	    Address[cnt+1] = -1; 
	    argc--; argv++; cnt++;
	    if(cnt >= MAX_NEIGHBORS) {
		Alarm(EXIT, "too many neighbors...\n");
	    }
	}else if(!(strncmp(*argv, "-k", 2))) { 
	    sscanf(argv[1], "%d", (int*)&tmp);
	    if (tmp == 0) KR_Flags |= KR_OVERLAY_NODES;
	    if (tmp == 1) KR_Flags |= KR_CLIENT_ACAST_PATH;
	    if (tmp == 2) KR_Flags |= KR_CLIENT_MCAST_PATH;
	    argc--; argv++;
	}else{

		Alarm(PRINT, "ERR: %d | %s\n", argc, *argv);
		
		Alarm(PRINT, "Usage: \n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		  "\t[-p <port number> ] : to send on, default is 8100",
		  "\t[-l <IP address>  ] : local address,",
		  "\t[-a <IP address>  ] : to connect to,",
		  "\t\tat most 255 different directly connected neighbors,",
		  "\t[-d <IP address>  ] : auto-discovery multicast address,",
		  "\t[-w <Route_Weight>] : [distance, latency, loss, explat], default: distance,",
		  "\t[-sf              ] : stream based fairness (for reliable links),",
		  "\t[-m               ] : accept monitor commands for setting loss rates,",
		  "\t[-x <seconds>     ] : time until exit,",
		  "\t[-U               ] : Unicast only: no multicast capabilities,",
		  "\t[-W               ] : Wireless Mode,",
		  "\t[-k <level>       ] : kernel routing on data packets,");
		Alarm(EXIT, "Bye...\n");
	}
    }
    Num_Initial_Nodes = cnt;

    for(cnt=0; Address[cnt] != -1; cnt++) {
        i1 = (Address[cnt] >> 24) & 0xff;
	i2 = (Address[cnt] >> 16) & 0xff;
	i3 = (Address[cnt] >> 8) & 0xff;
	i4 = Address[cnt] & 0xff;
        Alarm(PRINT, "IP: %d.%d.%d.%d\n", i1, i2, i3, i4);
    }

#ifdef SPINES_SSL
    /* openssl */
    if (Security) {
        if (!Public_Key)
            Alarm(EXIT, "Public key required (use -pub <public_key> option)\n");
        if (!Private_Key)
            Alarm(EXIT, "Private key required (use -priv <private_key> option)\n");
        if (!CA_Cert)
            Alarm(EXIT, "CA certificate required (use -cacert <certificate> option)\n");

        Alarm(PRINT, "public key: %s\n", Public_Key);
        Alarm(PRINT, "private key: %s\n", Private_Key);
        Alarm(PRINT, "CA certificate: %s\n", CA_Cert);
    }
#endif
}
