#ifndef lint
static const char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif

#include <stdlib.h>
#include <string.h>

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYPATCH 20070509

#define YYEMPTY (-1)
#define yyclearin    (yychar = YYEMPTY)
#define yyerrok      (yyerrflag = 0)
#define YYRECOVERING (yyerrflag != 0)

extern int yyparse(void);

static int yygrowstack(void);
#define YYPREFIX "yy"
#line 2 "config_parse.y"
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
 *  Yair Amir, Claudiu Danilov, John Schultz, Daniel Obenshain, and Thomas Tantillo.
 *
 * Copyright (c) 2003 - 2016 The Johns Hopkins University.
 * All rights reserved.
 *
 * Major Contributor(s):
 * --------------------
 *    John Lane
 *    Raluca Musaloiu-Elefteri
 *    Nilo Rivera
 *
 */

#include "arch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARCH_PC_WIN95
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/param.h>

#else /* ARCH_PC_WIN95 */
#include <winsock.h>
#endif /* ARCH_PC_WIN95 */

#include "spu_alarm.h"
#include "configuration.h"
#include "spu_memory.h"
#include "spu_objects.h"
#include "conf_body.h"

        int     line_num, semantic_errors;
 extern char    *yytext;
 extern int     yyerror(char *str);
 extern void    yywarn(char *str);
 extern int     yylex();

/* #define MAX_ALARM_FORMAT 40
 static char    alarm_format[MAX_ALARM_FORMAT];
 static int     alarm_precise = 0;
 static int     alarm_custom_format = 0; */

void    parser_init()
{
    /* Defaults Here */
}

/* static char *segment2str(int seg) {
  static char ipstr[40];
  int id = Config->segments[seg].bcast_address;
  sprintf(ipstr, "%d.%d.%d.%d:%d",
  	(id & 0xff000000)>>24,
  	(id & 0xff0000)>>16,
  	(id & 0xff00)>>8,
  	(id & 0xff),
	Config->segments[seg].port);
  return ipstr;
}
static void alarm_print_proc(proc *p, int port) {
  if(port == p->port)
    Alarm(CONF_SYS, "\t%20s: %d.%d.%d.%d\n", p->name,
  	  (p->id & 0xff000000)>>24,
  	  (p->id & 0xff0000)>>16,
  	  (p->id & 0xff00)>>8,
  	  (p->id & 0xff));
  else
    Alarm(CONF_SYS, "\t%20s: %d.%d.%d.%d:%d\n", p->name,
  	  (p->id & 0xff000000)>>24,
  	  (p->id & 0xff0000)>>16,
  	  (p->id & 0xff00)>>8,
  	  (p->id & 0xff),
	  p->port);
}

static int32u name2ip(char *name) {
  int anip, i1, i2, i3, i4;
  struct hostent *host_ptr;

  host_ptr = gethostbyname(name);
  
  if ( host_ptr == 0)
    Alarm( EXIT, "Conf_init: no such host %s\n",
	   name);
  
  memcpy(&anip, host_ptr->h_addr_list[0], 
	 sizeof(int32) );
  anip = htonl( anip );
  i1= ( anip & 0xff000000 ) >> 24;
  i2= ( anip & 0x00ff0000 ) >> 16;
  i3= ( anip & 0x0000ff00 ) >>  8;
  i4=   anip & 0x000000ff;
  return ((i1<<24)|(i2<<16)|(i3<<8)|i4);
}

static  void expand_filename(char *out_string, int str_size, const char *in_string)
{
  const char *in_loc;
  char *out_loc;
  char hostn[MAXHOSTNAMELEN+1];
  
  for ( in_loc = in_string, out_loc = out_string; out_loc - out_string < str_size; in_loc++ )
  {
          if (*in_loc == '%' ) {
                  switch( in_loc[1] ) {
                  case 'h':
                  case 'H':
                          gethostname(hostn, sizeof(hostn) );
                          out_loc += snprintf(out_loc, str_size - (out_loc - out_string), "%s", hostn); 
                          in_loc++;
                          continue;
                  default:
                          break;
                  }

          }
          *out_loc = *in_loc;
          out_loc++;
          if (*in_loc == '\0') break;
  }
  out_string[str_size-1] = '\0';
}

static  int 	get_parsed_proc_info( char *name, proc *p )
{
	int	i;

	for ( i=0; i < num_procs; i++ )
	{
		if ( strcmp( Config->allprocs[i].name, name ) == 0 )
		{
			*p = Config->allprocs[i];
			return( i );
		}
	}
	return( -1 );
}
*/

