/*-----------------------------------------------------------------------
 **
 **
 ** MIME_headers
 **
 ** Written by Paul L Daniels, originally for the Xamime project
 ** (http://www.xamime.com) but since spawned off to the ripMIME/alterMIME
 ** family of email parsing tools.
 **
 ** Copyright PLD, 1999,2000,2001,2002,2003
 ** Licence: BSD
 ** For more information on the licence and copyrights of this code, please
 **	email copyright@pldaniels.com

 ** CHANGES
 ** 2003-Jun-24: PLD: Added subject parsing
 **

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "ffget.h"
#include "pldstr.h"
#include "libmime-decoders.h"
#include "logger.h"
#include "strstack.h"
#include "boundary-stack.h"
#include "filename-filters.h"
#include "MIME_headers.h"


#ifndef FL
#define FL __FILE__, __LINE__
#endif

// Debug precodes
#define MIMEH_DPEDANTIC ((glb.debug >= _MIMEH_DEBUG_PEDANTIC))
#define MIMEH_DNORMAL   ((glb.debug >= _MIMEH_DEBUG_NORMAL  ))

#define DMIMEH if ((glb.debug >= _MIMEH_DEBUG_NORMAL))


char *MIMEH_defect_description_array[_MIMEH_DEFECT_ARRAY_SIZE];



struct MIMEH_globals {

	int doubleCR;
	int doubleCR_save;
	char doubleCRname[_MIMEH_STRLEN_MAX +1];

	char appledouble_filename[_MIMEH_STRLEN_MAX +1];

	char subject[_MIMEH_STRLEN_MAX +1];

	char *headerline;
	char *headerline_original;	// Holds the original header-form without decoding.
	int save_headers;
	int save_headers_original;
	int test_mailbox;
	int debug;
	int webform;
	int doubleCR_count;
	int header_fix;
	int verbose;
	int verbose_contenttype;

	int header_longsearch; // keep searching until valid headers are found - this is used to filter out qmail bounced emails - breaks RFC's but people are wanting it :-(
	int longsearch_limit;	// how many segments do we attempt to look ahead...

	char output_dir[_MIMEH_STRLEN_MAX +1];

	FILE *header_file;

	FILE *original_header_file;
	int original_header_save_to_file;
};

static struct MIMEH_globals glb;



/*-----------------------------------------------------------------\
  Function Name	: MIMEH_version
  Returns Type	: int
  ----Parameter List
  1. void, 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_version(void)
{
	fprintf(stdout,"mimeheaders: %s\n", MIMEH_VERSION);

	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_init
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
int MIMEH_init( void )
{

	glb.doubleCR = 0;
	glb.headerline = NULL;
	glb.headerline_original = NULL;
	glb.header_file = NULL;
	glb.original_header_file = NULL;
	glb.original_header_save_to_file = 0;

	glb.save_headers = 0;
	glb.save_headers_original = 0;
	glb.test_mailbox = 0;
	glb.debug = 0;
	glb.webform = 0;
	glb.doubleCR_count = 0;
	glb.doubleCR_save = 1;
	glb.header_fix = 1;
	glb.verbose = 0;
	glb.verbose_contenttype = 0;
	glb.output_dir[0]='\0';
	glb.doubleCRname[0]='\0';
	glb.appledouble_filename[0]='\0';
	glb.header_longsearch=0;
	glb.longsearch_limit=1;

	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_get_doubleCR
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
int MIMEH_get_doubleCR( void )
{
	return glb.doubleCR;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_doubleCR
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
int MIMEH_set_doubleCR( int level )
{
	glb.doubleCR = level;

	return glb.doubleCR;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_headerfix
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
int MIMEH_set_headerfix( int level )
{
	glb.header_fix = level;
	return glb.header_fix;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_doubleCR_save
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
int MIMEH_set_doubleCR_save( int level )
{
	glb.doubleCR_save = level;

	return glb.doubleCR_save;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_get_doubleCR_save
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
int MIMEH_get_doubleCR_save( void )
{
	return glb.doubleCR_save;
}


/*-----------------------------------------------------------------\
  Function Name	: *MIMEH_get_doubleCR_name
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
char *MIMEH_get_doubleCR_name( void )
{
	return glb.doubleCRname;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_debug
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
int MIMEH_set_debug( int level )
{
	glb.debug = level;
	return glb.debug;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_outputdir
  Returns Type	: int
  ----Parameter List
  1. char *dir , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_set_outputdir( char *dir )
{
	if (dir) snprintf(glb.output_dir,_MIMEH_STRLEN_MAX,"%s",dir);
	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_webform
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
int MIMEH_set_webform( int level )
{
	glb.webform = level;
	return glb.webform;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_mailbox
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
int MIMEH_set_mailbox( int level )
{
	glb.test_mailbox = level;
	return level;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_verbosity
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
int MIMEH_set_verbosity( int level )
{
	glb.verbose = level;
	return level;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_verbosity_contenttype
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
int MIMEH_set_verbosity_contenttype( int level )
{
	glb.verbose_contenttype = level;
	return level;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_get_verbosity_contenttype
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
int MIMEH_get_verbosity_contenttype( void )
{
	return glb.verbose_contenttype;
}



/*------------------------------------------------------------------------
Procedure:     MIMEH_set_headers_save ID:1
Purpose:       Sets MIMEH's headers save file (where MIMEH will save the
headers it reads in from the mailpack)
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIMEH_set_headers_save( FILE *f )
{
	glb.header_file = f;
	glb.save_headers = 1;
	return 0;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_headers_original_save_to_file
  Returns Type	: int
  ----Parameter List
  1. FILE *f , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_set_headers_original_save_to_file( FILE *f )
{
	if (f == NULL) glb.original_header_save_to_file = 0;
	else glb.original_header_save_to_file = 1;
	glb.original_header_file = f;

	return glb.original_header_save_to_file;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_headers_nosave
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
int MIMEH_set_headers_nosave( void )
{
	glb.header_file = NULL;
	glb.save_headers = 0;
	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_get_headers_save
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
int MIMEH_get_headers_save( void )
{
	return glb.save_headers;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_headers_save_original
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
int MIMEH_set_headers_save_original( int level )
{
	glb.save_headers_original = level;

	return glb.save_headers_original;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_get_headers_ptr
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
char *MIMEH_get_headers_ptr( void )
{
	return glb.headerline;
}


/*-----------------------------------------------------------------\
  Function Name	: *MIMEH_get_headers_original_ptr
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
char *MIMEH_get_headers_original_ptr( void )
{
	return glb.headerline_original;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_header_longsearch
  Returns Type	: int
  ----Parameter List
  1. int level , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
The header long-search is a facility switch that will make the
header searching to continue on until it either reaches the end of
the file or it finds valid (??) headers to work on.


--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_set_header_longsearch( int level )
{
	glb.header_longsearch = level;

	return glb.header_longsearch;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_set_defect
  Returns Type	: int
  ----Parameter List
  1. struct MIMEH_header_info *hinfo, 
  2.  int defect ,  The defect code to set
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_set_defect( struct MIMEH_header_info *hinfo, int defect )
{
	if ((defect >= 0)&&(defect < _MIMEH_DEFECT_ARRAY_SIZE))
	{
		hinfo->defects[defect]++;
		hinfo->header_defect_count++;
		DMIMEH LOGGER_log("%s:%d:MIMEH_set_defect:DEBUG: Setting defect index '%d' to '%d'",FL, defect, hinfo->defects[defect]); 
	}
	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_is_contenttype
  Returns Type	: int
  ----Parameter List
  1. int range_type, 
  2.  int content_type , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_is_contenttype( int range_type, int content_type )
{
	int diff;

	diff = content_type -range_type;

	if ((diff < _CTYPE_RANGE)&&(diff > 0)) return 1;
	else return 0;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_is_binary
  Returns Type	: int
  ----Parameter List
  1. struct FFGET_FILE *f , 
  ------------------
  Exit Codes	: 1 = yes, it's binary, 0 = no.
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_is_binary( char *fname )
{
	char buffer[1024];
	int read_count;
	FILE *f;

	f = fopen(fname,"r");
	if (!f) return 1;
	read_count = fread(buffer, 1, 1024, f);
	fclose(f);

	while (read_count)
	{
		read_count--;
		if (buffer[read_count] == 0) return 1;
	}

	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_are_headers_RFC822
  Returns Type	: int
  ----Parameter List
  1. char *fname , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_are_headers_RFC822( char *headers )
{
	char conditions[7][16] = { "received", "from", "subject", "date", "content",  "boundary" };
	int hitcount = 0;
	int condition_item;
	char *lc_headers = NULL;

	if (headers == NULL)
	{
		DMIMEH LOGGER_log("%s:%d:MIMEH_are_headers_RFC822:DEBUG: Headers are NULL");
		return 0;
	}

	DMIMEH LOGGER_log("%s:%d:MIMEH_are_headers_RFC822:DEBUG:----\n%s\n----",FL,headers);

	lc_headers = strdup(headers);
	if (lc_headers == NULL) return 0;

	//PLD_strlower((unsigned char *)lc_headers);
	PLD_strlower(lc_headers);

	DMIMEH LOGGER_log("%s:%d:MIMEH_are_headers_RFC822:DEBUG:----(lowercase)----\n%s\n----",FL,lc_headers);

	for (condition_item=0; condition_item < 6; condition_item++)
	{
		char *p;

		DMIMEH LOGGER_log("%s:%d:MIMEH_are_headers_RFC822:DEBUG: Condition test item[%d] = '%s'",FL,condition_item,conditions[condition_item]);
		p = strstr(lc_headers, conditions[condition_item]);
		if (p != NULL)
		{
			if (p > lc_headers)
			{
				if ((*(p-1) == '\n')||(*(p-1) == '\r'))
					hitcount++;
			} else if (p == lc_headers) hitcount++;
		}
	}

	if (lc_headers != NULL) free(lc_headers);

	return hitcount;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_save_doubleCR
  Returns Type	: int
  ----Parameter List
  1. FFGET_FILE *f , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_save_doubleCR( FFGET_FILE *f )
{
	//char c;
	int c;
	FILE *fo;
	struct stat st;


	// Determine a file name we can use.

	do {
		glb.doubleCR_count++;
		snprintf(glb.doubleCRname,_MIMEH_STRLEN_MAX,"%s/doubleCR.%d",glb.output_dir,glb.doubleCR_count);
	}
	while (stat(glb.doubleCRname, &st) == 0);


	fo = fopen(glb.doubleCRname,"w");
	if (!fo)
	{
		LOGGER_log("%s:%d:MIMEH_save_doubleCR:ERROR: unable to open '%s' to write (%s)", FL,glb.doubleCRname,strerror(errno));
		return -1;
	}

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_save_doubleCR:DEBUG: Saving DoubleCR header: %s\n", FL,glb.doubleCRname);

	while (1)
	{
		c = FFGET_fgetc(f);
		fprintf(fo,"%c",c);

		if ((c == EOF)||(c == '\n'))
		{
			break;
		}
	}

	fclose(fo);

	return 0;
}


/*-----------------------------------------------------------------\
  Function Name	: *
  Returns Type	: char
  ----Parameter List
  1. MIMEH_absorb_whitespace( char *p , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
char * MIMEH_absorb_whitespace( char *p )
{
	if (p)
	{
		while ((*p != '\0')&&((*p == ' ')||(*p == '\t'))) p++;
	}

	return p;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_strip_comments
  Returns Type	: int
  ----Parameter List
  1. char *input , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
Removes comments from RFC[2]822 headers

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_strip_comments( char *input )
{
	char *p,*p_org;
	int in_quote=0;

	if (input == NULL) return 0;

	p = p_org = input;

	do {
		char *q = NULL;

		// Locate (if any) the first occurance of the (
		while ((p_org != NULL)&&((*p_org != '(')||(in_quote==1)))
		{
			switch (*p_org) {
				case '"':
					in_quote ^= 1;
					break;
				case '\n':
				case '\r':
					in_quote = 0;
					break;
				case '\0':
					p_org = NULL;
					break;
			}

			if (p_org) p_org++;
		}

		p = p_org;


		if ((p != NULL)&&(in_quote == 0))
		{
			int stop_searching = 0;

			DMIMEH LOGGER_log("%s:%d:MIMEH_strip_comments:DEBUG: Located open ( at %s",FL,p);
			// If we did locate an opening parenthesis, look for the closing one
			//		NOTE - we cannot have a breaking \n or \r inbetween
			//		q = strpbrk(p, ")\n\r");
			q = p;
			while ( (q != NULL) && (stop_searching == 0) )
			{
				switch (*q) {
					case '\0':
						stop_searching = 1;
						q = NULL;
						break;

					case '\n':
					case '\r':
						stop_searching = 1;
						in_quote = 0;
						break;

					case '"':
						in_quote ^= 1;
						break;

					case ')':
						DMIMEH LOGGER_log("%s:%d:MIMEH_strip_comments:DEBUG: Located closing ) at %s",FL,q);
						if (in_quote == 0) stop_searching = 1;
						break;
				}
				if ((q != NULL)&&(stop_searching == 0)) q++;
			}

			// If we've got both opening and closing, then we need to remove
			//		the contents of the comment, including the parenthesis
			if (q != NULL)
			{
				if (*q != ')')
				{
					// if we found a \n or \r between the two (), then jump out
					// and move p to the next position.
					p_org++;
					continue;

				} else {
					// Move q to the first char after the closing parenthesis
					q++;

					DMIMEH LOGGER_log("%s:%d:MIMEH_strip_comments:DEBUG: located closing ) at %s ",FL, q);
					// While there's more chars in string, copy them to where 
					//		the opening parenthesis is
					while (*q != '\0')
					{
						*p = *q;	
						p++;
						q++;
					} // While q != '\0'
					DMIMEH LOGGER_log("%s:%d:MIMEH_strip_comments:DEBUG: char copy done",FL);

					// Terminate the string
					*p = '\0';
				} // if q !=/= ')'

			} else break; // if q == NULL

		} // if p == NULL

	} while ((p != NULL)&&(p_org != NULL)); // do-while more comments to remove

	DMIMEH LOGGER_log("%s:%d:MIMEH_strip_comments:DEBUG: Final string = '%s'",FL,input);

	return 0;
}



int MIMEH_check_ct(char *q)
{
	char *p=q;
	p++;
	if(*p!='\0' && strcasestr(p,"Content-Type:")==p)return 1;
	p++;
	if(*p!='\0' && strcasestr(p,"Content-Type:")==p)return 1;
	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_fix_header_mistakes
  Returns Type	: int
  ----Parameter List
  1. char *data , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

Some headers are broken in their wrapping, ie, they fail to
put a leading space at the start of the next wrapped data line; ie

Content-Transfer-Encoding: quoted-printable
Content-Disposition: attachment;
filename="yxnjjhyk.xml"

Which causes normal header processing to not locate the filename.

This function will see if there are any lines with a trailing ; that
do not have a leading space on the next line and subsequently replace
the \r\n chars after the ; with blanks, effectively pulling the line up

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_fix_header_mistakes( char *data )
{
	int result = 0;
	char *p;

	DMIMEH LOGGER_log("%s:%d:MIMEH_fix_header_mistakes:DEBUG: Checking and fixing headers in '%s'",FL,data);

	if (glb.header_fix == 0) return result;

	p = data;
	while (p) {
		int nonblank_detected = 0;
		char *q;

		p = strchr(p, ';');
		if (p == NULL) break;


		q = p+1;

		DMIMEH LOGGER_log("%s:%d:MIMEH_fix_header_mistakes:DEBUG: Located ';' at offset %d '%20s",FL, p -data, p);

		if ((*q == '\n')||(*q == '\r')) {
			nonblank_detected = 0;
		} else {
			/** If the ; isn't immediately followed by a \n or \r, then search till
			 ** the end of the line to see if all the chars are blank **/

			while ((*q != '\0')||(*q != '\r')||(*q != '\n')) {
				switch (*q) {
					case '\t':
					case '\n':
					case '\r':
					case ' ':
						nonblank_detected = 0;
						break;
					default:
						nonblank_detected = 1;
				} /*switch*/

				if (nonblank_detected == 1) break;

				q++;
			} /** while looking for the end of the line **/
		} /** ELSE - if *q wasn't a line break char **/

		if (nonblank_detected == 1) {
			DMIMEH LOGGER_log("%s:%d:MIMEH_fix_header_mistakes:DEBUG: Line was normal/safe, continue...",FL);
			p++;
			continue;
		} /** if nonblank_detected == 1 **/

		/** if we had nothing but blanks till the end of the 
		 ** line, then we need to pull up the next line **/
		if (*q != '\0') {
			if(!MIMEH_check_ct(q)){
				DMIMEH LOGGER_log("%s:%d:MIMEH_fix_header_mistakes:DEBUG: Line needs fixing",FL);
				*q = ' ';
				q++;
				if ((*q == '\n')||(*q == '\r')) *q = ' ';
				DMIMEH LOGGER_log("%s:%d:MIMEH_fix_header_mistakes:DEBUG: Line fixed",FL);
				p = q;
			}else{
				p++;
			}
		} /** If q wasn't the end of data **/

	} /** while looking for more ';' chars **/

	DMIMEH LOGGER_log("%s:%d:MIMEH_fix_header_mistakes:DEBUG: Done",FL);

	return result;
}

