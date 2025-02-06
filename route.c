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
#include "util/sp_events.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "node.h"
#include "link.h"
#include "route.h"


/* Global variables */

extern stdhash  All_Nodes;
extern stdhash  All_Edges;
extern int32    My_Address;

/* Local variables */

static Node* head;



/***********************************************************/
/* void Build_Q_Set(void)                                  */
/*                                                         */
/* Builds the Q set (see Dijkstra). Actually, it organizes */
/* nodes in a linked list                                  */
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

void Build_Q_Set(void) 
{
    stdhash_it it;
    Node *nd, *old;
    
    /* There should be at least one node here, myself */
    stdhash_begin(&All_Nodes, &it); 
    nd = *((Node **)stdhash_it_val(&it));
    head = nd;
    head->prev = NULL;
    head->next = NULL;
    if(head->address == My_Address) {
	head->cost = 0;
	head->distance = 0;
	head->forwarder = head;
    }
    else {
	head->cost = -1;
	head->distance = -1;
	head->forwarder = NULL;
    }
    old = nd;
    stdhash_it_next(&it);
    while(!stdhash_it_is_end(&it)) {
        nd = *((Node **)stdhash_it_val(&it));
	if(nd->address == My_Address) {
	    head->prev = nd;
	    nd->next = head;
	    nd->prev = NULL;
	    head = nd;
	    head->cost = 0;
	    head->distance = 0;
	    head->forwarder = head;
	}
	else {
	    old->next = nd;
	    nd->prev = old;
	    nd->next = NULL;
	    nd->cost = -1;
	    nd->distance = -1;
	    nd->forwarder = NULL;
	    old = nd;
	}
	stdhash_it_next(&it);
    }
}



/***********************************************************/
/* void Set_Routes(void)                                   */
/*                                                         */
/* Runs Dijkstra. Computes the shortest paths.             */
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
    stdhash_it it;
    Node *nd, *dest, *tmp;
    Edge *edge;

    Build_Q_Set();
	

    Alarm(DEBUG, "\n\n\n\n");



    while(head != NULL) {
	if(head->distance == -1)
	   break;

	/* Take the head and move it to the S set */
	nd = head;
	head = nd->next;
	nd->next = NULL;
	if(head != NULL)
	    head->prev = NULL;

	Alarm(DEBUG, "head is: %d.%d.%d.%d\n", 
	      IP1(nd->address), IP2(nd->address), 
	      IP3(nd->address), IP4(nd->address)); 
 
	if(head == NULL)
	    Alarm(DEBUG, "And the next one is NULL\n");
      

	/* Take all the links of this node and relax them */
	 stdhash_find(&All_Edges, &it, &nd->address);
	 while(!stdhash_it_is_end(&it)) {
	     edge = *((Edge **)stdhash_it_val(&it));

	     if(edge->cost < 0) {
		 stdhash_it_keyed_next(&it);
		 continue;
	     }

	     dest = edge->dest;
	     if((dest->distance == -1) || 
		(dest->distance > nd->distance + 1)) {
		 /* Ok, this link should be relaxed */
		 dest->distance = nd->distance + 1;
		 dest->cost = nd->cost + edge->cost;
		 if(nd->address ==  My_Address) {
		     dest->forwarder = dest;
		 }
		 else {
		     dest->forwarder = nd->forwarder;
		 }

		 /* Now put this node in the right spot in the sorted Q set */
		 if(dest->prev != NULL) {
		     /* Take dest from its place */
		     dest->prev->next = dest->next;
		     if(dest->next != NULL) {
			 dest->next->prev = dest->prev;
		     }
		     /* And do an insert sort... */
		     if((head->distance == -1)||
			(head->distance > dest->distance)) {
			 dest->prev = NULL;
			 dest->next = head;
			 head->prev = dest;
			 head = dest;
		     }
		     else {
			 tmp = head;
			 while(tmp->next != NULL) {
			     if((tmp->next->distance > dest->distance)||
				(tmp->next->distance == -1))
				 break;
			     tmp = tmp->next;
			 }
			 dest->next = tmp->next;
			 dest->prev = tmp;
			 tmp->next = dest;
			 if(dest->next != NULL) {
			     dest->next->prev = dest;
			 }
		     }
		 }
		 else if(dest != head)
		     Alarm(EXIT, "Routing ERROR !!!\n");
	     }
	     stdhash_it_keyed_next(&it);
	 }
    }

    Alarm(DEBUG, "\n\n\n\n");


}


/***********************************************************/
/* Node* Get_Route(int32 dest)                             */
/*                                                         */
/* Gets the next node to forward the packet for a certain  */
/* destination                                             */
/*                                                         */
/*                                                         */
/* Arguments                                               */
/*                                                         */
/* dest : IP address of the destination                    */
/*                                                         */
/* Return Value                                            */
/*                                                         */
/* (Node*) pointer to the next node in the path            */
/*                                                         */
/***********************************************************/

Node* Get_Route(int32 dest)
{
    stdhash_it it;
    Node *nd;
    
    stdhash_find(&All_Nodes, &it, &dest);
    if(stdhash_it_is_end(&it))
	return NULL;
    nd = *((Node **)stdhash_it_val(&it));
    return nd->forwarder;
}


/* For debugging only */

void Print_Routes(void) 
{
    stdhash_it it;
    Node *nd;

    Alarm(PRINT, "\n\nROUTES:\n");
    stdhash_begin(&All_Nodes, &it); 
    while(!stdhash_it_is_end(&it)) {
        nd = *((Node **)stdhash_it_val(&it));
	if(nd->forwarder != NULL) {
	    Alarm(PRINT, "%d.%d.%d.%d via: %d.%d.%d.%d dist: %d; cost: %d\n", 
		  IP1(nd->address), IP2(nd->address), 
		  IP3(nd->address), IP4(nd->address), 
		  IP1(nd->forwarder->address), IP2(nd->forwarder->address), 
		  IP3(nd->forwarder->address), IP4(nd->forwarder->address),
		  nd->distance, nd->cost);
	}
	else {
	    Alarm(PRINT, "%d.%d.%d.%d  !!! NO ROUTE \n", 
		  IP1(nd->address), IP2(nd->address), 
		  IP3(nd->address), IP4(nd->address)); 
	}
	stdhash_it_next(&it);
    }
    Alarm(PRINT, "\n\n");
}


