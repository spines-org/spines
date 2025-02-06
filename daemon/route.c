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
 * Copyright (c) 2003 - 2009 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARCH_PC_WIN95
#include <winsock2.h>
#endif

#include "util/arch.h"
#include "util/alarm.h"
#include "util/memory.h"
#include "util/sp_events.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "node.h"
#include "link.h"
#include "route.h"
#include "dijkstra.h"
#include "objects.h"
#include "state_flood.h"
#include "link_state.h"
#include "kernel_routing.h"
#include "multicast.h"

/* Global variables */

extern stdhash  All_Nodes;
extern stdhash  All_Edges;
extern int32    My_Address;
extern int      Route_Weight;
extern int      Schedule_Set_Route;
extern int      Unicast_Only;
extern Route*   All_Routes;
extern int16    Num_Nodes;
extern int16    KR_Flags;


/* Local variables */
static Node *head;
static int route_time;

/***********************************************************/
/* void Init_Routes(void)                                  */
/*                                                         */
/* Initializes the data structures for Floyd Warshall algo */
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


void Init_Routes(void) 
{
    stdit src_it, it, nd_it;
    State_Chain *s_chain;
    Node *nd, *nd_tmp;
    Edge *edge;
    int cnt, i, j, offset;


    /* Clearing the old routes */
    stdhash_begin(&All_Nodes, &nd_it); 
    /* There should be at least one node, me */
    if(stdhash_is_end(&All_Nodes, &nd_it)) {
	Alarm(EXIT, "Init_Routes: No nodes available\n");
    }
    nd = *((Node **)stdhash_it_val(&nd_it));
    head = nd;
    head->prev = NULL;
    head->next = NULL;
    stdhash_it_next(&nd_it);
    /* Ok, done wih the head of the list. Now see the other nodes */
    while(!stdhash_is_end(&All_Nodes, &nd_it)) {
	nd = *((Node **)stdhash_it_val(&nd_it));
	/* Do an insert sort... */
	if(head->address > nd->address) {
	    nd->prev = NULL;
	    nd->next = head;
	    head->prev = nd;
	    head = nd;
	}
	else {
	    nd_tmp = head;
	    while(nd_tmp->next != NULL) {
		if(nd_tmp->next->address > nd->address) {
		    break;
		}
		nd_tmp = nd_tmp->next;
	    }
	    nd->next = nd_tmp->next;
	    nd->prev = nd_tmp;
	    nd_tmp->next = nd;
	    if(nd->next != NULL) {
		nd->next->prev = nd;
	    }
	}
	/* Ok, go to the next node */
	stdhash_it_next(&nd_it);
    }
    
    /* Index all the nodes in order */
    cnt = 0;
    nd_tmp = head;
    while(nd_tmp != NULL) {
	nd_tmp->node_no = cnt;
	cnt++;
	nd_tmp = nd_tmp->next;
    }

    /* Allocate space for route matrix if its size changed */
    if(Num_Nodes != cnt) {
	Num_Nodes = cnt;
	if(All_Routes != NULL) {
	    free(All_Routes);
	}
	All_Routes = malloc(sizeof(Route)*Num_Nodes*Num_Nodes); 
	if(All_Routes == NULL) {
	    Alarm(EXIT, "Cannot allocate space for the route matrix\n");
	}
    }

    /* Initialize route matrix */
    for(i=0; i<Num_Nodes; i++) {
	for(j=0; j<Num_Nodes; j++) {
	    All_Routes[i*Num_Nodes+j].distance = -1;
	    All_Routes[i*Num_Nodes+j].cost = -1;
	    All_Routes[i*Num_Nodes+j].forwarder = NULL;
	    All_Routes[i*Num_Nodes+j].predecessor = 0;
	}
    }


    /* Initializing the routes with the direct neighbors */
    stdhash_begin(&All_Edges, &src_it); 
    while(!stdhash_is_end(&All_Edges, &src_it)) {
        /* source by source */
        s_chain = *((State_Chain **)stdhash_it_val(&src_it));
	stdhash_find(&All_Nodes, &nd_it, &s_chain->address);
	if(stdhash_is_end(&All_Nodes, &nd_it)) {
	    Alarm(EXIT, "Init_Routes: edge without source\n");
	}
	nd = *((Node **)stdhash_it_val(&nd_it));

        stdhash_begin(&s_chain->states, &it); 
        while(!stdhash_is_end(&s_chain->states, &it)) {
            /* one by one, the destinations... */
            edge = *((Edge **)stdhash_it_val(&it));
	    if(edge->cost >= 0) {
		offset = Num_Nodes*edge->source->node_no + edge->dest->node_no;
		All_Routes[offset].distance = 1;
		All_Routes[offset].cost = edge->cost;
		if(edge->source_addr == My_Address) {
		    All_Routes[offset].forwarder = edge->dest;
		}
		else {
		    All_Routes[offset].forwarder = NULL;
		}
		All_Routes[offset].predecessor = edge->source_addr;
	    }
	    stdhash_it_next(&it);
	}
	stdhash_it_next(&src_it);
    }
}



