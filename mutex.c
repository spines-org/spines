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
 * Copyright (c) 2003 - 2008 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
 *
 */



#include "mutex.h"


int Mutex_Init(pthread_mutex_t  *mutex)
{
#ifdef _REENTRANT
    return pthread_mutex_init(mutex,  NULL);
#else
    return 0;
#endif
}

int Mutex_Trylock(pthread_mutex_t  *mutex)
{
#ifdef _REENTRANT
    return pthread_mutex_trylock(mutex);
#else
    return 0;
#endif
}

int Mutex_Lock(pthread_mutex_t  *mutex)
{
#ifdef _REENTRANT
    return pthread_mutex_lock(mutex);
#else
    return 0;
#endif
}

int Mutex_Unlock(pthread_mutex_t  *mutex)
{
#ifdef _REENTRANT
    return pthread_mutex_unlock(mutex);
#else
    return 0;
#endif
}

