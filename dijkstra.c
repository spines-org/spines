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

#include <stdio.h>

#include "util/arch.h"
#include "util/alarm.h"
#include "util/sp_events.h"
#include "stdutil/src/stdutil/stdhash.h"

#include "node.h"
#include "link.h"
#include "link_state.h"
#include "state_flood.h"
#include "route.h"

/* Global variables */

extern stdhash  All_Nodes;
extern stdhash  All_Edges;
extern int32    My_Address;

/* Local variables */

static Node* head;



/***********************************************************/
/* void Dj_Build_Q_Set(void)                               */
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

void Dj_Build_Q_Set(void) 
{
    stdit it;
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
    while(!stdhash_is_end(&All_Nodes, &it)) {
        nd = *((Node **)stdhash_it_val(&it));
	if(nd->address == My_Address) {
	    /* Put myself to be the head of the list */
	    head->prev = nd;
	    nd->next = head;
	    nd->prev = NULL;
	    head = nd;
	    head->cost = 0;
	    head->distance = 0;
	    head->forwarder = head;
	}
	else {
	    /* Add the node to the tail of the linked list */
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
/* void Dj_Set_Routes(int dummy_int, void *dummy)          */
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

void Dj_Set_Routes(int dummy_int, void *dummy) {
    stdit it, c_it;
    Node *nd, *dest, *tmp;
    Edge *edge;
    State_Chain *s_chain;


    Dj_Build_Q_Set();
	
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
	 if(!stdhash_is_end(&All_Edges, &it)) {
	     s_chain = *((State_Chain **)stdhash_it_val(&it));
	     stdhash_begin(&s_chain->states, &c_it);
	     while(!stdhash_is_end(&s_chain->states, &c_it)) {
		 edge = *((Edge **)stdhash_it_val(&c_it));
		 if(edge->cost < 0) {
		     stdhash_it_next(&c_it);
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
		 stdhash_it_next(&c_it);
	     }
	 }
    }

    Alarm(DEBUG, "\n\n\n\n");


}


/***********************************************************/
/* Node* Dj_Get_Route(int32 dest)                          */
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

Node* Dj_Get_Route(int32 dest)
{
    stdit it;
    Node *nd;
    
    stdhash_find(&All_Nodes, &it, &dest);
    if(stdhash_is_end(&All_Nodes, &it))
	return NULL;
    nd = *((Node **)stdhash_it_val(&it));
    return nd->forwarder;
}


/* For debugging only */

void Dj_Print_Routes(FILE *fp) 
{
    stdit it;
    char line[256];
    Node *nd;

    sprintf(line, "Dijkstra ROUTES:\n");
    Alarm(PRINT, "%s", line);
    if (fp != NULL) fprintf(fp, "%s", line);

    stdhash_begin(&All_Nodes, &it); 
    while(!stdhash_is_end(&All_Nodes, &it)) {
        nd = *((Node **)stdhash_it_val(&it));
	if(nd->address == My_Address) {
	    sprintf(line, "%d.%d.%d.%d \tLOCAL NODE \n", 
		  IP1(nd->address), IP2(nd->address), 
		  IP3(nd->address), IP4(nd->address)); 
        Alarm(PRINT, "%s", line);
        if (fp != NULL) fprintf(fp, "%s", line);
	}
	else if(nd->forwarder != NULL) {
	    sprintf(line, "%d.%d.%d.%d via: %d.%d.%d.%d dist: %d; cost: %d\n", 
		  IP1(nd->address), IP2(nd->address), 
		  IP3(nd->address), IP4(nd->address), 
		  IP1(nd->forwarder->address), IP2(nd->forwarder->address), 
		  IP3(nd->forwarder->address), IP4(nd->forwarder->address),
		  nd->distance, nd->cost);
        Alarm(PRINT, "%s", line);
        if (fp != NULL) fprintf(fp, "%s", line);
	}
	else {
	    sprintf(line, "%d.%d.%d.%d  !!! NO ROUTE \n", 
		  IP1(nd->address), IP2(nd->address), 
		  IP3(nd->address), IP4(nd->address)); 
        Alarm(PRINT, "%s", line);
        if (fp != NULL) fprintf(fp, "%s", line);
	}
	stdhash_it_next(&it);
    }
    sprintf(line, "\n\n");
    Alarm(PRINT, "%s", line);
    if (fp != NULL) fprintf(fp, "%s", line);
}