/* convert_segment_to_string()
 * char * segstr : output string
 * int strsize : length of output string space
 * segment *seg : input segment structure
 * int return : length of string written or -1 if error (like string not have room)
 * 
 *
 * The format of the returned string will be as shown below with each segment appended
 * to the string. Each use of IPB will be replaced with the broadcast IP address, port
 * with the port. The optional section is a list of interfaces tagged with D or C
 * and idnetified by ip address. 
 *
 * "Segment IP:port host1name host1ip (ALL/ANY/C/D/M IP)+ host2name host2ip (ALL/ANY/C/D/M IP )+ ..."
 *
 */
/* static  int    convert_segment_to_string(char *segstr, int strsize, segment *seg)
{
    int         i,j;
    size_t      curlen = 0;
    char        temp_str[200];

    sprintf(temp_str, "Segment %d.%d.%d.%d:%d ", 
            (seg->bcast_address & 0xff000000)>>24, 
            (seg->bcast_address & 0xff0000)>>16, 
            (seg->bcast_address & 0xff00)>>8, 
            (seg->bcast_address & 0xff), 
            seg->port );

    strncat( segstr, temp_str, strsize - curlen);
    curlen += strlen(temp_str);

    for (i = 0; i < seg->num_procs; i++) {
        sprintf(temp_str, "%s %d.%d.%d.%d ", 
                seg->procs[i]->name, 
                (seg->procs[i]->id & 0xff000000)>>24, 
                (seg->procs[i]->id & 0xff0000)>>16, 
                (seg->procs[i]->id & 0xff00)>>8, 
                (seg->procs[i]->id & 0xff) );
        strncat( segstr, temp_str, strsize - curlen);
        curlen += strlen(temp_str); */

        /* Now add all interfaces */
        /* for ( j=0 ; j < seg->procs[i]->num_if; j++) { */
            /* add addional interface specs to string */
            /* if ( seg->procs[i]->ifc[j].type & IFTYPE_ANY )
            {
                strncat( segstr, "ANY ", strsize - curlen);
                curlen += 4;
            }
            if ( seg->procs[i]->ifc[j].type & IFTYPE_DAEMON )
            {
                strncat( segstr, "D ", strsize - curlen);
                curlen += 2;
            }
            if ( seg->procs[i]->ifc[j].type & IFTYPE_CLIENT )
            {
                strncat( segstr, "C ", strsize - curlen);
                curlen += 2;
            }
            if ( seg->procs[i]->ifc[j].type & IFTYPE_MONITOR )
            {
                strncat( segstr, "M ", strsize - curlen);
                curlen += 2;
            }
            sprintf(temp_str, "%d.%d.%d.%d ", 
                (seg->procs[i]->ifc[j].ip & 0xff000000)>>24, 
                (seg->procs[i]->ifc[j].ip & 0xff0000)>>16, 
                (seg->procs[i]->ifc[j].ip & 0xff00)>>8, 
                (seg->procs[i]->ifc[j].ip & 0xff) );
            strncat( segstr, temp_str, strsize - curlen);
            curlen += strlen(temp_str);
        }
    } */

    /* terminate each segment by a newline */
    /* strncat( segstr, "\n", strsize - curlen);
    curlen += 1;

    if (curlen > strsize) { */
        /* ran out of space in string -- should never happen. */
