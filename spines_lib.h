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


#ifndef SPINES_LIB_H
#define SPINES_LIB_H


#ifdef __cplusplus
extern "C" {
#endif

#define     UDP_LINKS            0
#define     RELIABLE_LINKS       1


int  spines_socket(int port, int address, int *flags); 
int  spines_bind(int sk, int port);
void spines_close(int sk);
int  spines_sendto(int sk, int address, int port, char *buff, int len);
int  spines_recvfrom(int sk, int *sender, int *port, char *buff, int len);
    
int spines_join(int sk, int address);
int spines_leave(int sk, int address);

int  spines_listen(int sk);
int  spines_accept(int sk, int on_port, int on_address, int *flags);
int  spines_connect(int sk, int address, int port);
int  spines_send(int sk, char *buff, int len);
int  spines_recv(int sk, char *buff, int len);

int  spines_setloss(int sk, int address, float loss, float burst);

#ifdef __cplusplus
}
#endif

#endif
