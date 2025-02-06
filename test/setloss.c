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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "../spines_lib.h"


void print_usage(void);

int main(int argc, char *argv[])
{
    int source, dest, port, sk, ret;
    float loss_rate, burst_rate;
    int i1, i2, i3, i4;


    port = 8100;
    
    if((argc < 5)||(argc > 6)) {
	print_usage();
	return(1);    
    }
    
    sscanf(argv[1], "%f", &loss_rate);
    sscanf(argv[2], "%f", &burst_rate);

    sscanf(argv[3] ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
    source = ((i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4);

    sscanf(argv[4] ,"%d.%d.%d.%d",&i1, &i2, &i3, &i4);
    dest = ((i1 << 24 ) | (i2 << 16) | (i3 << 8) | i4);

    if(argc == 6) {
	sscanf(argv[5], "%d", &port);
    }

    sk = spines_socket(port, dest, NULL);
    if (sk < 0) {
	printf("socket error\n");
	exit(1);
    }


    ret = spines_setloss(sk, source, loss_rate, burst_rate);
    if( ret < 0 ) {
	printf("setloss error\n");
	exit(1);
    }
    
    spines_close(sk);
    
    return(1);
}

void print_usage(void) {
    printf("Usage:\tsetloss loss_rate burst_rate source_ip destination_ip [port]\n\n");
}