/*------------------------------------------------------------------------
Procedure:     MIMEH_read_headers ID:1
Purpose:       Reads from the stream F until it detects a From line, or a blank line
(end of headers)
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIMEH_read_headers( struct MIMEH_header_info *hinfo, FFGET_FILE *f )
{
	char buffer[_MIMEH_STRLEN_MAX+1];
	int totalsize=0;
	int linesize=0;
	int totalsize_original=0;
	int result = 0;
	int firstline = 1;
	int search_count=0;
	char *tmp;
	char *tmp_original;
	char *fget_result = NULL;
	char *headerline_end;
	char *p;
	char *linestart;
	char *lineend;

	int is_RFC822_headers=0;	// 20040208-1335:PLD: Added to give an indication if the headers are RFC822 like; used in conjunction with the header_longsearch for pulling apart qmail bouncebacks.

	/**
	  Lets start the ugly big fat DO loop here so that we can, if needed
	  search until we find headers which are actually valid.  Personally
	  I hate this - but people want it in order to detect malformed 
	  (deliberate or otherwise) emails.  It'd be nice if for once in the
	  software world someone actually enforced standards rather than trying
	  to be overly intelligent about interpreting data.  
	 **/

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: File position = %ld [0x%0X]"
			,FL
			,FFGET_ftell(f)
			,FFGET_ftell(f)
			);

	do {

		search_count++;
		glb.headerline = NULL;
		glb.headerline_original = NULL;
		tmp_original = NULL;


		while ((fget_result=FFGET_fgets(buffer,_MIMEH_STRLEN_MAX, f)))
		{

			linestart = buffer;
			linesize = strlen(linestart);
			lineend = linestart +linesize;

			if (strstr(linestart,"\r\n")) hinfo->crlf_count++;
			else if (strstr(linestart,"\r\r")) hinfo->crcr_count++;
			else if (strchr(linestart,'\n')) hinfo->lf_count++;

			if (MIMEH_DNORMAL)LOGGER_log("%s:%d:MIMEH_read_headers: [CRLF=%d, CRCR=%d, LF=%d] Data In=[sz=%d:tb=%d:mem=%p]'%s'",FL, hinfo->crlf_count, hinfo->crcr_count, hinfo->lf_count, linesize, f->trueblank, glb.headerline, buffer);

			// If we are being told to copy the input data to an output file
			//		then do so here (this is for the originals)
			if ((glb.original_header_save_to_file > 0)&&(glb.original_header_file != NULL))
			{
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: saving to file...",FL);
				fprintf(glb.original_header_file,"%s",linestart);
			}

			// if we are being told to keep a copy of the original data
			//	as it comes in from ffget, then do the storage here
			if (glb.save_headers_original > 0)
			{
				if (MIMEH_DNORMAL) LOGGER_log("MIMEH_read_headers:DEBUG:Data-In:[%d:%d] '%s'", strlen(linestart), linesize, linestart);
				tmp_original = realloc(glb.headerline_original, totalsize_original+linesize+1);
				if (tmp_original == NULL)
				{
					LOGGER_log("%s:%d:MIMEH_read_headers:ERROR: Cannot allocate %d bytes to contain new headers_original ", FL,totalsize_original +linesize +1);
					if (glb.headerline_original != NULL) free(glb.headerline_original);
					glb.headerline_original = NULL;
					return -1;
				}

				if (glb.headerline_original == NULL)
				{
					glb.headerline_original = tmp_original;
					totalsize_original = linesize +1;
					PLD_strncpy( glb.headerline_original, linestart, (linesize+1));
					if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: '%s'", FL, glb.headerline_original);
				} else {
					glb.headerline_original = tmp_original;
					PLD_strncpy( (glb.headerline_original +totalsize_original -1), linestart, (linesize +1));
					totalsize_original += linesize;
					if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: HO =  '%s'", FL, glb.headerline_original);
				}
				//LOGGER_log("DEBUG:linesize=%d data='%s'",linesize, linestart);
			}


			/** Normal processing of the headers now starts. **/
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: realloc'ing dataspace",FL); 
			tmp = realloc(glb.headerline, totalsize+linesize+1);
			if (tmp == NULL)
			{
				LOGGER_log("%s:%d:MIMEH_read_headers:ERROR: Cannot allocate %d bytes to contain new headers ", FL,totalsize +linesize +1);
				if (glb.headerline != NULL) free(glb.headerline);
				glb.headerline = NULL;
				return -1;
			}

			if (glb.headerline == NULL)
			{
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: Initial appending of head to dataspace headerline = NULL  realloc block = %p linestart = %p linesize = %d",FL, tmp, linestart, linesize); 
				glb.headerline = tmp;
				totalsize = linesize;
				PLD_strncpy(glb.headerline, linestart, (linesize +1));
				headerline_end = glb.headerline +totalsize;
			} // If the global headerline is currently NULL
			else
			{
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: Appending of new data to existing header  existing-headerline = %p  new realloc block = %p linestart = %p linesize = %d",FL, glb.headerline, tmp, linestart, linesize); 

				// Perform header unfolding by removing any CRLF's
				//	of the last line if the first characters of the
				//	newline are blank/space

				glb.headerline = tmp;

				if ((linestart < lineend)&&((*linestart == '\t')||(*linestart == ' ')))
				{

					// Here we start at the last character of the previous line
					// and check to see if it's a 'space' type charcter, if it is
					// we will then reduce the total size of the headers thus far and
					// move the pointer where we're going to append this new line back
					//	one more character - Ultimately what we wish to achieve is that
					//	the new line will tacked on [sans leading spaces] to the end of
					//	the previous line.
					//
					// 'p' holds the location at the -end- of the current headers where
					//		we are going to append the newly read line


					p = glb.headerline +totalsize -1;
					if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: unwrapping headers headers=%p, p = %p",FL,glb.headerline, p);
					while ((p >= glb.headerline)&&(( *p == '\n' )||( *p == '\r' )))
					{
						if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: Removing trailing space p=[%p]%c",FL, p, *p);
						*p = '\0';
						p--;
						totalsize--;
					}


					p = glb.headerline +totalsize -1;

				} 


				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: Memcopying line, source = %p, dest = %p, size = %d", FL, linestart, glb.headerline +totalsize, linesize);
				memcpy((glb.headerline +totalsize), linestart, (linesize));
				totalsize += linesize;
				*(glb.headerline +totalsize) = '\0';


			}	// If the glb.headerline already is allocated and we're appending to it.


			if (f->trueblank)
			{
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_read_headers:DEBUG: Trueblank line detected in header reading",FL);
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_read_headers:DEBUG: Headers /before/ decoding\n-------\n%s\n-------------------",FL, glb.headerline);

				MIMEH_fix_header_mistakes( glb.headerline );
				MDECODE_decode_ISO( glb.headerline, totalsize  );

				if ((glb.save_headers)&&(glb.headerline))
				{
					if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_read_headers:DEBUG: Saving header line.",FL);
					fprintf(glb.header_file,"%s",glb.headerline);
				}
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_read_headers:DEBUG: Final Headers\n------------------\n%s---------------", FL,glb.headerline);
				//result = 1;
				//result = 0;
				break;
			} // If the last line was in fact a true blank line


			// If there was a doubleCR at the end of the line,
			//	then we need to save the next set of data until there
			//	is a \n

			if (FFGET_doubleCR)
			{
				if (glb.doubleCR_save != 0)
				{
					MIMEH_save_doubleCR(f);
					glb.doubleCR = 1;
				}
				FFGET_doubleCR = 0;
				FFGET_SDL_MODE = 0;
			} // FFGET_doubleCR test

			firstline = 0;
		} // While reading more headers from the source file.


		// If FFGET ran out of data whilst processing the headers, then acknowledge this
		// by returning a -1.
		//
		// NOTE - This does not mean we do not have any data!
		//  it just means that our input ran out.

		if (!fget_result)
		{
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_read_headers:ERROR: FFGET module ran out of input while reading headers",FL);
			/** If we're meant to be saving the headers, we better do that now, even though we couldn't
			 ** read everything we wanted to **/
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_read_headers:DEBUG: save_headers=%d totalsize=%d headerline=%s", FL, glb.save_headers, totalsize, glb.headerline);

			if ((glb.save_headers)&&(glb.headerline))
			{
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_read_headers:DEBUG: Saving header line.",FL);
				MIMEH_fix_header_mistakes( glb.headerline );
				MDECODE_decode_ISO( glb.headerline, totalsize  );
				fprintf(glb.header_file,"%s",glb.headerline);
			}

			result = -1;
		} else {

			if (glb.header_longsearch > 0) {
				/** Test the headers for RFC compliance... **/
				is_RFC822_headers =  MIMEH_are_headers_RFC822(glb.headerline);
				if (is_RFC822_headers == 0)
				{
					/** If not RFC822 headers, then clean up everything we allocated in here **/
					DMIMEH LOGGER_log("%s:%d:MIME_read_headers:DEBUG: No RFC822 headers detected, cleanup.", FL);
					MIMEH_headers_cleanup();
				}
			}
		}
	} while ((is_RFC822_headers==0)&&(glb.header_longsearch>0)&&(result==0)&&(search_count<glb.longsearch_limit));

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_read_headers:DEBUG: Finished.",FL);
	return result;
}



