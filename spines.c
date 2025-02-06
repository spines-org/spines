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
#include "network.h"
#include "udp.h"
#include "reliable_udp.h"
#include "session.h"
#include "objects.h"


#ifdef	ARCH_PC_WIN95
#include	<winsock.h>
WSADATA		WSAData;
#endif	/* ARCH_PC_WIN95 */

/* Global Variables */
int16	 Port;
int32    Address[16];
int32    My_Address;
channel  Control_Channel;
int16    Num_Nodes;
stdhash  All_Nodes;
stdhash  All_Edges;
stdhash  Changed_Edges;
Node*    Neighbor_Nodes[MAX_LINKS/MAX_LINKS_4_EDGE];
int16    Num_Neighbors;
Link*    Links[MAX_LINKS];
channel  Local_Send_Channels[MAX_LINKS_4_EDGE];
channel  Local_Recv_Channels[MAX_LINKS_4_EDGE];  
sys_scatter Recv_Pack[MAX_LINKS_4_EDGE];
int      Err_Rate;
int      Reliable_Flag;
int      network_flag;
sp_time  Up_Down_Interval;
sp_time  Time_until_Exit;
stdhash  Sessions_ID;
stdhash  Sessions_Port;
stdhash  Rel_Sessions_Port;
stdhash  Sessions_Sock;
int16    Link_Sessions_Blocked_On; 


/* This shouldn't be here. Just for testing... */
int32    Send_To;


/* Statics */
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

    /* This shouldn't be here. Just for testing... */
    if(Send_To != -1)
	Random_Send(Send_To, NULL);

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
    Mem_init_object_abort(TREE_NODE, sizeof(Node), 30, 10);
    Mem_init_object_abort(DIRECT_LINK, sizeof(Link), 10, 1);
    Mem_init_object_abort(CONTROL_DATA, sizeof(Control_Data), 10, 1);
    Mem_init_object_abort(RELIABLE_DATA, sizeof(Reliable_Data), 30, 1);
    Mem_init_object_abort(UDP_DATA, sizeof(UDP_Data), 10, 0);
    Mem_init_object_abort(OVERLAY_EDGE, sizeof(Edge), 10, 10);
    Mem_init_object_abort(CHANGED_EDGE, sizeof(Changed_Edge), 10, 1);
    Mem_init_object(PACK_HEAD_OBJ, sizeof(packet_header), 10, 1);
    Mem_init_object(PACK_BODY_OBJ, sizeof(packet_body), 200, 10);
    Mem_init_object(SYS_SCATTER, sizeof(sys_scatter), 10, 1);
    Mem_init_object(BUFFER_CELL, sizeof(Buffer_Cell), 210, 1);
    Mem_init_object(UDP_CELL, sizeof(UDP_Cell), 210, 0);
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
    Err_Rate = 0;
    network_flag = 1;
    Reliable_Flag = 0;
    Up_Down_Interval.sec  = 0;
    Up_Down_Interval.usec = 0;
    Time_until_Exit.sec  = 0;
    Time_until_Exit.usec = 0;
    
    Send_To = -1;
    

    while(--argc > 0) {
        argv++;
	if(!strncmp(*argv, "-p", 2)) {
	    sscanf(argv[1], "%d", (int*)&Port);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-u", 2)) {
	    sscanf(argv[1], "%d", (int*)&Up_Down_Interval.sec);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-x", 2)) {
	    sscanf(argv[1], "%d", (int*)&Time_until_Exit.sec);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-r", 2)) {
	    Reliable_Flag = 1;
	}else if(!strncmp(*argv, "-e", 2)) {
	    sscanf(argv[1], "%d", &Err_Rate);
	    if(Err_Rate > 50)
		Alarm(PRINT, "\n\nAre you nuts ? What do you expect for %d percent losses ?\n\n",
		      Err_Rate);
	    argc--; argv++;
	}else if(!strncmp(*argv, "-l", 2)) {
	    sscanf(argv[1], "%s", ip_str);
	    sscanf( ip_str ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	    My_Address = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
	    argc--; argv++;
	}else if(!strncmp(*argv, "-s", 2)) {
	    /* This shouldn't be here. Just for testing... */
	    sscanf(argv[1], "%s", ip_str);
	    sscanf( ip_str ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
	    Send_To = ( (i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4 );
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
	}else{
	    Alarm(PRINT, "Usage: \n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		  "\t[-p <port number>] : to send on, default is 8100",
		  "\t[-e <error rate> ] : error rate receiving at this node,",
		  "\t[-l <IP address> ] : local address,",
		  "\t[-a <IP address> ] : to connect to,",
		  "\t\tthere can be at most 15 different IP addresses,",
		  "\t[-x <seconds>    ] : time until exit,",
		  "\t[-r              ] : use reliable links,",
		  "\t[-u <seconds>    ] : up-down interval");
	    Alarm(EXIT, "Bye...\n");
	}
    }
    Num_Nodes = cnt;

    Alarm(PRINT, "Port: %d\n", Port);
    for(cnt=0; Address[cnt] != -1; cnt++) {
        i1 = (Address[cnt] >> 24) & 0xff;
	i2 = (Address[cnt] >> 16) & 0xff;
	i3 = (Address[cnt] >> 8) & 0xff;
	i4 = Address[cnt] & 0xff;
        Alarm(PRINT, "IP: %d.%d.%d.%d\n", i1, i2, i3, i4);
    }
}
