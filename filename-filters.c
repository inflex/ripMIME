/*------------------------------------------------------------------------
 Module:        /extra/development/xamime/xamime_working/ripmime/filename-filters.c
 Author:        Paul L Daniels
 Project:       ripMIME
 State:         Release
 Creation Date: 01 Jan 03
 Description:   Filename Filters is a module which is designed to check and 'safety-enhance'
                filenames which are passed to it.  This may include things like removing
                directory risers ( ../.. ), root directory attempts ( / ), and parameter passing.
------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>


#include "pldstr.h"
#include "logger.h"
#include "filename-filters.h"


#ifndef FL
#define FL __FILE__, __LINE__
#endif

#define FNFILTER_DEBUG_PEDANTIC 10
#define FNFILTER_DEBUG_NORMAL 1

// Debug precodes
#define FNFILTER_DPEDANTIC ((glb.debug >= FNFILTER_DEBUG_PEDANTIC))
#define FNFILTER_DNORMAL   ((glb.debug >= FNFILTER_DEBUG_NORMAL  ))

#define DFN if ((glb.debug >= FNFILTER_DEBUG_NORMAL))


struct FNFILTER_globals {
	int debug;
	int verbose;
	int paranoid;

	int x_mac;
};

static struct FNFILTER_globals glb;


int FNFILTER_init( void )
{
	glb.debug = 0;
	glb.verbose = 0;
	glb.paranoid = 0;
	glb.x_mac = 0;

	return 0;
}


int FNFILTER_set_debug( int level )
{
	glb.debug = level;

	return glb.debug;
}

int FNFILTER_set_verbose( int level )
{
	glb.verbose = level;

	return glb.verbose;
}


int FNFILTER_set_mac( int level )
{
	glb.x_mac = level;

	return glb.x_mac;
}

int FNFILTER_set_paranoid( int level )
{
	glb.paranoid = level;

	return glb.paranoid;
}


/*------------------------------------------------------------------------
Procedure:     quick_clean_filename ID:1
Purpose:       Removes non-7bit characers from the filename
Input:         char *fname: Null terminated string
Output:
Errors:
------------------------------------------------------------------------*/
int FNFILTER_paranoid_filter( char *fname, int size )
{
	char tmp[1024];
	char *p;


	/* Scan for . and .. filenames 
		** 20040727-12H54
		** Patch supplied by Marco Ariano
		** Patch modified by Paul L Daniels
		**
	*/	
	if ((1 == size)&&('.' == *fname))
	{
		*fname = '_';
		return 0;
	} else if ((2 == size)&&(0 == strncmp(fname,"..",2))) {
		snprintf(fname,3,"__");
		return 0;
	}
	
	
	/* scan out any directory separators */

	p = strrchr(fname,'/');
	if (p)
	{
		// Check to see that this seperator isn't the -last- char in the string
		if (*(p+1) == '\0') *p = '\0';
		else
		{
			p++;
			PLD_strncpy(tmp,p,sizeof(tmp));
			PLD_strncpy(fname,tmp,size);
		}
	}
	else if ( (p = strrchr(fname,'\\')))
	{
		// Check to see that this seperator isn't the -last- char in the string
		if (*(p+1) == '\0') *p = '\0';
		else
		{
			p++;
			PLD_strncpy(tmp,p,sizeof(tmp));
			PLD_strncpy(fname,tmp,size);
		}
	}


	if ( glb.paranoid > 0 )
	{
		// If we're really paranoid, then we go along and convert anything we don't like
		//	the look of into 7-bit
		//
		//	These days we shouldn't be using this any more as there are many filenames
		//	which require > 7-bit charsets.

		while (*fname)
		{
			if( !isalnum((int)*fname) && (*fname != '.') ) *fname='_';
			if( (*fname < ' ')||(*fname > '~') ) *fname='_';
			fname++;
		}
	}

	return 0;
}









/*------------------------------------------------------------------------
Procedure:     MIME_decode_filename ID:1
Purpose:       Removed spurilous characters from filename strings.
Input:         char *fname: null terminated character string
Output:
Errors:
------------------------------------------------------------------------*/
int FNFILTER_filter( char *fname, int size )
{
	int fnl;
	char tmp[1024];
	char *p;

	if (fname == NULL) return 0;
	
  	fnl = strlen(fname);

	DFN LOGGER_log("%s:%d:FNFILTER_filter:DEBUG: fname[%d chars] = '%s'\n", FL, fnl, fname );

	/** If we're handling a Mac file, prefilter **/
	if (glb.x_mac == 1)
	{
		char *q = fname;

		DFN LOGGER_log("%s:%d:FNFILTER_filter:DEBUG: Filtering x-mac filename '%s'",FL,fname);
		while (*q)
		{
			if (*q == '/') *q = '-'; /** Convert the Mac / separator to a hyphen **/
			q++;
		}
		DFN LOGGER_log("%s:%d:FNFILTER_filter:DEBUG: x-mac filename is now '%s'",FL,fname);
	}


	/* We only look at trimming the quotes off a filename if it has more than 2 chars
		* because obviously we'll need to strip off 2 chars (leading and finishing quote)
		*/
	if ( fnl > 2 )
	{

		/* if the MIME_filename starts and ends with "'s */
		if ((fname[0] == '\"') && (fname[fnl-1] == '\"'))
		{
			if (FNFILTER_DNORMAL) LOGGER_log("%s:%d:FNFILTER_filter:DEBUG: Trimming quotes off filename\n", FL );

			/* reduce the file namelength by two*/
			fnl = fnl -2; // 17-11-2002: was =-2, thanks to Vasily Chernikov for spotting the glaring error!

			/* shuffle the MIME_filename chars down */
			memmove(fname,fname+1,fnl);

			/* terminate the string */
			fname[fnl] = '\0';
			if (FNFILTER_DNORMAL) LOGGER_log("%s:%d:FNFILTER_filter:DEBUG: Trimming filename done, fname = '%s'\n", FL, fname );
		} /* if */
	} /* if */

	p = strrchr(fname,'/');
	if (p)
	{
		p++;
		PLD_strncpy( tmp, p, sizeof(tmp) );
		PLD_strncpy( fname, tmp, size);

	} else {

		// Check for Windows/DOS backslash seperator

		p = strrchr( fname, '\\' );
		if ( p )
		{
			if ( *(p+1) != '"' )
			{
				p++;
				PLD_strncpy( tmp, p, sizeof(tmp) );
				PLD_strncpy( fname, tmp, size );
			}
		}
	}

	// Scan for ? symbols - these are often used to make the email client pass paremeters to the filename
	// 	Check though to see that the previous character is not a '=' symbol, because if it is, then we
	//	actually have an ISO encoded filename

	p = strchr( fname, '?' );
	if (p != NULL)
	{
		if (p > fname)
		{
			if (*(p-1) != '=')
			{
				*p = '\0';
			} else {
				// leave the ? alone, as it's part of an ISO encoded filename
			}

		} else {
			// First char of the filename is a '?', change this to a hypen.
			*p = '-';
		}
	}
		
	if (FNFILTER_DNORMAL) LOGGER_log("%s:%d:FNFILTER_filter:DEBUG: Starting paranoia filter\n", FL );

	FNFILTER_paranoid_filter( fname, strlen( fname ) );

	if (FNFILTER_DNORMAL) LOGGER_log("%s:%d:FNFILTER_filter:DEBUG: paranoia filter done. Filename='%s'\n", FL, fname );

	return 0;
}