/*------------------------------------------------------------------------
Procedure:     MIMEH_display_info ID:1
Purpose:       DEBUGGING - Displays the values of the hinfo structure to
stderr
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIMEH_display_info( struct MIMEH_header_info *hinfo )
{
	if (hinfo)
	{
		LOGGER_log("%s:%d:MIMEH_display_info:\
				Content Type = %d\n\
				Boundary = %s\n\
				Filename = %s\n\
				name = %s\n\
				Encoding = %d\n\
				Disposit = %d\n\
				"\
				,FL\
				,hinfo->content_type\
				,hinfo->boundary\
				,hinfo->filename\
				,hinfo->name\
				,hinfo->content_transfer_encoding\
				,hinfo->content_disposition);
		fflush(stdout);
	}
	return 0; 

}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_decode_multivalue_language_string
  Returns Type	: int
  ----Parameter List
  1. char *input , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_decode_multivalue_language_string( char *input )
{
	int sq_count = 0;
	int language_set = 0;
	char *q = input;

	DMIMEH LOGGER_log("%s:%d:MIMEH_decode_multivalue_language_string:DEBUG: Decoding '%s'",FL,input);
	// Count the single-quotes
	while ((*q != '\0')&&(sq_count != 2)) if (*q++ == '\'') sq_count++;
	if (sq_count < 2)
	{
		//		LOGGER_log("%s:%d:MIMEH_decode_multivalue_language_string:WARNING: Insufficient single quotes for valid language-charset string",FL);
		q = input;
	} else {
		language_set = 1;
	}

	// q will be pointing at the 2nd single-quote, which is the end of
	// the language encoding set, so we just jump over that and start
	// reading off the data and decoding it.
	MDECODE_decode_multipart( q );

	// If the language was set, we need to move down our decoded data to the 
	//		start of the input buffer
	if (language_set == 1)
	{
		while (*q != '\0') { *input = *q; input++; q++; }
		*input = '\0';
	}

	DMIMEH LOGGER_log("%s:%d:MIMEH_decode_multivalue_language_string:DEBUG: Output = '%s'",FL,q);


	return 0;

}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_recompose_multivalue
  Returns Type	: int
  ----Parameter List
  1. struct MIMEH_header_info *hinfo,  Global header information, can be NULL
  2.  char *header_name_prefix, Prefix we're looking for (ie, filename)
  3.  char *header_value, String which the prefix should exist in 
  4.  char *buffer, Output buffer
  5.  size_t buffer_size , Output buffer size
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
Multivalue strings are ones which appear like:

filename*0*=us-ascii'en-us'attachment%2E%65
filename*1*="xe"

which should duly be recoded as:

filename=attachment.exe

Another example: (extracted from the RFC2231 document)

Content-Type: application/x-stuff
title*0*=us-ascii'en'This%20is%20even%20more%20
title*1*=%2A%2A%2Afun%2A%2A%2A%20
title*2="isn't it!"

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_recompose_multivalue( struct MIMEH_header_info *hinfo, char *header_name_prefix, char *header_value, char *buffer, size_t buffer_size, char **data_end_point )
{
	int result = 0;
	char *start_position = header_value;


	DMIMEH LOGGER_log("%s:%d:MIMEH_recompose_multivalue:DEBUG: seeking for %s in %s and appending to '%s'. Buffer size=%d", FL, header_name_prefix, header_value,buffer, buffer_size );


	// Locate the first part of the multipart string
	start_position = strstr(header_value, header_name_prefix);
	if (start_position != NULL)
	{
		char *q;
		char *buffer_start;

		// Setup our buffer insertion point for what ever new data we extract
		buffer_start = buffer +strlen(buffer);
		buffer_size -= strlen(buffer);

		q = start_position;

		// If the string we're looking for exists, then continue...
		do {
			char *p;
			char *end_point;
			char end_point_char='\0';
			int decode_data=0;
			int q_len;

			p = strstr(q, header_name_prefix);
			if (p == NULL) break;

			DMIMEH LOGGER_log("%s:%d:MIMEH_recompose_multivalue:DEBUG: prefix = %s", FL, p);

			q = strchr(p,'=');
			if (q == NULL) break;

			// Test to see if we have to look for a language encoding specification *sigh*
			if (*(q-1) == '*')
			{
				decode_data=1;
			}

			// Move the pointer past the '=' separator
			q++;

			DMIMEH LOGGER_log("%s:%d:MIMEH_recompose_multivalue:DEBUG: data = %s", FL, q);

			// Find where this multipart string ends
			end_point = strpbrk(q, ";\t\n\r ");
			if (end_point != NULL)
			{
				*end_point = '\0';
				end_point_char = *end_point;
				*data_end_point = end_point; // Set this so we know where to start decoding the next time we call this fn
			} else {
				char *ep;

				// If strpbrk comes up with nothing, then we set the data_end_point to the end of the string
				ep = q;
				while (*ep != '\0') ep++;
				*data_end_point = ep;
			}

			// Trim off quotes.
			if (*q == '"') 
			{ 
				int bl;

				//	LOGGER_log("%s:%d:DEBUG: Trimming '%s'", FL, q);
				q++;
				bl = strlen(q);
				if (*(q +bl -1) == '"') *(q +bl -1) = '\0';
				//LOGGER_log("%s:%d:DEBUG: Trim done, '%s'", FL, q);
			}

			if (decode_data == 1)
			{
				MIMEH_decode_multivalue_language_string(q);
			}

			DMIMEH LOGGER_log("%s:%d:MIMEH_recompose_multivalue:DEBUG: segment value = '%s', appending to '%s'", FL, q, buffer);
			snprintf(buffer_start,buffer_size,"%s",q);
			q_len = strlen(q);
			buffer_size -= q_len;
			buffer_start += q_len;
			DMIMEH LOGGER_log("%s:%d:MIMEH_recompose_multivalue:DEBUG: Buffer[remaining=%d]= '%s'", FL, buffer_size,buffer);

			if (end_point != NULL)
			{
				*end_point = end_point_char; 
				q = end_point +1; 
			}
			else q = NULL;

		} while ((q != NULL)&&(buffer_size > 0));

	}

	DMIMEH LOGGER_log("%s:%d:MIMEH_recompose_multivalue:DEBUG: End point set to: [%d] '%s'",FL, (*data_end_point -header_value), *data_end_point);
	return result;
}


/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_header_parameter
  Returns Type	: int
  ----Parameter List
  1. char *data, 
  2.  char *searchstr, 
  3.  char *output_value, 
  4.  int output_value_size , 
  5.  char *data_end_point, used to keep track of the last point of
  successful data decoding is.
  ------------------
  Exit Codes	: 
  0 = Success, found the required parameter
  1 = No luck, didn't find the required parameter

  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:
11-Aug-2004: Added new variable, data_end_point.  This variable
was required because without it, there was no way of telling
where to continue on the search for more valid data, this is
due to having to no longer rely on fixed atom separators like
: and ; in the MIME text (thankyou MUA's which incorrectly
interpreted the RFC's *sigh*)

\------------------------------------------------------------------*/
int MIMEH_parse_header_parameter( struct MIMEH_header_info *hinfo,  char *data, char *searchstr, char *output_value, int output_value_size, char **data_end_point  )
{
	int return_value = 0;
	char *p;
	char *hl;

	// Set the data end point to be the beginning of the data, as we
	//		have not yet searched through any of the header data
	*data_end_point = data;

	// Duplicate and convert to lowercase the header data
	//		that we have been provided with.
	hl = strdup(data);
	//PLD_strlower((unsigned char *)hl);
	PLD_strlower(hl);

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: Seeking '%s' in '%s'", FL, searchstr, hl);

	// Look for the search string we're after (ie, filename, name, location etc)
	if (strncmp(hl,searchstr,strlen(searchstr))==0) p = hl; else p = NULL;
	//	p = strstr (hl, searchstr); //TESTING 
	if (p != NULL)
	{
		char *string = NULL;

		if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: found %s in %s", FL, searchstr, p);

		// Work out where in the -original- string the located parameter is.
		//		We need to work from the original string because we need to 
		//		preserve case and our searching string is in _lower-case_.
		//	
		//	After we've located it, we offset the pointer past the string we
		//		searched for.  At this position, we should see a separator of
		//		some type in the set [*;:=\t ].

		string = p -hl +data +strlen(searchstr);

		/**
		 ** After searching for our parameter, if we've got a
		 ** basic match via strstr, we should then proceed to
		 ** check that the characters either side of it are
		 ** relevant to a typical parameter specification
		 **
		 ** the characters *, =, <space> and tab can succeed a
		 ** parameter name.
		 **/
		switch (*string) {
			case '*':
			case '=':
			case ' ':
			case '\t':
				/**
				 ** Permitted characters were found after the parameter name
				 ** so continue on...
				 **/
				break;
			default:
				/**
				 ** Something other than the permitted characters was found, 
				 ** this implies (assumed) that the string match was actually
				 ** just a bit of good luck, return to caller 
				 **/
				if (hl) free(hl);
				return 1;
		} /** Switch **/

		/**
		 ** Don't forget to also test the character _BEFORE_ the search string
		 **/
		if (1)
		{
			char *before_string;

			before_string = string -1 -strlen(searchstr);
			if (before_string >= data)
			{
				/**
				 ** The characters, <space>, <tab>, ;, : may preceed a parameter name
				 **/
				switch (*(before_string)) {
					case ';':
					case ':':
					case ' ':
					case '\t':
						/**
						 ** Permitted characters were found after the parameter name
						 ** so continue on...
						 **/
						break;
					default:
						/**
						 ** Something other than the permitted characters was found, 
						 ** this implies (assumed) that the string match was actually
						 ** just a bit of good luck, return to caller 
						 **/
						if (hl) free(hl);
						return 1;
				} /** Switch before_string **/
			} /** if before_string > data **/
		} /** 1 **/


		// If the char is a '*', this means we've got a multivalue parameter
		//		which needs to be decoded (ie, name*1*=foo name*2*=bar )
		if (*string == '*')
		{
			DMIMEH LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: Found a '*' after the name, so attempting multipart value decode",FL);

			// PLD:DEV:11/08/2004-18H30
			//	Issue: RFC2231 handling
			return_value = MIMEH_recompose_multivalue( hinfo, searchstr, data, output_value, output_value_size, data_end_point);


		} else {

			// skip any spaces
			while (isspace((int) *string )) string++;

			//if ( *string != '=' )
			if ( *string == '\0' )
			{
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: In '%s' parsing, was expecting a '=' in the start of '%s'\n", FL, searchstr, string );

			}
			else {
				char *endchar;

				// Eliminate multiple = separators.
				// Reference: c030804-006a
				//	PLD:DEV: 11/08/2004-15H15
				while ((*(string +1) == '=')&&(*(string+1) != '\0')) { string++; MIMEH_set_defect(hinfo,MIMEH_DEFECT_MULTIPLE_EQUALS_SEPARATORS); }


				// Get the end of our string
				endchar = string +strlen(string) -1;
				*data_end_point = endchar;

				// Strip off trailing whitespace
				while ((endchar > string)&&(isspace((int)*endchar)))
				{
					*endchar = '\0';
					endchar--;
				}

				// we are at the '=' in the header, so skip it
				if (*string == '=') string++;
				else {
					MIMEH_set_defect(hinfo,MIMEH_DEFECT_MISSING_SEPARATORS);
				}

				// skip any spaces... again
				while ( isspace((int) *string ) ) string++;

				DMIMEH LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: Extracting value out of '%s'",FL,string);

				// Because of all the potential exploits and bad behaviour
				//		we have to be really careful about how we determine
				//		what the enclosed string is for our parameter.
				//
				//	Previously we could _assume_ that we just get the last 
				//		quote (") on the line and copy out what was between,
				//		unfortunately that doesn't work anymore. Instead now
				//		we have to step along the data stream one char at a
				//		time and make decisions along the way.

				switch (*string) {
					case '\"':
					{
						// If our first char is a quote, then we'll then try and find
						//		the second quote which closes the string, alas, this is
						//		not always present in the header data, either due to a
						//		broken MUA or due to an exploit attempt.
						char *string_end;

						DMIMEH LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: Using quoted-string tests",FL);

						// Remove multiple-sequential quotes
						string++;
						while ((*string != '\0')&&(*string == '\"')){ string++; MIMEH_set_defect(hinfo,MIMEH_DEFECT_MULTIPLE_QUOTES); }

						if (*string == '\0') break; // 20071030-0958: Added by Claudio Jeker - prevents overflow.

						// Find the next quote which isn't sequential to the above
						//		quotes that we just skipped over
						string_end = strchr(string+1, '\"');
						if (string_end != NULL)
						{
							DMIMEH LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: End of value found",FL);
							*string_end = '\0';
							*data_end_point = string_end +1;
						} else {
							// If string_end == NULL
							//
							// If we didn't find any more quotes, that 
							//		means we've probably got an unbalanced string (oh joy)
							//		so then we convert to looking for other items such as
							//		;\n\r\t and space.
							//	
							if (hinfo) MIMEH_set_defect(hinfo,MIMEH_DEFECT_UNBALANCED_QUOTES);
							string_end = strpbrk(string,"; \n\r\t");
							if (string_end != NULL)
							{
								*string_end = '\0';
								*data_end_point = string_end +1;
							} else {
								// There is no termination to the string, instead the
								//		end of the string is \0.
							}
						}
					}
					break;

					default:
					{
						char *string_end;

						DMIMEH LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: Using NON-quoted-string tests",FL);
						string_end = strpbrk(string,"; \n\r\t");
						if (string_end != NULL)
						{
							*string_end = '\0';
							*data_end_point = string_end +1;
						} else {
							// There is no termination to the string, instead the
							//		end of the string is \0.
						}
					}
					break;
				} /** end of switch **/

				DMIMEH LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: Extracting value out of '%s'",FL,string);
				// Trim up and leading/trailing quotes
				if (((*string == '\"')&&(*(string +strlen(string)-1) == '\"'))
						|| ((*string == '\'')&&(*(string +strlen(string)-1) == '\'')) )
				{
					int slen = strlen(string) -2;
					char *s = string;
					if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse-header_parameter:DEBUG: Stripping quotes from '%s'",FL,string);
					while (slen > 0)
					{
						*s = *(s+1);
						s++;
						slen--;
					}
					*s = '\0';

				}

				// Now that our string is all cleaned up, save it to our output value
				snprintf( output_value, output_value_size, "%s", string );
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: Final value = '%s'",FL, output_value);
			} // If the first non-whitespace char wasn't a '='
		} // If the first char after the search-string wasn't a '*'

	}
	else {
		return_value = 1;
	}

	if (hl != NULL) free(hl);

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_header_parameter:DEBUG: [return=%d] Done seeking for '%s' data_end_point=%p (from %p)",FL, return_value, searchstr, *data_end_point, data);

	return return_value;

}







