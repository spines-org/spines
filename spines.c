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


#include <string.h>

#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "util/memory.h"
#include "util/data_link.h"
#include "stdutil/stdhash.h"
#include "stdutil/stdcarr.h"

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


#ifdef	ARCH_PC_WIN95
#include	<winsock.h>
WSADATA		WSAData;
#endif	/* ARCH_PC_WIN95 */


/* Global Variables */

/* Startup */
int16	 Port;
int32    Address[256];
int32    My_Address;

/* Nodes and direct links */
int16    Num_Initial_Nodes;
Node*    Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
int16    Num_Neighbors;
stdhash  All_Nodes;
Link*    Links[MAX_LINKS];
channel  Local_Send_Channels[MAX_LINKS_4_EDGE];
channel  Local_Recv_Channels[MAX_LINKS_4_EDGE];  
sys_scatter Recv_Pack[MAX_LINKS_4_EDGE];

stdhash  Monitor_Loss;
int      Accept_Monitor;

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


/* Params */
int      network_flag;
int      Route_Weight;
sp_time  Up_Down_Interval;
sp_time  Time_until_Exit;
int      Minimum_Window;
int      Fast_Retransmit;
int      Stream_Fairness;
int      Padding;


/* Statistics */
long long int total_received_bytes;
long long int total_received_pkts;
long long int total_udp_pkts;
long long int total_udp_bytes;
long long int total_rel_udp_pkts;
long long int total_rel_udp_bytes;
long long int total_link_ack_pkts;
long long int total_link_ack_bytes;
long long int total_hello_pkts;
long long int total_hello_bytes;
long long int total_link_state_pkts;
long long int total_link_state_bytes;
long long int total_group_state_pkts;
long long int total_group_state_bytes;


