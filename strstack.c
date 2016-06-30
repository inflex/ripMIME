#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include "logger.h"
#include "pldstr.h"
#include "strstack.h"

#ifndef FL
#define FL __FILE__,__LINE__
#endif

#define DSS if (ss->debug)


/*-----------------------------------------------------------------\
 Function Name	: SS_init
 Returns Type	: int
 	----Parameter List
	1. void , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int SS_init( struct SS_object *ss )
{
	ss->debug = 0;
	ss->verbose = 0;
	ss->count = 0;
	ss->stringstack = NULL;

	return 0;
}

/*-----------------------------------------------------------------\
 Function Name	: SS_set_verbose
 Returns Type	: int
 	----Parameter List
	1. int level , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int SS_set_verbose( struct SS_object *ss, int level )
{
	ss->verbose = level;

	return ss->verbose;
}

/*-----------------------------------------------------------------\
 Function Name	: SS_set_debug
 Returns Type	: int
 	----Parameter List
	1. int level , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int SS_set_debug( struct SS_object *ss, int level )
{
	ss->debug = level;

	return ss->debug;
}

/*-----------------------------------------------------------------\
 Function Name	: SS_done
 Returns Type	: int
 	----Parameter List
	1. void , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int SS_done( struct SS_object *ss )
{
	struct SS_node *next;

	while ((ss->stringstack != NULL)&&(ss->count > 0))
	{
		DSS LOGGER_log("%s:%d:SS_done: Popping off %s",FL,ss->stringstack->data);
		next = ss->stringstack->next;
		free(ss->stringstack->data);
		free(ss->stringstack);
		ss->stringstack = next;
		ss->count--;
	}

	ss->stringstack = NULL;
	ss->count = 0;

	return 0;
}



/*-----------------------------------------------------------------\
 Function Name	: SS_dump
 Returns Type	: int
 	----Parameter List
	1. struct SS_object *ss , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int SS_dump( struct SS_object *ss )
{
	struct SS_node *n = ss->stringstack;

	while (n != NULL)
	{
		LOGGER_log("%s",n->data);
		n = n->next;
	}

	return 0;
}

/*-----------------------------------------------------------------\
 Function Name	: SS_push
 Returns Type	: int
 	----Parameter List
	1. char *string , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int SS_push( struct SS_object *ss, char *data, size_t data_length )
{

	struct SS_node *node = malloc(sizeof(struct SS_node));


	if (node)
	{
		DSS LOGGER_log("%s:%d:SS_push: Pushing %s to %p, stack count = %d",FL,data, ss->stringstack, ss->count);
		node->next = ss->stringstack;
		ss->stringstack = node;
		ss->stringstack->data = strdup(data);
		ss->stringstack->data_length = data_length;
		ss->count++;
	}
	else
	    {
		LOGGER_log("%s:%d:SS_push:ERROR: Cannot allocate memory for string stack PUSH, %s", FL, strerror(errno));
	}

	return 0;
}


/*-----------------------------------------------------------------\
 Function Name	: *SS_pop
 Returns Type	: char
 	----Parameter List
	1. void , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
char *SS_pop( struct SS_object *ss )
{

	struct SS_node *node = ss->stringstack;

	if ((ss->stringstack)&&(ss->count > 0))
	{
		ss->stringstack = ss->stringstack->next;
		PLD_strncpy(ss->datastacksafe,node->data, SS_STRLEN_MAX);
		free(node->data);
		free(node);
		ss->count--;
	} else return NULL;

	return ss->datastacksafe;
}

/*-----------------------------------------------------------------\
 Function Name	: *SS_top
 Returns Type	: char
 	----Parameter List
	1. void , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
char *SS_top( struct SS_object *ss )
{

	if (ss->stringstack)
	{
		return ss->stringstack->data;
	}
	else return NULL;
}

/*-----------------------------------------------------------------\
 Function Name	: SS_count
 Returns Type	: int
 	----Parameter List
	1. void , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int SS_count( struct SS_object *ss )
{
	return ss->count;
}

/*-----------------------------------------------------------------\
 Function Name	: *SS_cmp
 Returns Type	: char
 	----Parameter List
	1. struct SS_object *ss, 
	2.  char *find_me , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
char *SS_cmp( struct SS_object *ss, char *find_me, size_t find_me_len )
{
	struct SS_node *n = ss->stringstack;
	int hit=0;

	while ((n != NULL)&&(hit == 0))
	{
		if (strncmp(find_me, n->data, find_me_len) == 0) hit++;
		if (hit == 0) n = n->next;
	}

	if (hit == 0) return NULL;
	else return n->data;
}