/*-----------------------------------------------------------------\
  Function Name	: MIMEH_is_valid_header_prefix
  Returns Type	: int
  ----Parameter List
  1. char *data, 
  2.  char *prefix_name , 
  ------------------
  Exit Codes	: 
  0 = no, not valid
  1 = yes, valid.
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/

int MIMEH_is_valid_header_prefix( char *data, char *prefix_name )
{
	int plen = strlen(prefix_name);

	/** If our string doesn't start with content-type, then exit **/
	if (strncasecmp(data, prefix_name, plen)!=0)
	{
		return 0;
	} else {
		char end_char;

		/** Test to see that the terminating char after the content-type
		 ** string is suitable to indicating that the content-type is 
		 ** infact a header name
		 **/
		end_char = *(data +plen);
		switch (end_char){
			case ':':
			case ' ':
			case '\t':
			case '\0':
				/** Valid terminating characters found **/
				break;
			default:
				/** Otherwise, return 0 **/
				return 0;
		} /** switch end_char **/
	} /** if-else **/

	return 1;
}



/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_contenttype_linear
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_contenttype_linear_EXPERIMENT( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	char *chv = header_value;
	char *chn = header_name;
	int boundary_found = 0;
	//	int name_found = 0;
	//	int filename_found = 0;

	/** Absorb whitespace **/
	while (isspace(*chn)) chn++;

	/** Test if the content-type string is valid **/
	if (MIMEH_is_valid_header_prefix(chn, "content-type")==0) return 0;

	/** Now, let's try parse our content-type parameter/value string **/
	while (*chv)
	{
		while (isspace(*chv)) chv++;
		if ((boundary_found==0)&&(MIMEH_is_valid_header_prefix(chv,"boundary")==1))
		{
		}

		//		if (strncasecmp(chv, "boundary"

	}


	return 0;

}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_contenttype
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_contenttype( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{

	int return_value;
	char *p, *q;
	char *hv = strdup( header_value );

	// CONTENT TYPE -------------------------------
	// CONTENT TYPE -------------------------------
	// CONTENT TYPE -------------------------------

	DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: Start",FL);

	p = strstr(header_name,"content-type");
	if (p != NULL)
	{
		DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype: Content-type string found in header-name",FL);

		/** 20041216-1106:PLD: Increase our sanity **/
		hinfo->sanity++;
		PLD_strlower(  header_value );
		PLD_strlower(  header_value );
		q = header_value;

		if (strstr(q,"multipart/appledouble")) hinfo->content_type = _CTYPE_MULTIPART_APPLEDOUBLE;
		else if (strstr(q,"multipart/signed")) hinfo->content_type = _CTYPE_MULTIPART_SIGNED;
		else if (strstr(q,"multipart/related")) hinfo->content_type = _CTYPE_MULTIPART_RELATED;
		else if (strstr(q,"multipart/mixed")) hinfo->content_type = _CTYPE_MULTIPART_MIXED;
		else if (strstr(q,"multipart/alternative")) hinfo->content_type = _CTYPE_MULTIPART_ALTERNATIVE;
		else if (strstr(q,"multipart/report")) hinfo->content_type = _CTYPE_MULTIPART_REPORT;
		else if (strstr(q,"multipart/")) hinfo->content_type = _CTYPE_MULTIPART;
		else if (strstr(q,"text/calendar")) hinfo->content_type = _CTYPE_TEXT_CALENDAR;
		else if (strstr(q,"text/plain")) hinfo->content_type = _CTYPE_TEXT_PLAIN;
		else if (strstr(q,"text/html")) hinfo->content_type = _CTYPE_TEXT_HTML;
		else if (strstr(q,"text/")) hinfo->content_type = _CTYPE_TEXT;
		else if (strstr(q,"image/gif")) hinfo->content_type = _CTYPE_IMAGE_GIF;
		else if (strstr(q,"image/jpeg")) hinfo->content_type = _CTYPE_IMAGE_JPEG;
		else if (strstr(q,"image/")) hinfo->content_type = _CTYPE_IMAGE;
		else if (strstr(q,"audio/")) hinfo->content_type = _CTYPE_AUDIO;
		else if (strstr(q,"message/rfc822")) hinfo->content_type = _CTYPE_RFC822;
		else if (strstr(q,"/octet-stream")) hinfo->content_type = _CTYPE_OCTECT;
		else if (strstr(q,"/ms-tnef")) hinfo->content_type = _CTYPE_TNEF;
		else if (strstr(q,"application/applefile")) 
		{
			hinfo->content_type = _CTYPE_APPLICATION_APPLEFILE;
			if ( hinfo->filename[0] == '\0' ) 
			{
				if (strlen(glb.appledouble_filename)>0)
				{
					snprintf(hinfo->filename, sizeof(hinfo->filename), "%s.applemeta", glb.appledouble_filename );
				} else {
					snprintf(hinfo->filename, sizeof(hinfo->filename), "applefile");
				}
			}
		}
		else hinfo->content_type = _CTYPE_UNKNOWN;

		/** Is there an x-mac-type|creator parameter? **/
		if ((strstr(header_value,"x-mac-type="))&&(strstr(header_value,"x-mac-creator=")))
		{
			/** By setting this flag to 1, we are saying that if the
			 ** filename contains a forward slash '/' char, then it's
			 ** to be treated as a normal char, not a directory 
			 ** separator.  However, as we cannot generate a filename
			 ** with that char normally, we'll convert it to something
			 ** else
			 **/
			DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: Located x-mac attachment",FL);
			hinfo->x_mac = 1;
			FNFILTER_set_mac(hinfo->x_mac);
		}


		// Copy the string to our content-type string storage field
		p = header_value;
		if (p != NULL)
		{
			char *c = p;

			// Step 1 - jump over any whitespace
			while ( *c == ' ' || *c == '\t') c++;

			// Step 2 - Copy the string
			PLD_strncpy( hinfo->content_type_string, c, _MIMEH_CONTENT_TYPE_MAX);

			// Step 3 - clean up the string
			c = hinfo->content_type_string;
			while (*c && *c != ' ' && *c != '\t' && *c != '\n' && *c != '\r' && *c != ';') c++;

			// Step 4 - Terminate the string
			*c = '\0';
		}

		// If we have an additional parameter at the end of our content-type, then we 
		//	should search for a name="foobar" sequence.
		//p = strchr( hv, ';' );
		p = strpbrk( hv, ";\t\n\r " );
		if (p != NULL)
		{
			char *param = NULL;
			char *data_end_point = param;

			p++;
			param = strpbrk( p, ";\n\r\t " );
			while ( param != NULL )
			{
				/**
				 **
				 ** The Process of decoding our line....
				 **	. While not end of the line...
				 **		. Remove whitespace
				 **		. test for 'name'
				 **		. test for 'boundary'
				 **		. Move to next char after parameter values
				 ** 
				 ** Go to the next character after the 'token separator' character
				 ** and then proceed to absorb any excess white space.
				 ** Once we've stopped at a new, non-white character, we can begin
				 ** to see if we've got a sensible parameter like name=, filename=
				 ** or boundary=
				 **/
				param++;
				param = MIMEH_absorb_whitespace(param);

				/** 
				 ** If we get to the end of the line, just break out of the token
				 ** parsing while loop
				 **/
				if (*param == '\0') break;

				/**
				 ** Look for name or filename specifications in the headers
				 ** Look for name or filename specifications in the headers
				 ** Look for name or filename specifications in the headers
				 **/

				return_value = MIMEH_parse_header_parameter( hinfo, param, "name", hinfo->name, sizeof(hinfo->name), &data_end_point);
				/** Update param to point where data_end_point is
				 ** this is so when we come around here again due 
				 ** to the while loop, we'll know where to pick up
				 ** the search for more parameters
				 **/
				if (data_end_point > param) param = data_end_point;

				// If we finally had success, then copy the name into filename for hinfo
				if ( return_value == 0 )
				{
					// Move the parameter search point up to where we stopped 
					//		processing the data in the MIMEH_parse_header_parameter() call

					DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: Pushing new filename to stack '%s'",FL, hinfo->name);
					/** Step 1:  Check to see if this filename already
					 ** exists in the stack.  We do this so that we don't
					 ** duplicate entries and also to prevent false 
					 ** bad-header reports. **/
					if (SS_cmp(&(hinfo->ss_names), hinfo->name, strlen(hinfo->name))==NULL) 
					{
						DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: Filtering '%s'",FL, hinfo->name);
						FNFILTER_filter(hinfo->name, _MIMEH_FILENAMELEN_MAX);
						DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: Pushing '%s'",FL, hinfo->name);
						SS_push(&(hinfo->ss_names),hinfo->name,strlen(hinfo->name));
						if (SS_count(&(hinfo->ss_names)) > 1)
						{
							MIMEH_set_defect(hinfo, MIMEH_DEFECT_MULTIPLE_NAMES);
						}

						if ( hinfo->filename[0] == '\0' ) {
							snprintf( hinfo->filename, sizeof(hinfo->filename), "%s", hinfo->name );
						}
					} /* If the file name doesn't already exist in the stack */

				} /* If a filename was located in the headers */




				/** 
				 ** Look for the MIME Boundary specification in the headers 
				 ** Look for the MIME Boundary specification in the headers 
				 ** Look for the MIME Boundary specification in the headers 
				 **/
				return_value = MIMEH_parse_header_parameter(hinfo, param, "boundary", hinfo->boundary, sizeof(hinfo->boundary), &data_end_point);
				DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: Param<=>data_end gap = %d", FL,data_end_point -param);
				DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: param start pos = '%s'",FL, param);
				if (data_end_point > param) param = data_end_point;
				DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: param start pos = '%s'",FL, param);

				if ( return_value == 0 ) {
					// Move the parameter search point up to where we stopped 
					//		processing the data in the MIMEH_parse_header_parameter() call

					//hinfo->boundary_located = 1;
					hinfo->boundary_located++;
					if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: Pushed boundary to stack (%s)",FL, hinfo->boundary);
					BS_push(hinfo->boundary);
					if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: Setting hinfo->boundary_located to %d",FL, hinfo->boundary_located );

					if (hinfo->boundary_located > 1)
					{
						// Register the defect
						MIMEH_set_defect(hinfo, MIMEH_DEFECT_MULTIPLE_BOUNDARIES);

						//Reset the counter back to 1.
						hinfo->boundary_located=1;
					}
				}

				//param = PLD_strtok( &tx, NULL, ";\n\r" );
				// * PLD:20040831-22H15: Added 'if (param != NULL)' prefix to debugging lines
				// * In response to bug #32, submitted by ICL ZA
				if (param != NULL) DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: param start pos = '%s'",FL, param);
				param = strpbrk( param, ";\n\r " ); 
				if (param != NULL) DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: param start pos = '%s'",FL, param);

			} // While
		}
	}

	if (hv != NULL) free(hv);

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttype:DEBUG: end.",FL);

	return 0;

}










/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_contentlocation
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_contentlocation( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	char *p, *q;

	// CONTENT LOCATION -------------------------------
	// CONTENT LOCATION -------------------------------
	// CONTENT LOCATION -------------------------------

	PLD_strlower( header_name );
	p = strstr(header_name,"content-location");
	if (p)
	{
		/** 20041216-1108:PLD: Increase our sanity **/
		hinfo->sanity++;

		if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_parse_contentlocation:DEBUG: Content Location line found - '%s'\n", FL, header_value);


		p = q = header_value;
		while (q)
		{
			q = strpbrk(p, "\\/");
			if (q != NULL) p = q+1;
		}

		if (p)
		{
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_parse_contentlocation:DEBUG: filename = %s\n", FL, p);
			snprintf(hinfo->name, sizeof(hinfo->name),"%s",p);
			snprintf(hinfo->filename, sizeof(hinfo->name),"%s",p);
			FNFILTER_filter(hinfo->filename, _MIMEH_FILENAMELEN_MAX);
			SS_push(&(hinfo->ss_filenames), hinfo->filename, strlen(hinfo->filename));

		}
	}

	return 0;

}














/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_contenttransferencoding
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_contenttransferencoding( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	char *p, *q;
	char c = '\n';

	// CONTENT TRANSFER ENCODING ---------------------
	// CONTENT TRANSFER ENCODING ---------------------
	// CONTENT TRANSFER ENCODING ---------------------


	p = strstr(header_name,"content-transfer-encoding");
	if (p)
	{
		/** 20041216-1107:PLD: Increase our sanity **/
		hinfo->sanity++;

		q = strpbrk(header_value,"\n\r;");
		if (q != NULL)
		{
			c = *q;
			*q = '\0';
		}

		p = header_value;

		PLD_strlower( p );

		if (strstr(p,"base64"))
		{
			hinfo->content_transfer_encoding = _CTRANS_ENCODING_B64;
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttransferencoding: Encoding set to BASE64", FL);
		}
		else if (strstr(p,"7bit"))
		{
			hinfo->content_transfer_encoding = _CTRANS_ENCODING_7BIT;
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttransferencoding: Encoding set to 7-BIT ", FL);
		}
		else if (strstr(p,"8bit"))
		{
			hinfo->content_transfer_encoding = _CTRANS_ENCODING_8BIT;
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttransferencoding: Encoding set to 8-BIT", FL);
		}
		else if (strstr(p,"quoted-printable"))
		{
			hinfo->content_transfer_encoding = _CTRANS_ENCODING_QP;
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttransferencoding: Encoding set to Quoted-Printable", FL);
		}
		else if (strstr(p,"binary"))
		{
			hinfo->content_transfer_encoding = _CTRANS_ENCODING_BINARY;
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttransferencoding: Encoding set to Binary", FL);
		}
		else if (
				(strstr(p,"uu"))
				||(strstr(p,"x-u"))
				||(strcmp(p,"u") == 0)
				)
		{
			hinfo->content_transfer_encoding = _CTRANS_ENCODING_UUENCODE;
			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contenttransferencoding: Encoding set to UUENCODE", FL);
		}
		else hinfo->content_transfer_encoding = _CTRANS_ENCODING_RAW;


		// Copy the string to our content-transfer string storage field
		p = header_value;
		if (p != NULL)
		{
			char *cp = p;

			// Step 1 - jump over any whitespace
			while ( *cp == ' ' || *cp == '\t') cp++;

			// Step 2 - Copy the string
			PLD_strncpy( hinfo->content_transfer_encoding_string, cp, _MIMEH_CONTENT_TRANSFER_ENCODING_MAX);

			// Step 3 - clean up the string
			cp = hinfo->content_transfer_encoding_string;
			while (*cp && *cp != ' ' && *cp != '\t' && *cp != '\n' && *cp != '\r' && *cp != ';') cp++;

			// Step 4 - Terminate the string
			*cp = '\0';
		}

		// Set the character which we changed to a \0 back to its original form so that
		//		we don't cause problems from tainted data for any further parsing calls
		//		which use the data.
		if (q != NULL) *q = c;
	}

	return 0;

}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_contentdisposition
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_contentdisposition( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	char  *p;
	char *hv = strdup(header_value);

	// CONTENT DISPOSITION ------------------------------
	// CONTENT DISPOSITION ------------------------------
	// CONTENT DISPOSITION ------------------------------

	//LOGGER_log("%s:%d:DEBUG: Headers='%s'",FL,header_value);
	p = strstr(header_name,"content-disposition");
	if (p != NULL)
	{
		/** 20041216-1107:PLD: Increase our sanity **/
		hinfo->sanity++;

		// Change p to now point to the header VALUE, p no longer
		//		points to the content-disposition start!

		p = header_value;
		PLD_strlower( header_value );

		// Here we just check to find out what type of disposition we have.
		if (strstr(p,"inline")) 
		{
			hinfo->content_disposition = _CDISPOSITION_INLINE;
		} 
		else if (strstr(p,"form-data"))
		{
			hinfo->content_disposition = _CDISPOSITION_FORMDATA;
		}
		else if (strstr(p,"attachment")) 
		{
			hinfo->content_disposition = _CDISPOSITION_ATTACHMENT;
		}
		else
		{
			hinfo->content_disposition = _CDISPOSITION_UNKNOWN;
		}

		// Copy the string to our content-transfer string storage field
		if (p != NULL)
		{
			char *q = p;

			// Step 1 - jump over any whitespace
			while ( *q == ' ' || *q == '\t') q++;

			// Step 2 - Copy the string
			PLD_strncpy( hinfo->content_disposition_string, q, _MIMEH_CONTENT_DISPOSITION_MAX);

			// Step 3 - clean up the string
			q = hinfo->content_disposition_string;
			while (*q && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r' && *q != ';') q++;

			// Step 4 - Terminate the string
			*q = '\0';
		}

		DMIMEH LOGGER_log("%s:%d:MIMEH_parse_contentdisposition:DEBUG: Disposition string = '%s'",FL, hv);

		// Commence to decode the disposition string into its components.
		p = strpbrk( hv, ";\t\n\r " );
		if (p != NULL)
		{
			//			struct PLD_strtok tx;
			char *param;

			hinfo->name[0]='\0';

			p++;
			param = p;
			while ( param != NULL )
			{
				int parse_result;
				char *data_end_point;

				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contentdisposition:DEBUG: Parsing '%s'",FL,param);

				// Seek out possible 'filename' parameters 

				parse_result = MIMEH_parse_header_parameter(hinfo, param, "filename", hinfo->name, sizeof(hinfo->name), &data_end_point);
				if (data_end_point > param) param = data_end_point;
				if (parse_result == 0) {
					FNFILTER_filter(hinfo->name, _MIMEH_FILENAMELEN_MAX);
					SS_push(&(hinfo->ss_filenames), hinfo->name, strlen(hinfo->name));
					if (SS_count(&(hinfo->ss_filenames)) > 1)
					{
						MIMEH_set_defect(hinfo,MIMEH_DEFECT_MULTIPLE_FILENAMES);
					}
				}

				param = strpbrk( param , ";\n\r\t " );
				if (param) param++;

				//param = PLD_strtok( &tx, NULL, ";\n\r\t " );
			} // While

			if ( hinfo->filename[0] == '\0' ) 
			{
				snprintf( hinfo->filename, sizeof(hinfo->filename), "%s", hinfo->name );
			}

			// Handle situations where we'll need the filename for the future.
			if ( hinfo->content_type == _CTYPE_MULTIPART_APPLEDOUBLE )
			{
				snprintf( glb.appledouble_filename, sizeof(glb.appledouble_filename), "%s", hinfo->filename );	
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_contentdisposition:DEBUG: Setting appledouble filename to: '%s'",FL,glb.appledouble_filename);
			}

		} // If the header-value contained ;'s ( indicating parameters )

	} // If the header-name actually contained 'content-disposition'

	if (hv != NULL) free(hv);

	return 0;
}




/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_subject
  Returns Type	: int
  ----Parameter List
  1. char *header_name,  contains the full headers
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_generic( char *header_name, char *header_value, struct MIMEH_header_info *hinfo, char *tokenstr, char *buffer, size_t bsize )
{
	int compare_result = 0;
	int tlen;

	DMIMEH LOGGER_log("%s:%d:MIMEH_parse_generic:DEBUG: Searching for %s in %s",FL,tokenstr,header_name);
	/** Sanity check the parameters **/
	if (hinfo == NULL) return -1;
	if (tokenstr == NULL) return -1;
	if (header_name == NULL) return -1;
	if (header_value == NULL) return -1;
	if (buffer == NULL) return -1;
	if (bsize < 1) return -1;

	tlen = strlen(tokenstr);
	compare_result = strncmp( header_name, tokenstr, tlen );
	if (compare_result == 0)
	{

		switch (*(header_name +tlen)) {
			case ':':
			case ' ':
			case '\t':
			case '\0':
				DMIMEH LOGGER_log("%s:%d:MIMEH_parse_generic:DEBUG: Located! Sanity up +1",FL);
				snprintf( buffer, bsize, "%s", header_value );
				hinfo->sanity++;
				break;
		}

	}

	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_subject
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_subject( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	int result = 0;
	result = MIMEH_parse_generic( header_name, header_value, hinfo, "subject", hinfo->subject, sizeof(hinfo->subject)  );
	snprintf(glb.subject, sizeof(glb.subject),"%s", hinfo->subject);

	return result;
}




/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_date
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_date( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	return MIMEH_parse_generic( header_name, header_value, hinfo, "date", hinfo->date, sizeof(hinfo->date) );
}



/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_from
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_from( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	return MIMEH_parse_generic( header_name, header_value, hinfo, "from", hinfo->from, sizeof(hinfo->from) );
}
/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_to
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_to( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	return MIMEH_parse_generic( header_name, header_value, hinfo, "to", hinfo->to, sizeof(hinfo->to) );
}
/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_messageid
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_messageid( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	return MIMEH_parse_generic( header_name, header_value, hinfo, "message-id", hinfo->messageid, sizeof(hinfo->messageid) );
}
/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_received
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_received( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	return MIMEH_parse_generic( header_name, header_value, hinfo, "received", hinfo->received, sizeof(hinfo->received) );
}

/*-----------------------------------------------------------------\
  Date Code:	: 20081216-210524
  Function Name	: MIMEH_parse_charset
  Returns Type	: int
  ----Parameter List
  1. char *header_name, 
  2.  char *header_value, 
  3.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_charset( char *header_name, char *header_value, struct MIMEH_header_info *hinfo )
{
	int result = 0;

	result = MIMEH_parse_generic( header_name, header_value, hinfo, "charset", hinfo->charset, sizeof(hinfo->charset) );
	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_charset:DEBUG: Charset value = '%s'", FL, hinfo->charset);

	return result;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_process_headers
  Returns Type	: int
  ----Parameter List
  1. struct MIMEH_header_info *hinfo, 
  2.  char *headers , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_headers_process( struct MIMEH_header_info *hinfo, char *headers )
{
	/** scan through our headers string looking for information that is
	 ** valid **/
	char *safeh, *h, *safehl;
	char *current_header_position;
	int headerlength;

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: Start [hinfo=%p]\n",FL, hinfo);

	safeh = h = headers;

	/** Duplicate the headers for processing - this way we don't 'taint' the
	 ** original headers during our searching / altering. **/

	headerlength = strlen(h);
	safehl = malloc(sizeof(char) *(headerlength+1));
	PLD_strncpy(safehl, h, headerlength+1);

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_parse_headers:DEBUG: Header length = %d\n", FL,headerlength);

	MIMEH_strip_comments(h);

	current_header_position = h;

	// Searching through the headers, we seek out header 'name:value;value;value' sets,
	//		Each set is then cleaned up, seperated and parsed.

	while ((current_header_position != NULL)&&( current_header_position <= (h +headerlength) ))
	{
		char *header_name, *header_value;
		char *header_name_end_position;
		char *header_value_end_position;

		DMIMEH LOGGER_log("%s:%d:MIMEH_headers_process:DEBUG: Processing '%s'",FL,current_header_position);

		/** Tokenise for the header 'name', ie, content-type, subject etc **/
		header_name = current_header_position;
		header_name_end_position = strpbrk( header_name, ":\t " );
		if (header_name_end_position == NULL)
		{
			// We couldn't find a terminating :, so, instead we try to find
			//	the first whitespace
			//
			// PLD:DEV:11/08/2004-15H27
			//	Issue: c030804-006a
			//
			// NOTE: this may activate on the true-blank lines, hence why we
			//		dump the source string, just for confirmation

			DMIMEH LOGGER_log("%s:%d:MIMEH_headers_process:DEBUG: Could not locate ':' separator, using whitespace (source='%s')",FL,header_name);
			header_name_end_position = strpbrk( header_name, "\t " );
			if (header_name_end_position == NULL)
			{
				DMIMEH LOGGER_log("%s:%d:MIMEH_headers_process:DEBUG: Cannot find a header name:value pair in '%s'",FL, header_name);
			}
		}

		// Seek forward from the start of our header, looking for the first occurance
		//		of the line end (implying the end of the current header name:value,
		//		we can do this because we know that when the headers were read in, we
		//		have already unfolded them, such that there should only be one header:value
		//		pairing per 'line'.

		current_header_position = strpbrk( current_header_position, "\n\r");
		if ( current_header_position == NULL )
		{
			// Theoretically, this should not happen, as headers are always
			//		terminated with a \n\r\n\r finishing byte sequence, thus
			//		if this _does_ happen, then we will simply jump out of the 
			//		current iteration and let the loop try find another pairing
			//
			// There probably should be a logging entry here to indicate that
			//		"something strange happened"

			continue;
		} else {

			// Shuffle our CHP (current-header-position) pointer along the header
			//		data until it is no longer pointing to a \r or \n, this is so
			//		that when the next iteration of this loop comes around, it'll
			//		immediately be in the right place for starting the next parse

			while (( *current_header_position == '\n') ||( *current_header_position == '\r' )) current_header_position++;
		}

		if (( header_name_end_position == NULL )||( header_name_end_position > current_header_position))
		{ 
			// Some headers can contain various levels of non name/value pairings,
			//		while their presence could be debatable in terms of RFC validity
			//		we will 'ignore' them rather than throwing up our arms.  This 
			//		ensures that we are not made to break over spurilous data.

			if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: This line contains no header:value pair (%s)", FL, current_header_position);
			continue;

		} else {

			// Get the header-value string and prepare to
			//		parse the data through our various parsing 
			//		functions.

			header_value = header_name_end_position +1;
			header_value_end_position = strpbrk( header_value, "\n\r" );
			if ( header_value_end_position != NULL )
			{
				*header_name_end_position = '\0';
				*header_value_end_position = '\0';
				if (MIMEH_DNORMAL)
				{
					LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: Header Name ='%s'", FL, header_name );
					LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: Header Value='%s'", FL, header_value );
				}

				// To make parsing simpler, convert our
				//		header name to lowercase, that way
				//		we also reduce the CPU requirements for 
				//		searching because pre-lowering the header-name
				//		occurs once, but string testing against it
				//		occurs multiple times ( at least once per parsing

				PLD_strlower( header_name );
				MIMEH_parse_subject( header_name, header_value, hinfo );
				MIMEH_parse_contenttype( header_name, header_value, hinfo );
				MIMEH_parse_contenttransferencoding( header_name, header_value, hinfo );
				MIMEH_parse_contentdisposition( header_name, header_value, hinfo );
				/** These items aren't really -imperative- to have, but they do
				 ** help with the sanity checking **/
				MIMEH_parse_date( header_name, header_value, hinfo );
				MIMEH_parse_from( header_name, header_value, hinfo );
				MIMEH_parse_to( header_name, header_value, hinfo );
				MIMEH_parse_messageid( header_name, header_value, hinfo );
				MIMEH_parse_received( header_name, header_value, hinfo );
				MIMEH_parse_charset( header_name, header_value, hinfo );


				if (hinfo->filename[0] == '\0')
				{
					MIMEH_parse_contentlocation( header_name, header_value, hinfo );
				}

			} else {
				if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_headerss:DEBUG: Header value end position is NULL",FL);
			}
		}



	} // while

	// Final analysis on our headers:
	if ( hinfo->content_type == _CTYPE_MULTIPART_APPLEDOUBLE )
	{
		char tmp[128];
		snprintf( tmp, sizeof(tmp), "mac-%s", hinfo->filename );
		snprintf( hinfo->filename, sizeof(hinfo->filename), "%s", tmp );
		snprintf( hinfo->name, sizeof(hinfo->name), "%s", tmp );
	}

	// PLD:20031205
	// Act like Outlook *God forbid!* and if there's a octect-stream 
	//	content-type, but the encoding is still null/empty, then 
	//	change the content-encoding to be RAW

	if ( hinfo->content_type == _CTYPE_OCTECT )
	{
		if ((hinfo->content_transfer_encoding == _CTRANS_ENCODING_UNSPECIFIED)
				|| (hinfo->content_transfer_encoding == _CTRANS_ENCODING_UNKNOWN)
				|| (strlen(hinfo->content_transfer_encoding_string) < 1)
			)	
		{
			//LOGGER_log("%s:%d:DEBUG: Encoding pair was octet but no encoding, filename=%s\n",FL,hinfo->filename);
			hinfo->content_transfer_encoding = _CTRANS_ENCODING_RAW;
		}
	}


	if (safehl)
	{
		free(safehl);
	}
	else LOGGER_log("%s:%d:MIME_parse_headers:WARNING: Unable to free HEADERS allocated memory\n", FL);

	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: END [hinfo=%p]\n", FL, hinfo);

	return 0;
}


/*-----------------------------------------------------------------\
  Date Code:	: 20081124-184934
  Function Name	: MIMEH_headers_clearcount
  Returns Type	: int
  ----Parameter List
  1. struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_headers_clearcount( struct MIMEH_header_info *hinfo ) {

	hinfo->crlf_count = 0;
	hinfo->lf_count = 0;
	hinfo->crcr_count = 0;
	return 0;

}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_get_headers
  Returns Type	: int
  ----Parameter List
  1. struct MIMEH_header_info *hinfo, 
  2.  FFGET_FILE *f , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_headers_get( struct MIMEH_header_info *hinfo, FFGET_FILE *f )
{
	int result = 0;

	// Setup some basic defaults 
	hinfo->filename[0] = '\0';
	hinfo->name[0] = '\0';
	hinfo->content_type = _CTYPE_UNKNOWN;
	hinfo->subject[0] = '\0';
	hinfo->charset[0] = '\0';

	// 20040116-1234:PLD - added to appease valgrind
	hinfo->content_disposition = 0;
	hinfo->content_transfer_encoding = 0;
	hinfo->boundary_located = 0;

	hinfo->crlf_count=0;
	hinfo->crcr_count=0;
	hinfo->lf_count=0;

	if (f->linebreak == (FFGET_LINEBREAK_CR|FFGET_LINEBREAK_LF)) {
		if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_headers_get:DEBUG: Setting to CRLF based on ffget value of %d", FL, f->linebreak);
		snprintf(hinfo->delimeter,sizeof(hinfo->delimeter),"\r\n");
		hinfo->crlf_count = 1;
	} else if (f->linebreak == FFGET_LINEBREAK_LF) {
		if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_headers_get:DEBUG: Setting to LF based on ffget value of %d", FL, f->linebreak);
		snprintf(hinfo->delimeter,sizeof(hinfo->delimeter),"\n");
		hinfo->lf_count = 1;
	} else {
		if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_headers_get:DEBUG: ffget value of %d offered us no guide for the delimeter", FL, f->linebreak);
	}



	// Initialise header defects array.
	hinfo->header_defect_count = 0;
	memset(hinfo->defects, 0, _MIMEH_DEFECT_ARRAY_SIZE);

	snprintf( hinfo->content_type_string, _MIMEH_CONTENT_TYPE_MAX , "text/plain" ); 


	// Read from the file, the headers we need
	FFGET_set_watch_SDL(1);
	result = MIMEH_read_headers(hinfo, f);
	FFGET_set_watch_SDL(0);

	if (hinfo->lf_count > hinfo->crlf_count) { 
		snprintf(hinfo->delimeter,sizeof(hinfo->delimeter),"\n");
	}

	// If we ran out of input whilst looking at headers, then, we basically
	// flag this, free up the headers, and return.
	if (result == -1)
	{
		if (glb.headerline) free(glb.headerline);
		return result;
	}

	// If we came back with an OKAY result, but there's nothing in the
	//	headers, then flag off an error
	if (glb.headerline == NULL)
	{
		if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIME_parse_headers:DEBUG: null headerline\n", FL);
		return 1;
	}

	return result;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_headers_cleanup
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
int MIMEH_headers_cleanup( void )
{
	if (glb.headerline != NULL)
	{
		free(glb.headerline);
		glb.headerline = NULL;
	}

	if (glb.headerline_original != NULL)
	{ 
		free(glb.headerline_original);
		glb.headerline_original = NULL;
	} 

	return 0;
}



/*-----------------------------------------------------------------\
  Function Name	: MIMEH_parse_headers
  Returns Type	: int
  ----Parameter List
  1. FFGET_FILE *f, 
  2.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_parse_headers( FFGET_FILE *f, struct MIMEH_header_info *hinfo )
{
	int result = 0;
	DMIMEH LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: Start [F=%p, hinfo=%p]\n", FL, f, hinfo);

	/** 20041216-1100:PLD: Set the header sanity to zero **/
	if ( result == 0 ) hinfo->sanity = 0; 

	/** Proceed to read, process and finish headers **/
	DMIMEH LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: Getting headers",FL);
	if ( result == 0 ) result = MIMEH_headers_get( hinfo, f );
	DMIMEH LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: Processing headers",FL);
	if ( result == 0 ) result = MIMEH_headers_process( hinfo, glb.headerline );
	DMIMEH LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: cleanup of headers",FL);
	if ( result == 0 ) result = MIMEH_headers_cleanup(); 
	if (MIMEH_DNORMAL) LOGGER_log("%s:%d:MIMEH_parse_headers:DEBUG: END [F=%p, hinfo=%p, sanity=%d]\n", FL, f, hinfo, hinfo->sanity);

	return result;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_dump_defects
  Returns Type	: int
  ----Parameter List
  1. struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
Displays a list of the located defects

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_dump_defects( struct MIMEH_header_info *hinfo )
{
	int i;

	MIMEH_defect_description_array[MIMEH_DEFECT_MISSING_SEPARATORS] = strdup("Missing separators");
	MIMEH_defect_description_array[MIMEH_DEFECT_MULTIPLE_FIELD_OCCURANCE] = strdup("Multiple field occurance");
	MIMEH_defect_description_array[MIMEH_DEFECT_UNBALANCED_BOUNDARY_QUOTE] = strdup("Unbalanced boundary quote");
	MIMEH_defect_description_array[MIMEH_DEFECT_MULTIPLE_BOUNDARIES] = strdup("Multiple boundries");
	MIMEH_defect_description_array[MIMEH_DEFECT_MULTIPLE_COLON_SEPARATORS] = strdup("Multiple colon separators");
	MIMEH_defect_description_array[MIMEH_DEFECT_MULTIPLE_EQUALS_SEPARATORS] = strdup("Multiple equals separators");
	MIMEH_defect_description_array[MIMEH_DEFECT_UNBALANCED_QUOTES] = strdup("Unbalanced quotes");
	MIMEH_defect_description_array[MIMEH_DEFECT_MULTIPLE_QUOTES] = strdup("Multiple quotes");
	MIMEH_defect_description_array[MIMEH_DEFECT_MULTIPLE_NAMES] = strdup("Multiple names");
	MIMEH_defect_description_array[MIMEH_DEFECT_MULTIPLE_FILENAMES] = strdup("Multiple filenames");

	for (i = 0; i < _MIMEH_DEFECT_ARRAY_SIZE; i++)
	{
		if (hinfo->defects[i] > 0)
		{
			LOGGER_log("Header Defect: %s: %d",MIMEH_defect_description_array[i],hinfo->defects[i]);
		}

	}

	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: MIMEH_get_defect_count
  Returns Type	: int
  ----Parameter List
  1. struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_get_defect_count( struct MIMEH_header_info *hinfo )
{

	return hinfo->header_defect_count;
}


/*-----------------------------------------------------------------\
  Date Code:	: 20090406-114302
  Function Name	: MIMEH_read_primary_headers
  Returns Type	: int
  ----Parameter List
  1. char *fname, 
  2.  struct MIMEH_header_info *hinfo , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIMEH_read_primary_headers( char *fname, struct MIMEH_header_info *hinfo )
{
	FFGET_FILE F;
	FILE *f;

	f = fopen(fname,"r");
	if (!f) { 
		LOGGER_log("%s:%d:MIMEH_read_primary_headers:ERROR: Cannot open mailpack '%s' (%s)", FL, fname, strerror(errno));
		return 0;
	}
	FFGET_setstream(&F,f);

	MIMEH_parse_headers(&F, hinfo);
	fclose(f);

	return 0;
}

//----------------------END