/***********************************************************/
/* Route* Find_Route(int32 source, int32 dest)             */
/*                                                         */
/* Finds a route (if it exists)                            */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* source: IP address of the source                        */
/* dest:   IP address of the destination                   */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Route*) a pointer to the Route structure, or NULL      */
/*          if the route does not exist                    */
/*                                                         */
/***********************************************************/

Route* Find_Route(int32 source, int32 dest) 
{
    stdit it;
    Node *src, *dst;
    Route *route;

    stdhash_find(&All_Nodes, &it, &source);
    if(stdhash_is_end(&All_Nodes, &it)) {
	return(NULL);
    }
    src = *((Node **)stdhash_it_val(&it));
    stdhash_find(&All_Nodes, &it, &dest);
    if(stdhash_is_end(&All_Nodes, &it)) {
	return(NULL);
    }
    dst = *((Node **)stdhash_it_val(&it));

    route = &All_Routes[Num_Nodes*src->node_no+dst->node_no];

    return(route);
}




/***********************************************************/
/* void Set_Routes(int dummy_int, void *dummy)             */
/*                                                         */
/* Runs Floyd-Warshall. Computes all-pairs shortest paths. */
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

void Set_Routes(int dummy_int, void *dummy) 
{
    Node *k_nd, *i_nd, *j_nd;
    Route *i_k_route, *k_j_route, *i_j_route;
    int flag;
    sp_time start, stop;


    start = E_get_time();
    Schedule_Set_Route = 0;
    
    /* Run Dijkstra if we only need unicast */
    if(Unicast_Only == 1) {
	Dj_Set_Routes(dummy_int, dummy);
    }
    else {
	Init_Routes();
	k_nd = head;
	while(k_nd != NULL) {
	    /* For all k */
	    i_nd = head;
	    while(i_nd != NULL) {
		/* For all i */
		i_k_route = &All_Routes[i_nd->node_no*Num_Nodes+k_nd->node_no];
		if(i_k_route->distance == -1) {
		    i_nd = i_nd->next;
		    continue; 
		}
	
		j_nd = head;
		while(j_nd != NULL) {
		    /* For all j */
		    if(i_nd->address == j_nd->address) {
			/* i == j */
			j_nd = j_nd->next;
			continue;
		    }


		    k_j_route = &All_Routes[k_nd->node_no*Num_Nodes+j_nd->node_no];
		    if(k_j_route->distance == -1) {
			j_nd = j_nd->next;
			continue;
		    }
		    /*
		     *Alarm(DEBUG, "k: %d; i: %d, j: %d\n",
		     *	  IP4(k_nd->address), IP4(i_nd->address), IP4(j_nd->address));
		     */ 

		    i_j_route = &All_Routes[i_nd->node_no*Num_Nodes+j_nd->node_no];
		    
		    flag = 0;
		    if(Route_Weight == DISTANCE_ROUTE) {
			/* Distance-based routing.*/
			if((i_j_route->distance == -1)||
				// ralucam: HACK!
			   // initial line (i_j_route->distance > i_k_route->distance + k_j_route->distance)) {
			   (i_j_route->cost > i_k_route->cost + k_j_route->cost)) {
			    flag = 1;
			}
		    }
		    else {
			/* Cost-based routing. */
			if((i_j_route->cost == -1)||
			   (i_j_route->cost > i_k_route->cost + k_j_route->cost)) {
			    flag = 1;
			}
		    }
		    
		    if(flag == 1) {
			/* Ok, the route through k is better */
			i_j_route->distance = i_k_route->distance + k_j_route->distance;
			i_j_route->cost = i_k_route->cost + k_j_route->cost;
			i_j_route->predecessor = k_j_route->predecessor;
			/* I am either on the path from i to k, or on the 
			 * path from k to j, or on neither of them,
			 * but not on both... */
			i_j_route->forwarder = i_k_route->forwarder;
			if(k_j_route->forwarder != NULL) {
			    i_j_route->forwarder = k_j_route->forwarder;
			}

		    }
		    j_nd = j_nd->next;	
		}
		i_nd = i_nd->next;	
	    }
	    k_nd = k_nd->next;	
	}
	
	/* Remove Group_State information that is related to any newly
	 * unreachable node. */
 	
	/* Get the computational time for setting the routes */
    }
    stop = E_get_time();
    route_time = (stop.sec - start.sec)*1000000;
    route_time += stop.usec - start.usec;

    /* Topology change; discard the neighbors arrays */
    Discard_All_Mcast_Neighbors();

#ifndef ARCH_PC_WIN95
    /* Kernel Routing:  Apply updates and change default route */
    if (KR_Flags != 0) {
        KR_Update_All_Routes();
    }
#endif
}


