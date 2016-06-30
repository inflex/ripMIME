#include <stdio.h>

/*-----------------------------------------------------------------\
 Function Name	: int
 Returns Type	: unsigned
 	----Parameter List
	1. get_byte_value( unsigned char *start , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int get_int8( char *start )
{
	return (int) *start;
}


/*-----------------------------------------------------------------\
  Date Code:	: 20081101-014815
  Function Name	: char
  Returns Type	: unsigned
  	----Parameter List
	1. get_uint8( char *start , 
	------------------
Exit Codes	: 
Side Effects	: 
--------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
unsigned int get_uint8( char *start ) {
	return (unsigned char) *start;
}

/*-----------------------------------------------------------------\
 Function Name	: int
 Returns Type	: unsigned
 	----Parameter List
	1. get_ushort_value( unsigned char *start , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int get_int16( char *start )
{
	int value = 0;

	value = (unsigned char)*start | (((unsigned char)*(start +1)) << 8);

	return value;
}

/*-----------------------------------------------------------------\
  Date Code:	: 20081101-014643
  Function Name	: int
  Returns Type	: unsigned
  	----Parameter List
	1. get_uint16( char *start , 
	------------------
Exit Codes	: 
Side Effects	: 
--------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
unsigned int get_uint16( char *start )
{
	unsigned int value = 0;

	value = (unsigned char)*start | (((unsigned char)*(start +1)) << 8);

	return value;
}

/*-----------------------------------------------------------------\
 Function Name	: int
 Returns Type	: unsigned
 	----Parameter List
	1. get_ulong_value( unsigned char *start , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int get_int32( char *start )
{
	int value = 0;

	value = (int)((unsigned char)*start)
		|(((unsigned char)*(start +1)) << 8) 
		|(((unsigned char)*(start +2)) << 16) 
		|(((unsigned char)*(start +3)) << 24);

//	printf("String=0x%x %x %x %x:%u = %d\n", *start, *(start +1), *(start +2), *(start +3), *(start +3), value);

	return value;
}

/*-----------------------------------------------------------------\
  Date Code:	: 20081101-014748
  Function Name	: int
  Returns Type	: unsigned
  	----Parameter List
	1. get_uint32( char *start , 
	------------------
Exit Codes	: 
Side Effects	: 
--------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
unsigned int get_uint32( char *start ) {
	unsigned int value = 0;

	value = (unsigned int)((unsigned char)*start)
		|(((unsigned char)*(start +1)) << 8) 
		|(((unsigned char)*(start +2)) << 16) 
		|(((unsigned char)*(start +3)) << 24);

	return value;

}