/*        Alarmp( SPLOG_ERROR, CONF_SYS, "config_parse.y:convert_segment_to_string: The segment string is too long! %d characters attemped is more then %d characters allowed", curlen, strsize);
        Alarmp( SPLOG_ERROR, CONF_SYS, "config_parse.y:convert_segment_to_string:The error occured on segment %d.%d.%d.%d. Successful string was: %s\n",
                (seg->bcast_address & 0xff000000)>>24, 
                (seg->bcast_address & 0xff0000)>>16, 
                (seg->bcast_address & 0xff00)>>8, 
                (seg->bcast_address & 0xff), 
                segstr);
        return(-1);
    }

    Alarmp( SPLOG_DEBUG, CONF_SYS, "config_parse.y:convert_segment_to_string:The segment string is %d characters long:\n%s", curlen, segstr);
    return(curlen);
}

#define PROC_NAME_CHECK( stoken ) { \
                                            char strbuf[80]; \
                                            int ret; \
                                            proc p; \
                                            if ( strlen((stoken)) >= MAX_PROC_NAME ) { \
                                                snprintf(strbuf, 80, "Too long name(%d max): %s)\n", MAX_PROC_NAME, (stoken)); \
                                                return (yyerror(strbuf)); \
                                            } \
                                            ret = get_parsed_proc_info( stoken, &p ); \
                                            if (ret >= 0) { \
                                                snprintf(strbuf, 80, "Name not unique. name: %s equals (%s, %d.%d.%d.%d)\n", (stoken), p.name, IP1(p.id), IP2(p.id), IP3(p.id), IP4(p.id) ); \
                                                return (yyerror(strbuf)); \
                                            } \
                                         }
#define PROCS_CHECK( num_procs, stoken ) { \
                                            char strbuf[80]; \
                                            if ( (num_procs) >= MAX_PROCS_RING ) { \
                                                snprintf(strbuf, 80, "%s (Too many daemons configured--%d max)\n", (stoken), MAX_PROCS_RING); \
                                                return (yyerror(strbuf)); \
                                            } \
                                         }
#define SEGMENT_CHECK( num_segments, stoken )  { \
                                            char strbuf[80]; \
                                            if ( (num_segments) >= MAX_SEGMENTS ) { \
                                                snprintf(strbuf, 80, "%s (Too many segments configured--%d max)\n", (stoken), MAX_SEGMENTS); \
                                                return( yyerror(strbuf)); \
                                            } \
                                         }
#define SEGMENT_SIZE_CHECK( num_procs, stoken )  { \
                                            char strbuf[80]; \
                                            if ( (num_procs) >= MAX_PROCS_SEGMENT ) { \
                                                snprintf(strbuf, 80, "%s (Too many daemons configured in segment--%d max)\n", (stoken), MAX_PROCS_SEGMENT); \
                                                return( yyerror(strbuf)); \
                                            } \
                                         }
#define INTERFACE_NUM_CHECK( num_ifs, stoken )  { \
                                            char strbuf[80]; \
                                            if ( (num_ifs) >= MAX_INTERFACES_PROC ) { \
                                                snprintf(strbuf, 80, "%s (Too many interfaces configured in proc--%d max)\n", (stoken), MAX_INTERFACES_PROC); \
                                                return( yyerror(strbuf)); \
                                            } \
                                         }

*/
#line 324 "y.tab.c"
#define OPENBRACE 257
#define CLOSEBRACE 258
#define EQUALS 259
#define COLON 260
#define BANG 261
#define DEBUGFLAGS 262
#define CRYPTO 263
#define SIGLENBITS 264
#define MPBITMASKSIZE 265
#define DIRECTEDEDGES 266
#define PATHSTAMPDEBUG 267
#define UNIXDOMAINPATH 268
#define RRCRYPTO 269
#define ITCRYPTO 270
#define ORDEREDDELIVERY 271
#define REINTRODUCEMSGS 272
#define TCPFAIRNESS 273
#define SESSIONBLOCKING 274
#define MSGPERSAA 275
#define SENDBATCHSIZE 276
#define ITMODE 277
#define RELIABLETIMEOUTFACTOR 278
#define NACKTIMEOUTFACTOR 279
#define INITNACKTOFACTOR 280
#define ACKTO 281
#define PINGTO 282
#define DHTO 283
#define INCARNATIONTO 284
#define MINRTTMS 285
#define ITDEFAULTRTT 286
#define PRIOCRYPTO 287
#define DEFAULTPRIO 288
#define MAXMESSSTORED 289
#define MINBELLYSIZE 290
#define DEFAULTEXPIRESEC 291
#define DEFAULTEXPIREUSEC 292
#define GARBAGECOLLECTIONSEC 293
#define RELCRYPTO 294
#define RELSAATHRESHOLD 295
#define HBHADVANCE 296
#define HBHACKTIMEOUT 297
#define HBHOPT 298
#define E2EACKTIMEOUT 299
#define E2EOPT 300
#define LOSSTHRESHOLD 301
#define LOSSCALCDECAY 302
#define LOSSCALCTIMETRIGGER 303
#define LOSSCALCPKTTRIGGER 304
#define LOSSPENALTY 305
#define PINGTHRESHOLD 306
#define STATUSCHANGETIMEOUT 307
#define HOSTS 308
#define EDGES 309
#define SP_BOOL 310
#define SP_TRIVAL 311
#define DDEBUG 312
#define DEXIT 313
#define DPRINT 314
#define DDATA_LINK 315
#define DNETWORK 316
#define DPROTOCOL 317
#define DSESSION 318
#define DCONF 319
#define DALL 320
#define DNONE 321
#define IPADDR 322
#define NUMBER 323
#define DECIMAL 324
#define STRING 325
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    1,    1,    1,    1,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    3,    5,    5,    6,    4,    7,    7,    8,
};
short yylen[] = {                                         2,
    1,    2,    2,    2,    0,    3,    3,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
    4,    2,    1,    2,    4,    2,    1,    3,
};
short yydefred[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    2,
    3,    4,    6,    7,    8,    9,   10,   11,   29,   12,
   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   24,   25,   26,   27,   28,   30,   31,   32,   33,
   34,   35,   36,   37,   38,   39,   40,   41,   42,   43,
   44,   45,   46,   47,   48,   49,   50,    0,    0,    0,
    0,    0,    0,   54,   51,   52,    0,   55,   56,   58,
};
short yydgoto[] = {                                      48,
   49,   50,   51,   52,  149,  150,  152,  153,
};
short yysindex[] = {                                   -263,
 -209, -208, -207, -206, -205, -204, -203, -202, -201, -200,
 -199, -198, -197, -196, -195, -194, -193, -192, -191, -190,
 -189, -188, -187, -186, -185, -184, -183, -182, -181, -180,
 -179, -178, -177, -176, -175, -174, -173, -172, -171, -170,
 -169, -168, -167, -166, -165, -162, -161,    0,    0, -263,
 -263, -263, -213, -225, -224, -210, -164, -223, -163, -160,
 -159, -158, -157, -156, -222, -220, -155, -219, -218, -217,
 -215, -214, -212, -211, -154, -153, -152, -151, -150, -149,
 -148, -147, -146, -145, -144, -143, -142, -139, -141, -132,
 -140, -138, -136, -135, -134, -133, -131, -130, -129,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0, -216, -137, -130,
 -128, -127, -129,    0,    0,    0, -126,    0,    0,    0,
};
short yyrindex[] = {                                    110,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  110,
  110,  110,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0, -125,
    0,    0, -124,    0,    0,    0,    0,    0,    0,    0,
};
short yygindex[] = {                                      0,
   -3,    0,    0,    0,  -37,    0,  -39,    0,
};
#define YYTABLESIZE 197
short yytable[] = {                                       1,
    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,
   12,   13,   14,   15,   16,   17,   18,   19,   20,   21,
   22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
   32,   33,   34,   35,   36,   37,   38,   39,   40,   41,
   42,   43,   44,   45,   46,   47,  100,  101,  102,   53,
   54,   55,   56,   57,   58,   59,   60,   61,   62,   63,
   64,   65,   66,   67,   68,   69,   70,   71,   72,   73,
   74,   75,   76,   77,   78,   79,   80,   81,   82,   83,
   84,   85,   86,   87,   88,   89,   90,   91,   92,   93,
   94,   95,   96,   97,   98,   99,  103,  104,  105,  106,
  115,  108,  116,  118,  119,  154,  120,  121,  122,    5,
  123,  124,  156,  159,    0,    0,    0,    0,    0,    0,
  155,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  158,    0,   53,   57,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  107,  109,    0,    0,  110,
  111,  112,  113,  114,  117,    0,    0,  127,    0,    0,
    0,    0,    0,    0,  134,    0,  136,    0,  125,  126,
  138,  128,  129,  130,  131,  132,  133,  140,  135,    0,
  137,  139,    0,  141,    0,  142,  143,  144,  145,  146,
    0,  147,  148,  151,  157,    0,  160,
};
short yycheck[] = {                                     263,
  264,  265,  266,  267,  268,  269,  270,  271,  272,  273,
  274,  275,  276,  277,  278,  279,  280,  281,  282,  283,
  284,  285,  286,  287,  288,  289,  290,  291,  292,  293,
  294,  295,  296,  297,  298,  299,  300,  301,  302,  303,
  304,  305,  306,  307,  308,  309,   50,   51,   52,  259,
  259,  259,  259,  259,  259,  259,  259,  259,  259,  259,
  259,  259,  259,  259,  259,  259,  259,  259,  259,  259,
  259,  259,  259,  259,  259,  259,  259,  259,  259,  259,
  259,  259,  259,  259,  259,  259,  259,  259,  259,  259,
  259,  259,  259,  259,  257,  257,  310,  323,  323,  310,
  323,  325,  323,  323,  323,  322,  324,  323,  323,    0,
  323,  323,  150,  153,   -1,   -1,   -1,   -1,   -1,   -1,
  258,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  258,   -1,  258,  258,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  310,  310,   -1,   -1,  310,
  310,  310,  310,  310,  310,   -1,   -1,  310,   -1,   -1,
   -1,   -1,   -1,   -1,  310,   -1,  310,   -1,  323,  323,
  310,  323,  323,  323,  323,  323,  323,  310,  323,   -1,
  323,  323,   -1,  324,   -1,  324,  323,  323,  323,  323,
   -1,  323,  323,  323,  323,   -1,  323,
};
#define YYFINAL 48
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 325
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"OPENBRACE","CLOSEBRACE","EQUALS",
"COLON","BANG","DEBUGFLAGS","CRYPTO","SIGLENBITS","MPBITMASKSIZE",
"DIRECTEDEDGES","PATHSTAMPDEBUG","UNIXDOMAINPATH","RRCRYPTO","ITCRYPTO",
"ORDEREDDELIVERY","REINTRODUCEMSGS","TCPFAIRNESS","SESSIONBLOCKING","MSGPERSAA",
"SENDBATCHSIZE","ITMODE","RELIABLETIMEOUTFACTOR","NACKTIMEOUTFACTOR",
"INITNACKTOFACTOR","ACKTO","PINGTO","DHTO","INCARNATIONTO","MINRTTMS",
"ITDEFAULTRTT","PRIOCRYPTO","DEFAULTPRIO","MAXMESSSTORED","MINBELLYSIZE",
"DEFAULTEXPIRESEC","DEFAULTEXPIREUSEC","GARBAGECOLLECTIONSEC","RELCRYPTO",
"RELSAATHRESHOLD","HBHADVANCE","HBHACKTIMEOUT","HBHOPT","E2EACKTIMEOUT",
"E2EOPT","LOSSTHRESHOLD","LOSSCALCDECAY","LOSSCALCTIMETRIGGER",
"LOSSCALCPKTTRIGGER","LOSSPENALTY","PINGTHRESHOLD","STATUSCHANGETIMEOUT",
"HOSTS","EDGES","SP_BOOL","SP_TRIVAL","DDEBUG","DEXIT","DPRINT","DDATA_LINK",
"DNETWORK","DPROTOCOL","DSESSION","DCONF","DALL","DNONE","IPADDR","NUMBER",
"DECIMAL","STRING",
};
char *yyrule[] = {
"$accept : Config",
"Config : ConfigStructs",
"ConfigStructs : ParamStruct ConfigStructs",
"ConfigStructs : HostStruct ConfigStructs",
"ConfigStructs : EdgeStruct ConfigStructs",
"ConfigStructs :",
"ParamStruct : CRYPTO EQUALS SP_BOOL",
"ParamStruct : SIGLENBITS EQUALS NUMBER",
"ParamStruct : MPBITMASKSIZE EQUALS NUMBER",
"ParamStruct : DIRECTEDEDGES EQUALS SP_BOOL",
"ParamStruct : PATHSTAMPDEBUG EQUALS SP_BOOL",
"ParamStruct : UNIXDOMAINPATH EQUALS STRING",
"ParamStruct : ITCRYPTO EQUALS SP_BOOL",
"ParamStruct : ORDEREDDELIVERY EQUALS SP_BOOL",
"ParamStruct : REINTRODUCEMSGS EQUALS SP_BOOL",
"ParamStruct : TCPFAIRNESS EQUALS SP_BOOL",
"ParamStruct : SESSIONBLOCKING EQUALS SP_BOOL",
"ParamStruct : MSGPERSAA EQUALS NUMBER",
"ParamStruct : SENDBATCHSIZE EQUALS NUMBER",
"ParamStruct : ITMODE EQUALS SP_BOOL",
"ParamStruct : RELIABLETIMEOUTFACTOR EQUALS NUMBER",
"ParamStruct : NACKTIMEOUTFACTOR EQUALS NUMBER",
"ParamStruct : INITNACKTOFACTOR EQUALS DECIMAL",
"ParamStruct : ACKTO EQUALS NUMBER",
"ParamStruct : PINGTO EQUALS NUMBER",
"ParamStruct : DHTO EQUALS NUMBER",
"ParamStruct : INCARNATIONTO EQUALS NUMBER",
"ParamStruct : MINRTTMS EQUALS NUMBER",
"ParamStruct : ITDEFAULTRTT EQUALS NUMBER",
"ParamStruct : RRCRYPTO EQUALS SP_BOOL",
"ParamStruct : PRIOCRYPTO EQUALS SP_BOOL",
"ParamStruct : DEFAULTPRIO EQUALS NUMBER",
"ParamStruct : MAXMESSSTORED EQUALS NUMBER",
"ParamStruct : MINBELLYSIZE EQUALS NUMBER",
"ParamStruct : DEFAULTEXPIRESEC EQUALS NUMBER",
"ParamStruct : DEFAULTEXPIREUSEC EQUALS NUMBER",
"ParamStruct : GARBAGECOLLECTIONSEC EQUALS NUMBER",
"ParamStruct : RELCRYPTO EQUALS SP_BOOL",
"ParamStruct : RELSAATHRESHOLD EQUALS NUMBER",
"ParamStruct : HBHADVANCE EQUALS SP_BOOL",
"ParamStruct : HBHACKTIMEOUT EQUALS NUMBER",
"ParamStruct : HBHOPT EQUALS SP_BOOL",
"ParamStruct : E2EACKTIMEOUT EQUALS NUMBER",
"ParamStruct : E2EOPT EQUALS SP_BOOL",
"ParamStruct : LOSSTHRESHOLD EQUALS DECIMAL",
"ParamStruct : LOSSCALCDECAY EQUALS DECIMAL",
"ParamStruct : LOSSCALCTIMETRIGGER EQUALS NUMBER",
"ParamStruct : LOSSCALCPKTTRIGGER EQUALS NUMBER",
"ParamStruct : LOSSPENALTY EQUALS NUMBER",
"ParamStruct : PINGTHRESHOLD EQUALS NUMBER",
"ParamStruct : STATUSCHANGETIMEOUT EQUALS NUMBER",
"HostStruct : HOSTS OPENBRACE HostList CLOSEBRACE",
"HostList : Host HostList",
"HostList : Host",
"Host : NUMBER IPADDR",
"EdgeStruct : EDGES OPENBRACE EdgeList CLOSEBRACE",
"EdgeList : Edge EdgeList",
"EdgeList : Edge",
"Edge : NUMBER NUMBER NUMBER",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#if YYDEBUG
#include <stdio.h>
#endif

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 500

int      yydebug;
int      yynerrs;
int      yyerrflag;
int      yychar;
short   *yyssp;
YYSTYPE *yyvsp;
YYSTYPE  yyval;
YYSTYPE  yylval;

/* variables for the parser stack */
static short   *yyss;
static short   *yysslim;
static YYSTYPE *yyvs;
static int      yystacksize;
#line 403 "config_parse.y"
void yywarn(char *str) {
        fprintf(stderr, "-------Parse Warning-----------\n");
        fprintf(stderr, "Parser warning on or before line %d\n", line_num);
        fprintf(stderr, "Error type; %s\n", str);
        fprintf(stderr, "Offending token: %s\n", yytext);
}
int yyerror(char *str) {
  fprintf(stderr, "-------------------------------------------\n");
  fprintf(stderr, "Parser error on or before line %d\n", line_num);
  fprintf(stderr, "Error type; %s\n", str);
  fprintf(stderr, "Offending token: %s\n", yytext);
  exit(1);
}
#line 656 "y.tab.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(void)
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = yyssp - yyss;
    newss = (yyss != 0)
          ? (short *)realloc(yyss, newsize * sizeof(*newss))
          : (short *)malloc(newsize * sizeof(*newss));
    if (newss == 0)
        return -1;

    yyss  = newss;
    yyssp = newss + i;
    newvs = (yyvs != 0)
          ? (YYSTYPE *)realloc(yyvs, newsize * sizeof(*newvs))
          : (YYSTYPE *)malloc(newsize * sizeof(*newvs));
    if (newvs == 0)
        return -1;

    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse(void)
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;

    yyerror("syntax error");

#ifdef lint
    goto yyerrlab;
#endif

yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yyvsp[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 6:
#line 330 "config_parse.y"
{ Conf_set_all_crypto(yyvsp[0].boolean); }
break;
case 7:
#line 331 "config_parse.y"
{ Conf_set_signature_len_bits(yyvsp[0].number); }
break;
case 8:
#line 332 "config_parse.y"
{ Conf_set_multipath_bitmask_size(yyvsp[0].number); }
break;
case 9:
#line 333 "config_parse.y"
{ Conf_set_directed_edges(yyvsp[0].boolean); }
break;
case 10:
#line 334 "config_parse.y"
{ Conf_set_path_stamp_debug(yyvsp[0].boolean); }
break;
case 11:
#line 335 "config_parse.y"
{ Conf_set_unix_domain_path(yyvsp[0].string); }
break;
case 12:
#line 337 "config_parse.y"
{ Conf_set_IT_crypto(yyvsp[0].boolean); }
break;
case 13:
#line 338 "config_parse.y"
{ Conf_set_IT_ordered_delivery(yyvsp[0].boolean); }
break;
case 14:
#line 339 "config_parse.y"
{ Conf_set_IT_reintroduce_messages(yyvsp[0].boolean); }
break;
case 15:
#line 340 "config_parse.y"
{ Conf_set_IT_tcp_fairness(yyvsp[0].boolean); }
break;
case 16:
#line 341 "config_parse.y"
{ Conf_set_IT_session_blocking(yyvsp[0].boolean); }
break;
case 17:
#line 342 "config_parse.y"
{ Conf_set_IT_msg_per_saa(yyvsp[0].number); }
break;
case 18:
#line 343 "config_parse.y"
{ Conf_set_IT_send_batch_size(yyvsp[0].number); }
break;
case 19:
#line 344 "config_parse.y"
{ Conf_set_IT_intrusion_tolerance_mode(yyvsp[0].boolean); }
break;
case 20:
#line 345 "config_parse.y"
{ Conf_set_IT_reliable_timeout_factor(yyvsp[0].number); }
break;
case 21:
#line 346 "config_parse.y"
{ Conf_set_IT_nack_timeout_factor(yyvsp[0].number); }
break;
case 22:
#line 347 "config_parse.y"
{ Conf_set_IT_init_nack_timeout_factor(yyvsp[0].decimal); }
break;
case 23:
#line 348 "config_parse.y"
{ Conf_set_IT_ack_timeout(yyvsp[0].number); }
break;
case 24:
#line 349 "config_parse.y"
{ Conf_set_IT_ping_timeout(yyvsp[0].number); }
break;
case 25:
#line 350 "config_parse.y"
{ Conf_set_IT_dh_timeout(yyvsp[0].number); }
break;
case 26:
#line 351 "config_parse.y"
{ Conf_set_IT_incarnation_timeout(yyvsp[0].number); }
break;
case 27:
#line 352 "config_parse.y"
{ Conf_set_IT_min_RTT_ms(yyvsp[0].number); }
break;
case 28:
#line 353 "config_parse.y"
{ Conf_set_IT_default_RTT(yyvsp[0].number); }
break;
case 29:
#line 355 "config_parse.y"
{ Conf_set_RR_crypto(yyvsp[0].boolean); }
break;
case 30:
#line 357 "config_parse.y"
{ Conf_set_Prio_crypto(yyvsp[0].boolean); }
break;
case 31:
#line 358 "config_parse.y"
{ Conf_set_Prio_default_prio(yyvsp[0].number); }
break;
case 32:
#line 359 "config_parse.y"
{ Conf_set_Prio_max_mess_stored(yyvsp[0].number); }
break;
case 33:
#line 360 "config_parse.y"
{ Conf_set_Prio_min_belly_size(yyvsp[0].number); }
break;
case 34:
#line 361 "config_parse.y"
{ Conf_set_Prio_default_expire_sec(yyvsp[0].number); }
break;
case 35:
#line 362 "config_parse.y"
{ Conf_set_Prio_default_expire_usec(yyvsp[0].number); }
break;
case 36:
#line 363 "config_parse.y"
{ Conf_set_Prio_garbage_collection_sec(yyvsp[0].number); }
break;
case 37:
#line 365 "config_parse.y"
{ Conf_set_Rel_crypto(yyvsp[0].boolean); }
break;
case 38:
#line 366 "config_parse.y"
{ Conf_set_Rel_saa_threshold(yyvsp[0].number); }
break;
case 39:
#line 367 "config_parse.y"
{ Conf_set_Rel_hbh_advance(yyvsp[0].boolean); }
break;
case 40:
#line 368 "config_parse.y"
{ Conf_set_Rel_hbh_ack_timeout(yyvsp[0].number); }
break;
case 41:
#line 369 "config_parse.y"
{ Conf_set_Rel_hbh_ack_optimization(yyvsp[0].boolean); }
break;
case 42:
#line 370 "config_parse.y"
{ Conf_set_Rel_e2e_ack_timeout(yyvsp[0].number); }
break;
case 43:
#line 371 "config_parse.y"
{ Conf_set_Rel_e2e_ack_optimization(yyvsp[0].boolean); }
break;
case 44:
#line 373 "config_parse.y"
{ Conf_set_Reroute_loss_threshold(yyvsp[0].decimal); }
break;
case 45:
#line 374 "config_parse.y"
{ Conf_set_Reroute_loss_calc_decay(yyvsp[0].decimal); }
break;
case 46:
#line 375 "config_parse.y"
{ Conf_set_Reroute_loss_calc_time_trigger(yyvsp[0].number); }
break;
case 47:
#line 376 "config_parse.y"
{ Conf_set_Reroute_loss_calc_pkt_trigger(yyvsp[0].number); }
break;
case 48:
#line 377 "config_parse.y"
{ Conf_set_Reroute_loss_penalty(yyvsp[0].number); }
break;
case 49:
#line 378 "config_parse.y"
{ Conf_set_Reroute_ping_threshold(yyvsp[0].number); }
break;
case 50:
#line 379 "config_parse.y"
{ Conf_set_Reroute_status_change_timeout(yyvsp[0].number); }
break;
case 51:
#line 382 "config_parse.y"
{ Conf_validate_hosts(); }
break;
case 54:
#line 389 "config_parse.y"
{ Conf_add_host(yyvsp[-1].number, yyvsp[0].ip.addr.s_addr); }
break;
case 58:
#line 399 "config_parse.y"
{ Conf_add_edge(yyvsp[-2].number, yyvsp[-1].number, yyvsp[0].number); }
break;
#line 1029 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;

yyoverflow:
    yyerror("yacc stack overflow");

yyabort:
    return (1);

yyaccept:
    return (0);
}