/***********************************************************/
/* Node* Get_Route(int32 source, int32 dest)               */
/*                                                         */
/* Gets the next node to forward the packet for a certain  */
/* destination                                             */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* source : IP address of the source                       */
/* dest : IP address of the destination                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Node*) pointer to the next node in the path            */
/*                                                         */
/***********************************************************/

Node* Get_Route(int32 source, int32 dest)
{
    Route *route;

    /* Get the route from Dijkstra if we only need unicast */
    if(Unicast_Only == 1) {
	return Dj_Get_Route(dest);
    }

    if((route = Find_Route(source, dest)) == NULL) {
	return(NULL);
    }
    return(route->forwarder);
}

void Trace_Route(int32 source, int32 dest, spines_trace *spines_tr)
{
    int i=0;
    Route *route;

    while ( (route = Find_Route(source, dest)) != NULL) {
        spines_tr->address[i] = source;
        spines_tr->distance[i] = route->distance;
        spines_tr->cost[i] = route->cost;
        if (route->forwarder != NULL) {
            source = route->forwarder->address;
        } else {
            /* TODO: Not completely right, but ok for now */
            source = dest;
        }
        if (++i == MAX_COUNT || source == dest) {
            break;
        }
    }
    spines_tr->count = i;
}



/* For debugging only */

void Print_Routes(FILE *fp) 
{
    stdit it;
    Node *nd;
    Route *route;
    char line[256];

    sprintf(line, "\n\nComputing routing time: %d\n", route_time);
    Alarm(PRINT, "%s", line);
    if (fp != NULL) fprintf(fp, "%s", line);

    /* Print the routes from Dijkstra if we only need unicast */
    if(Unicast_Only == 1) {
	Dj_Print_Routes(fp);
	return;
    }

    sprintf(line, "ROUTES:\n");
    Alarm(PRINT, "%s", line);
    if (fp != NULL) fprintf(fp, "%s", line);

    /* Print the local node first */
    sprintf(line, IPF " \tLOCAL NODE ", IP(My_Address));
    Alarm(PRINT, "%s\n", line);
    if (fp != NULL) fprintf(fp, "%s\n", line);

    stdhash_begin(&All_Nodes, &it); 
    while(!stdhash_is_end(&All_Nodes, &it)) {
        nd = *((Node **)stdhash_it_val(&it));
	
	if(nd->address == My_Address) {
	    stdhash_it_next(&it);
	    continue;
	}
	
	route = Find_Route(My_Address, nd->address);
	if(route != NULL) {
	    if(route->forwarder != NULL) {
		sprintf(line, "%d.%d.%d.%d via: %d.%d.%d.%d dist: %d; cost: %d ", 
		      IP1(nd->address), IP2(nd->address), 
		      IP3(nd->address), IP4(nd->address), 
		      IP1(route->forwarder->address), IP2(route->forwarder->address), 
		      IP3(route->forwarder->address), IP4(route->forwarder->address),
		      route->distance, route->cost);
                Alarm(PRINT, "%s\n", line);
                if (fp != NULL) fprintf(fp, "%s\n", line);
        	stdhash_it_next(&it);
		continue;
	    }	    
	}
	sprintf(line, "%d.%d.%d.%d \t!!! NO ROUTE ", 
	      IP1(nd->address), IP2(nd->address), 
	      IP3(nd->address), IP4(nd->address)); 
        Alarm(PRINT, "%s\n", line);
        if (fp != NULL) fprintf(fp, "%s\n", line);
        stdhash_it_next(&it);
    }
    sprintf(line, "\n\n");
    Alarm(PRINT, "%s", line);
    if (fp != NULL) fprintf(fp, "%s", line);
}

