#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>

#include "logger.h"
#include "pldstr.h"
#include "boundary-stack.h"

#ifndef FL
#define FL __FILE__,__LINE__
#endif

#define BS_STRLEN_MAX 1024
#define BS_BOUNDARY_DETECT_LIMIT_DEFAULT 4
#define DBS if (glb.debug)

struct BS_globals {
	int debug;
	int verbose;
	int syslogging;
	int errlogging;
	int count;
	int detect_limit;
	int hold_limit;
	int smallest_length;
	int have_empty_boundary;
	struct BS_node *boundarystack;
	char boundarystacksafe[BS_STRLEN_MAX];
};

struct BS_node {
	char *boundary;
	int boundary_length;
	int boundary_nhl;  // length of boundary without hyphens
	struct BS_node *next;
};


static struct BS_globals glb;





/*-----------------------------------------------------------------\
 Function Name	: BS_init
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
int BS_init( void )
{
	glb.debug = 0;
	glb.verbose = 0;
	glb.syslogging = 1;
	glb.errlogging = 0;
	glb.count = 0;
	glb.detect_limit = BS_BOUNDARY_DETECT_LIMIT_DEFAULT;
	glb.hold_limit = 0;
	glb.boundarystack = NULL;
	glb.smallest_length = -1;
	glb.have_empty_boundary = 0;

	return 0;
}

/*-----------------------------------------------------------------\
 Function Name	: BS_set_hold_limit
 Returns Type	: int
 	----Parameter List
	1. int limit , how many boundary strings to hold
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int BS_set_hold_limit( int limit )
{
	glb.hold_limit = limit;

	return 0;
}

/*-----------------------------------------------------------------\
 Function Name	: BS_set_verbose
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
int BS_set_verbose( int level )
{
	glb.verbose = level;

	return glb.verbose;
}

/*-----------------------------------------------------------------\
 Function Name	: BS_set_debug
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
int BS_set_debug( int level )
{
	glb.debug = level;

	return glb.debug;
}

/*-----------------------------------------------------------------\
 Function Name	: BS_set_boundary_detect_limit
 Returns Type	: int
 	----Parameter List
	1. int limit , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int BS_set_boundary_detect_limit( int limit )
{
	if ((limit > 0)&&(limit < BS_STRLEN_MAX))
	{
		glb.detect_limit = limit;
	}

	return glb.detect_limit;
}


/*-----------------------------------------------------------------\
 Function Name	: BS_clear
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
int BS_clear( void )
{
	struct BS_node *next;

	while (glb.boundarystack)
	{
		DBS LOGGER_log("%s:%d:BS_clear:DEBUG: Popping off %p",FL,glb.boundarystack);
		next = glb.boundarystack->next;
		free(glb.boundarystack->boundary);
		free(glb.boundarystack);
		glb.boundarystack = next;
	}

	glb.boundarystack = NULL;
	glb.count = 0;
	glb.smallest_length = -1;

	return 0;
}



/*-----------------------------------------------------------------\
 Function Name	: BS_non_hyphen_length
 Returns Type	: int
 	----Parameter List
	1. char *boundary , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int BS_non_hyphen_length( char *boundary )
{
	int count = 0;
	char *p = boundary;

	while (*p) { if (isalnum((int)*p)) count++; p++; };

	return count;
}

/*-----------------------------------------------------------------\
 Function Name	: BS_push
 Returns Type	: int
 	----Parameter List
	1. char *boundary , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int BS_push( char *boundary )
{

	struct BS_node *node;

	if ((glb.hold_limit > 0)&&(glb.count >= glb.hold_limit)) 
	{
		DBS LOGGER_log("%s:%d:BS_push:DEBUG: Number of boundaries to hold is at limit (limit=%d)",FL,glb.hold_limit);
		return 0;
	}
  
	node	= malloc(sizeof(struct BS_node));

	DBS LOGGER_log("%s:%d:BS_push:DEBUG: head = %p, nn = %p boundary = '%s'",FL, glb.boundarystack, node, boundary);

	if (node)
	{
		node->next = glb.boundarystack;
		glb.boundarystack = node;
		glb.boundarystack->boundary = strdup(boundary);
		glb.boundarystack->boundary_length = strlen(glb.boundarystack->boundary);
		if (glb.boundarystack->boundary_length == 0) glb.have_empty_boundary = 1;
		glb.boundarystack->boundary_nhl = BS_non_hyphen_length(boundary);
		glb.count++;

		// Set the smallest length
		if (glb.smallest_length == -1) glb.smallest_length = glb.boundarystack->boundary_length;
		else if (glb.boundarystack->boundary_length < glb.smallest_length) glb.smallest_length = glb.boundarystack->boundary_length;

		DBS LOGGER_log("%s:%d:DEBUGX: smallest = %d, NHL = %d,  boundary = '%s'",FL,glb.smallest_length, node->boundary_nhl, boundary);
	}
	else
	    {
		LOGGER_log("%s:%d:BS_push:ERROR: Cannot allocate memory for boundary stack PUSH, %s", FL, strerror(errno));
	}

	return 0;
}


/*-----------------------------------------------------------------\
 Function Name	: *BS_pop
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
char *BS_pop( void )
{

	struct BS_node *node = glb.boundarystack;

	if (glb.boundarystack)
	{
		glb.boundarystack = glb.boundarystack->next;
		PLD_strncpy(glb.boundarystacksafe,node->boundary, BS_STRLEN_MAX);
		free(node->boundary);
		free(node);
		glb.count--;
	}

	return glb.boundarystacksafe;
}

/*-----------------------------------------------------------------\
 Function Name	: *BS_top
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
char *BS_top( void )
{

	if (glb.boundarystack)
	{
		return glb.boundarystack->boundary;
	}
	else return NULL;
}

/*-----------------------------------------------------------------\
 Function Name	: BS_count
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
int BS_count( void )
{
	return glb.count;
}


/*-----------------------------------------------------------------\
 Function Name	: BS_is_long_enough
 Returns Type	: int
 	----Parameter List
	1. int blen , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int BS_is_long_enough( int blen )
{
	if (glb.smallest_length == -1) return 0;
	if (blen >= glb.smallest_length) return 1;

	return 0;
}

/*-----------------------------------------------------------------\
 Function Name	: BS_boundary_detect
 Returns Type	: int
 	----Parameter List
	1. char *needle, 
	2.  char *haystack , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int BS_boundary_detect( char *haystack, char *needle, int needle_length )
{
	int result=1;
	int current_start = glb.detect_limit;
	char *haystack_start;

	if ((glb.have_empty_boundary == 1)&&(needle_length < 1))
	{
		result = strncmp(haystack,"--",2);
		DBS LOGGER_log("%s:%d:BS_boundary_detect:DEBUG: empty-boundary test, result = %d",FL, result);
		return result;

	}

	if ((needle_length < 1)&&(glb.have_empty_boundary == 0)) return 1;


	DBS LOGGER_log("%s:%d:BS_boundary_detect: needle='%s', length=%d, haystack='%s', shift-window=%d"
			,FL
			,needle
			,needle_length
			,haystack
			,current_start
			);

	haystack_start = haystack;
	while ((current_start-- > 0)&&(*haystack_start != '\0'))
	{
		DBS LOGGER_log("%s:%d:BS_boundary_detect:DEBUG: CMP '%s' to '%s'",FL, needle, haystack_start);
		if (strncmp( needle, haystack_start, needle_length )==0)
		{
			DBS LOGGER_log("%s:%d:BS_boundary_detect:DEBUG: Hit on compare",FL);
			result = 0;  
			break; 
		}
		haystack_start++;
	}

	return result;
}



/*-----------------------------------------------------------------\
 Function Name	: BS_cmp
 Returns Type	: int
 	----Parameter List
	1. char *boundary, the boundary we want to check to see if is in
								the stack
	2.  int len , the length of the boundary
 	------------------
 Exit Codes	: 1 == boundary found, 0 == no boundary found
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int BS_cmp( char *boundary, int len )
{

	char testspace[128]; // was 1024
	int testspacelen=127; // was 1023
	int spin=1;
	int nhl=0;
	struct BS_node *node=glb.boundarystack;
	struct BS_node *nodetmp=NULL, *nodedel=NULL;

	if ((!boundary)||(glb.count == 0)) return 0;
	//if ((glb.smallest_length > 0)&&(len < glb.smallest_length)) return 0;
	if (BS_is_long_enough(len) == 0) return 0;

	nhl = BS_non_hyphen_length(boundary);

	DBS LOGGER_log("%s:%d:BS_cmp:DEBUG: possible-boundary='%s', len=%d, smallest=%d, count=%d, NHL=%d"
			, FL
			, boundary
			, len
			, glb.smallest_length
			, glb.count
			, nhl
			);


	// Crop the incoming string to fit in our testspace length
	if (len > testspacelen) len = testspacelen;

	// Copy the potential boundary into our testspace
	snprintf(testspace, testspacelen, "%s", boundary);

	// First, search through the stack looking for a boundary that matches
	// our search criterion
	//
	// When we do find one, we will jump out of this WHILE loop by setting
	// 'spin' to 0.

	while((node)&&(spin))
	{
//		if (node->boundary_length <= len)
		if (node->boundary_nhl == nhl)
		{	
			DBS LOGGER_log("%s:%d:BS_cmp:DEBUG: Comparing '%s' to '%s'", FL, boundary, node->boundary);
			// * 20040903-08H57:PLD: Set boundary length comparison from > 0 to >= 0
			if ((node->boundary != NULL)&&(node->boundary_length >= 0))
			{
				if ((BS_boundary_detect(testspace, node->boundary, node->boundary_length))==0)
				{
					DBS LOGGER_log("%s:%d:BS_cmp:DEBUG: Boundary HIT",FL);
					spin = 0;
				}
			}
		}

		if (spin != 0) node = node->next;
	}

	// If we have a hit on the matching, then, according
	// to nested MIME rules, we must "remove" any previous
	// boundaries
	//
	// We know that we had a HIT in matching if spin == 0, because
	// in our previous code block that's what we set spin to if
	if(spin==0)
	{

		DBS LOGGER_log("%s:%d:BS_cmp:DEBUG: Boundary hit on '%s' == '%s'",FL, boundary,node->boundary);

		// If our "HIT" node is /NOT/ the one on the top of the
		// stack, then we need to pop off and deallocate the nodes
		// PRIOR/Above the hit node.
		//
		// ie, if "NODE" is not the top, then pop off until we
		// do get to the node

		if (node != glb.boundarystack)
		{
			nodetmp = glb.boundarystack;
			while ((nodetmp)&&(nodetmp != node))
			{
				// - Set the node to delete (nodedel) to the current temp
				// node (notetmp)
				// - Increment the nodetmp to the next node in the stack
				// - Free the node to delete (nodedel)

				nodedel = nodetmp;
				nodetmp = nodetmp->next;
				free(nodedel->boundary);
				free(nodedel);
			}
			glb.boundarystack = node;
		}
		return 1;

	}
	else
	{
		return 0;
	}

	return 0;
}