/* Static Variables */
static void 	Usage(int argc, char *argv[]);
static void     Init_Memory_Objects(void);



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
    Alarm( PRINT, "| Copyright (c) 2003 Johns Hopkins University                               |\n"); 
    Alarm( PRINT, "| All rights reserved.                                                      |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| Spines is licensed under the Spines Open-Source License.                  |\n");
    Alarm( PRINT, "| You may only use this software in compliance with the License.            |\n");
    Alarm( PRINT, "| A copy of the License can be found at http://www.spines.org/LICENSE.txt   |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| Creators:                                                                 |\n");
    Alarm( PRINT, "|    Yair Amir             yairamir@cs.jhu.edu                              |\n");
    Alarm( PRINT, "|    Claudiu Danilov       claudiu@cs.jhu.edu                               |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| WWW:     www.spines.org     www.cnds.jhu.edu                              |\n");
    Alarm( PRINT, "| Contact: spines@spines.org                                                |\n");
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| Version 1.00.00, Built Jan 22, 2003                                       |\n"); 
    Alarm( PRINT, "|                                                                           |\n");
    Alarm( PRINT, "| This product uses software developed by Spread Concepts LLC for use       |\n");
    Alarm( PRINT, "| in the Spread toolkit. For more information about Spread,                 |\n");
    Alarm( PRINT, "| see http://www.spread.org                                                 |\n");
    Alarm( PRINT, "\\===========================================================================/\n\n");


    Usage(argc, argv);
    Alarm_set(NONE);

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
    /* initilize memory object types */
    Mem_init_object(PACK_HEAD_OBJ, sizeof(packet_header), 50, 1);
    Mem_init_object(PACK_BODY_OBJ, sizeof(packet_body), 200, 10);
    Mem_init_object(SYS_SCATTER, sizeof(sys_scatter), 10, 1);
    Mem_init_object_abort(TREE_NODE, sizeof(Node), 30, 10);
    Mem_init_object_abort(DIRECT_LINK, sizeof(Link), 10, 1);
    Mem_init_object_abort(OVERLAY_EDGE, sizeof(Edge), 50, 10);
    Mem_init_object_abort(OVERLAY_ROUTE, sizeof(Route), 900, 10);
    Mem_init_object_abort(CHANGED_STATE, sizeof(Changed_State), 50, 1);
    Mem_init_object_abort(STATE_CHAIN, sizeof(State_Chain), 50, 1);
    Mem_init_object_abort(MULTICAST_GROUP, sizeof(Group_State), 200, 1);
    Mem_init_object(BUFFER_CELL, sizeof(Buffer_Cell), 300, 1);
    Mem_init_object(UDP_CELL, sizeof(UDP_Cell), 300, 0);
    Mem_init_object_abort(CONTROL_DATA, sizeof(Control_Data), 10, 1);
    Mem_init_object_abort(RELIABLE_DATA, sizeof(Reliable_Data), 30, 1);
    Mem_init_object(SESSION_OBJ, sizeof(Session), 30, 0);
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
    int j;

    /* Setting defaults */
    Port = 8100;
    Address[0] = -1;
    My_Address = -1;
    Route_Weight = DISTANCE_ROUTE;
    network_flag = 1;
    Minimum_Window = 1;
    Fast_Retransmit = 0;
    Stream_Fairness = 0;
    Padding = 0;
    Up_Down_Interval.sec  = 0;
    Up_Down_Interval.usec = 0;
    Time_until_Exit.sec  = 0;
    Time_until_Exit.usec = 0;
    Accept_Monitor = 0;


    while(--argc > 0) {
        argv++;
	if(!strncmp(*argv, "-sf", 3)) {
	    Stream_Fairness = 1;
	}else if(!strncmp(*argv, "-m", 2)) {
	    Accept_Monitor = 1;
	}else if(!strncmp(*argv, "-p", 2)) {
	    sscanf(argv[1], "%d", (int*)&Port);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-u", 2)) {
	    sscanf(argv[1], "%d", (int*)&Up_Down_Interval.sec);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-x", 2)) {
	    sscanf(argv[1], "%d", (int*)&Time_until_Exit.sec);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-l", 2)) {
	    sscanf(argv[1], "%s", ip_str);
	    sscanf( ip_str ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	    My_Address = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
	    argc--; argv++;
	}else if((!strncmp(*argv, "-w", 2))&&(!strcmp(*(argv+1), "distance"))) {
	    Route_Weight = DISTANCE_ROUTE;
	    argc--; argv++;
	}else if((!strncmp(*argv, "-w", 2))&&(!strcmp(*(argv+1), "latency"))) {
	    Route_Weight = LATENCY_ROUTE;
	    argc--; argv++;
	}else if((!strncmp(*argv, "-w", 2))&&(!strcmp(*(argv+1), "loss"))) {
	    Route_Weight = LOSSRATE_ROUTE;
	    argc--; argv++;
	}else if((!strncmp(*argv, "-a", 2)) && (argc > 1) && 
		 (strlen(argv[1]) < sizeof(ip_str)) && (cnt < 16)) {
	    sscanf(argv[1], "%s", ip_str);
	    sscanf( ip_str ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	    Address[cnt] = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
	    /* Check for duplicates */
	    for(j=0; j<cnt; j++)
	        if(Address[j] == Address[cnt]) {
		    cnt--;
		    break;
  	        }
	    Address[cnt+1] = -1; 
	    argc--; argv++; cnt++;
	    if(cnt >= 256) {
		Alarm(EXIT, "too many neighbors...\n");
	    }
	}else{
	    Alarm(PRINT, "Usage: \n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		  "\t[-p <port number> ] : to send on, default is 8100",
		  "\t[-l <IP address>  ] : local address,",
		  "\t[-a <IP address>  ] : to connect to,",
		  "\t\tat most 255 different directly connected neighbors,",
		  "\t[-w <Route_Weight>] : [distance, latency, loss], default: distance,",
		  "\t[-sf              ] : stream based fairness (for reliable links),",
		  "\t[-m               ] : accept monitor commands for setting loss rates,",
		  "\t[-x <seconds>     ] : time until exit,",
		  "\t[-u <seconds>     ] : up-down interval");
	    Alarm(EXIT, "Bye...\n");
	}
    }
    Num_Initial_Nodes = cnt;

    Alarm(PRINT, "Port: %d\n", Port);
    for(cnt=0; Address[cnt] != -1; cnt++) {
        i1 = (Address[cnt] >> 24) & 0xff;
	i2 = (Address[cnt] >> 16) & 0xff;
	i3 = (Address[cnt] >> 8) & 0xff;
	i4 = Address[cnt] & 0xff;
        Alarm(PRINT, "IP: %d.%d.%d.%d\n", i1, i2, i3, i4);
    }
}
