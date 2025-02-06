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
#include "util/alarm.h"
#include "util/memory.h"
#include "util/sp_events.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "node.h"
#include "link.h"
#include "route.h"
#include "objects.h"
#include "state_flood.h"
#include "link_state.h"


/* Global variables */

extern stdhash  All_Nodes;
extern stdhash  All_Edges;
extern int32    My_Address;
extern int      Route_Weight;

/* Local variables */
static Node *head;


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
    stdhash_it src_it, it, nd_it, route_it;
    State_Chain *s_chain;
    Node *nd, *nd_tmp;
    Edge *edge;
    Route* route;


    /* Clearing the old routes */
    stdhash_begin(&All_Nodes, &nd_it); 
    /* There should be at least one node, me */
    if(stdhash_it_is_end(&nd_it)) {
	Alarm(EXIT, "Init_Routes: No nodes available\n");
    }
    nd = *((Node **)stdhash_it_val(&nd_it));
    stdhash_begin(&nd->routes, &route_it);
    while(!stdhash_it_is_end(&route_it)) {
	route = *((Route **)stdhash_it_val(&route_it));	    
	dispose(route);
	stdhash_it_next(&route_it);
    }
    stdhash_clear(&nd->routes);
    head = nd;
    head->prev = NULL;
    head->next = NULL;
    stdhash_it_next(&nd_it);

    /* Ok, done wih the head of the list. Now see the other nodes */
    while(!stdhash_it_is_end(&nd_it)) {
	nd = *((Node **)stdhash_it_val(&nd_it));
	stdhash_begin(&nd->routes, &route_it);
	while(!stdhash_it_is_end(&route_it)) {
	    route = *((Route **)stdhash_it_val(&route_it));	    
	    dispose(route);
	    stdhash_it_next(&route_it);
	}
	stdhash_clear(&nd->routes);
	
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
    

    /* Initializing the routes with the direct neighbors */
    stdhash_begin(&All_Edges, &src_it); 
    while(!stdhash_it_is_end(&src_it)) {
        /* source by source */
        s_chain = *((State_Chain **)stdhash_it_val(&src_it));
	stdhash_find(&All_Nodes, &nd_it, &s_chain->address);
	if(stdhash_it_is_end(&nd_it)) {
	    Alarm(EXIT, "Init_Routes: edge without source\n");
	}
	nd = *((Node **)stdhash_it_val(&nd_it));

        stdhash_begin(&s_chain->states, &it); 
        while(!stdhash_it_is_end(&it)) {
            /* one by one, the destinations... */
            edge = *((Edge **)stdhash_it_val(&it));
	    if(edge->cost >= 0) {
		if((route = (Route*) new(OVERLAY_ROUTE))==NULL) {
		    Alarm(EXIT, "Init_Routes: cannot allocate object\n");
		}
		route->source = edge->source_addr;
		route->dest = edge->dest_addr;
		route->distance = 1;
		route->cost = edge->cost;
		if(edge->source_addr == My_Address) {
		    route->forwarder = edge->dest;
		}
		else {
		    route->forwarder = NULL;
		}
		stdhash_insert(&nd->routes, &route_it, &edge->dest_addr, &route);
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
    stdhash_it it, src_it;
    Node *nd;
    Route *route;

    stdhash_find(&All_Nodes, &src_it, &source);
    if(!stdhash_it_is_end(&src_it)) {
	nd = *((Node **)stdhash_it_val(&src_it));
	stdhash_find(&nd->routes, &it, &dest);
	if(!stdhash_it_is_end(&it)) {
	    route = *((Route **)stdhash_it_val(&it));
	    return(route);
	}
    }
    return(NULL);
}




/***********************************************************/
/* void Set_Routes(void)                                   */
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

void Set_Routes(void) {
    stdhash_it route_it;
    Node *k_nd, *i_nd, *j_nd;
    Route *i_k_route, *k_j_route, *i_j_route;
    int flag;

    
    Init_Routes();

    k_nd = head;
    while(k_nd != NULL) {
	/* For all k */
	i_nd = head;
	while(i_nd != NULL) {
	    /* For all i */
	    if((i_k_route = Find_Route(i_nd->address, k_nd->address)) == NULL) {
		/* If there's no route from i to k, continue */
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
		if((k_j_route = Find_Route(k_nd->address, j_nd->address)) == NULL) {
		    /* If there's no route from k to j, continue */
		    j_nd = j_nd->next;
		    continue;
		}
		
		Alarm(DEBUG, "k: %d; i: %d, j: %d\n",
		      IP4(k_nd->address), IP4(i_nd->address), IP4(j_nd->address));

		if((i_j_route = Find_Route(i_nd->address, j_nd->address)) == NULL) {
		    if((i_j_route = (Route*) new(OVERLAY_ROUTE))==NULL) {
			Alarm(EXIT, "Set_Routes: cannot allocate object\n");
		    }
		    i_j_route->source = i_nd->address;
		    i_j_route->dest = j_nd->address;
		    i_j_route->distance = -1;
		    i_j_route->cost = -1;
		    stdhash_insert(&i_nd->routes, &route_it, &j_nd->address, &i_j_route);
		}

		flag = 0;
		if(Route_Weight == DISTANCE_ROUTE) {
		    /* Distance-based routing.*/
		    if((i_j_route->distance == -1)||
		       (i_j_route->distance > i_k_route->distance + k_j_route->distance)) {
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

    if((route = Find_Route(source, dest)) == NULL) {
	return(NULL);
    }
    return(route->forwarder);
}




/* For debugging only */

void Print_Routes(void) 
{
    stdhash_it it;
    Node *nd;
    Route *route;

    Alarm(PRINT, "\n\nROUTES:\n");
    stdhash_begin(&All_Nodes, &it); 
    while(!stdhash_it_is_end(&it)) {
        nd = *((Node **)stdhash_it_val(&it));
	
	if(nd->address == My_Address) {
	    Alarm(PRINT, "%d.%d.%d.%d \tLOCAL NODE \n", 
		  IP1(nd->address), IP2(nd->address), 
		  IP3(nd->address), IP4(nd->address)); 
	    
	    stdhash_it_next(&it);
	    continue;
	}
	
	route = Find_Route(My_Address, nd->address);
	if(route != NULL) {
	    if(route->forwarder != NULL) {
		Alarm(PRINT, "%d.%d.%d.%d via: %d.%d.%d.%d dist: %d; cost: %d\n", 
		      IP1(nd->address), IP2(nd->address), 
		      IP3(nd->address), IP4(nd->address), 
		      IP1(route->forwarder->address), IP2(route->forwarder->address), 
		      IP3(route->forwarder->address), IP4(route->forwarder->address),
		      route->distance, route->cost);
		stdhash_it_next(&it);
		continue;
	    }	    
	}
	Alarm(PRINT, "%d.%d.%d.%d \t!!! NO ROUTE \n", 
	      IP1(nd->address), IP2(nd->address), 
	      IP3(nd->address), IP4(nd->address)); 

	stdhash_it_next(&it);
    }
    Alarm(PRINT, "\n\n");
}
