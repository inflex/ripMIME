/*------------------------------------------------------------------------
Module:        /extra/development/xamime/xamime_working/ripmime/uuencode.c
Author:        Paul L Daniels
Project:       Xamime:ripMIME
State:         Stable
Creation Date:
Description:   uuencode is a collection of functions to aid the decoding / encoding of UUENCODED data files.
This module is primarily intended to be used with ripMIME rather than for 'stand alone' use.
The biggest issue is that the interfaces to the decoding functions are too specific at this point for a more generic use.
------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>

#include "logger.h"
#include "pldstr.h"
#include "ffget.h"
#include "filename-filters.h"

#include "uuencode.h"



#ifndef FL
#define FL __FILE__,__LINE__
#endif

#define UUENCODE_DEBUG_PEDANTIC 10
#define UUENCODE_DEBUG_NORMAL 1

#define UUENCODE_STRLEN_MAX 1024

// Debug precodes
#define UUENCODE_DPEDANTIC ((glb.debug >= UUENCODE_DEBUG_PEDANTIC))
#define UUENCODE_DNORMAL   ((glb.debug >= UUENCODE_DEBUG_NORMAL  ))
#define UUENCODE_VERBOSE  	((glb.verbosity > 0 ))

#define UUENCODE_WRITE_BUFFER_SIZE 4096
#define UUENCODE_WRITE_BUFFER_LIMIT 4000


static unsigned char uudec[256]={
	32,   33,   34,   35,   36,   37,   38,   39,   40,   41,   42,   43,   44,   45,   46,   47,\
		48,   49,   50,   51,   52,   53,   54,   55,   56,   57,   58,   59,   60,   61,   62,   63,\
		0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,\
		16,   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,   31,\
		32,   33,   34,   35,   36,   37,   38,   39,   40,   41,   42,   43,   44,   45,   46,   47,\
		48,   49,   50,   51,   52,   53,   54,   55,   56,   57,   58,   59,   60,   61,   62,   63,\
		0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,\
		16,   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,   31,\
		32,   33,   34,   35,   36,   37,   38,   39,   40,   41,   42,   43,   44,   45,   46,   47,\
		48,   49,   50,   51,   52,   53,   54,   55,   56,   57,   58,   59,   60,   61,   62,   63,\
		0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,\
		16,   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,   31,\
		32,   33,   34,   35,   36,   37,   38,   39,   40,   41,   42,   43,   44,   45,   46,   47,\
		48,   49,   50,   51,   52,   53,   54,   55,   56,   57,   58,   59,   60,   61,   62,   63,\
		0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,\
		16,   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,   31 \
};

struct UUENCODE_globals {
	int debug;
	int verbosity;
	int verbosity_contenttype;
	int decode;
	int doubleCR_mode;
	int (*filename_decoded_report)(char *, char *);	// Pointer to our filename reporting function
};

static struct UUENCODE_globals glb;

int uuencode_error;   // this contains the error code for parents to check if they receive a -1


/*-----------------------------------------------------------------\
  Function Name	: UUENCODE_init
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
int UUENCODE_init( void )
{
	glb.debug = 0;
	glb.verbosity = 0;
	glb.verbosity_contenttype = 0;
	glb.decode = 1;
	glb.doubleCR_mode = 0;

	return 0;
}


/*-----------------------------------------------------------------\
  Function Name	: UUENCODE_set_debug
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
int UUENCODE_set_debug( int level )
{
	glb.debug = level;
	return glb.debug;
}

/*-----------------------------------------------------------------\
  Function Name	: UUENCODE_set_verbosity
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
int UUENCODE_set_verbosity( int level )
{
	glb.verbosity = level;
	return glb.verbosity;
}

/*-----------------------------------------------------------------\
  Function Name	: UUENCODE_set_verbosity_contenttype
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
int UUENCODE_set_verbosity_contenttype( int level )
{
	glb.verbosity_contenttype = level;
	return glb.verbosity_contenttype;
}

/*-----------------------------------------------------------------\
  Function Name	: UUENCODE_set_decode
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
int UUENCODE_set_decode( int level )
{
	glb.decode = level;
	return glb.decode;
}

/*-----------------------------------------------------------------\
  Function Name	: UUENCODE_set_doubleCR_mode
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
int UUENCODE_set_doubleCR_mode( int level )
{
	glb.doubleCR_mode = level;
	return glb.doubleCR_mode;
}

/*-----------------------------------------------------------------\
  Function Name	: UUENCODE_set_filename_report_fn
  Returns Type	: int
  ----Parameter List
  1. int (*ptr_to_fn)(char *, 
  2.  char *) , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int UUENCODE_set_filename_report_fn( int (*ptr_to_fn)(char *, char *) )
{
	glb.filename_decoded_report = ptr_to_fn;

	return 0;
}

/*------------------------------------------------------------------------
Procedure:     UUENCODE_is_uuencode_header ID:1
Purpose:       Tries to determine if the line handed to it is a UUencode header.
Input:
Output:        0 = No
1 = Yes
Errors:
------------------------------------------------------------------------*/
int UUENCODE_is_uuencode_header( char *line )
{
	struct PLD_strtok tx;
	char buf[UUENCODE_STRLEN_MAX];
	char *bp,*fp;
	int result = 0;

	// If we're not supposed to be decoding UUencoded files, then return 0
	if (glb.decode == 0) return 0;

	snprintf( buf, sizeof(buf), "%s", line );

	bp = buf;

	// If you're wondering why we dont check for "begin ",it's because we dont know
	// if begin is followed by a \t or ' ' or some other white space
	// Also, check to see that we don't have a VCARD ( look to see if there's a trailing
	//		colon after the BEGIN
	//
	// 2003-08-12:PLD:Added *(bp+6) test as recommended by Bernard Fischer to ensure there's more
	//						data after the begin	

	if ((bp)&&(strncasecmp(bp,"begin",5)==0)&&(*(bp+5)!=':')&&(isspace((int)*(bp+5)))&&(*(bp+6)))
	{
		fp = NULL;
		bp = PLD_strtok(&tx, buf, " \n\r\t"); // Get the begin

		if (bp) fp = PLD_strtok(&tx, NULL, " \n\r\t"); // Get the file-permissions

		if (fp)
		{
			if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_is_uuencode_header:DEBUG: PERMISSIONS = %s\n", FL, fp);

			if ((atoi(fp) == 0)||(atoi(fp) > 777))   // Maximum is 777, because R+W+X = 7
			{
				result = 0;
			}
			else result = 1;

		}
		else {
			if (UUENCODE_DNORMAL)	LOGGER_log("%s:%d:UUENCODE_is_uuencode_header:WARNING: Cannot read permissions for UUENCODED data file (%s)\n", FL, line);
		}
	}


	return result;
}






/*------------------------------------------------------------------------
Procedure:     UUENCODE_is_file_uuenc ID:1
Purpose:       Tries to determine if a given file is UUEncoded, or at
least contains a UUENCODED file to it.
This should only be run -after- we've checked with
is_file_mime() because if the file is MIME, then it'll be able
to detect UUencoding within the normal decoding routines.
Input:         filename to test
Output:        0 = not uuencoded
1 = _probably_ uuencoded.
Errors:
------------------------------------------------------------------------*/
int UUENCODE_is_file_uuencoded( char *fname )
{
	int result = 0;
	int linecount = 0;
	int limit=20;
	char line[ UUENCODE_STRLEN_MAX ];
	FILE *f;

	f = fopen(fname,"r");
	if (!f)
	{
		LOGGER_log("%s:%d:UUENCODE_is_file_uuencoded:ERROR: cannot open file '%s' for reading (%s)", FL, fname,strerror(errno));
		uuencode_error = UUENCODE_STATUS_CANNOT_OPEN_FILE;
		return -1;
	}

	while ((linecount < limit)&&(fgets(line, sizeof(line), f)))
	{
		if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_is_file_uuencoded:DEBUG: Testing line '%s'\n", FL, line);
		if (UUENCODE_is_uuencode_header( line ))
		{
			result = 1;
			break;
		}
		linecount++;
	}

	fclose(f);

	return result;
}


/*-----------------------------------------------------------------\
  Function Name	: UUENCODE_decode_uu
  Returns Type	: int
  ----Parameter List
  1. FFGET_FILE *f,				Source Data Stream
  2.  char *unpackdir,			Directory to prefix to our write output
  3.  char *input_filename,	The fully pathed input filename, containing UU data
  4.  char *out_filename,		Pointer to a buffer where we will write the filename of the UU data
  5.  int out_filename_size, out_filename buffers size
  6.  int decode_whole_file, 0 == only first segment, >0 == all 
  7.  int keep ,					Keep the files we create, don't delete 
  ------------------
  Exit Codes	:		Returns the number of attachments decoded in the data
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int UUENCODE_decode_uu( FFGET_FILE *f, char *unpackdir, char *input_filename, char *out_filename, int out_filename_size, int decode_whole_file, int keep )
{
	int filename_found = 0;
	char buf[ UUENCODE_STRLEN_MAX ];
	char *bp = buf, *fn, *fp;
	int n, i, expected;
	char fullpath[ UUENCODE_STRLEN_MAX ]="";
	struct PLD_strtok tx;
	unsigned char *writebuffer = NULL;
	unsigned char *wbpos;
	int wbcount = 0;
	int loop = 0;
	int buflen = 0;
	int filecount = 0;
	FFGET_FILE ffinf;	// Local static FFGET struct used if *f is  NULL
	FFGET_FILE *finf;	// Points to either *f or &ffinf
	FILE *outf;
	int outfo =0; 			// set if outfile was opened.
	FILE *inf = NULL;
	int output_filename_supplied = 0;
	int start_found = 0;

	if ((out_filename != NULL))
	{
		if ((out_filename[0] != '\0'))
		{
			output_filename_supplied = 1;
		}
	}

	bp = buf;

	if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Starting.(input=%s,output=%s)\n", FL, input_filename,out_filename );


	// If no FFGET_FILE param is passed to us directly, then we must create out own.

	if (!f)
	{
		if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: NULL FFGET source stream given to us, create our own.\n",FL);

		// Setup the full source-data file path.
		snprintf(fullpath, sizeof(fullpath),"%s", input_filename );

		if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Full INPUT file path set as '%s'\n", FL, fullpath);

		inf = fopen(fullpath,"r");
		if (!inf)
		{
			LOGGER_log("%s:%d:UUENCODE_decode_uu:ERROR: Cannot open file '%s' for reading (%s)", FL, fullpath, strerror(errno));
			uuencode_error = UUENCODE_STATUS_CANNOT_OPEN_FILE;
			return -1;
		}

		FFGET_setstream(&ffinf, inf);
		FFGET_set_watch_SDL( glb.doubleCR_mode );
		finf = &ffinf;

		if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Creation done. [FFGET-FILE=%p, FILE=%p]\n", FL, finf, inf);
	} else {
		if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: File handle already exists to read from, using",FL);	
		finf = f;
	}

	writebuffer = malloc( UUENCODE_WRITE_BUFFER_SIZE *sizeof(unsigned char));
	if (!writebuffer)
	{
		LOGGER_log("%s:%d:UUENCODE_decode_uu:ERROR: cannot allocate 100K of memory for the write buffer",FL);
		uuencode_error = UUENCODE_STATUS_CANNOT_ALLOCATE_MEMORY;
		return -1;
	}
	else {
		wbpos = writebuffer;
		wbcount = 0;
	}


	if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Beginning.(%s)\n",FL,fullpath);


	while (!FFGET_feof(finf))
	{
		filename_found = 0;


		// First lets locate the BEGIN line of this UUDECODE file

		//		if (output_filename_supplied == 0)

		if (1) /** 20041105-23H00:PLD: Stepan Kasal - UUbegin patch **/
		{

			while (FFGET_fgets(buf, sizeof(buf), finf))
			{

				if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: BUFFER: \n%s\n", FL, buf);

				// Check for the presence of 'BEGIN', but make sure it's not followed by a 
				//		colon ( which indicates a VCARD instead of UUENCODE

				if ((strncasecmp(buf,"begin",5)==0)&&(buf[5] !=':')&&(isspace((int)buf[5])))
				{
					if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Located BEGIN\n",FL);
					// Okay, so the line contains begin at the start, now, lets get the decode details
					fp = fn = NULL;

					bp = PLD_strtok(&tx, buf, " \n\r\t"); // Get the begin

					if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: BEGIN = '%s'\n", FL, bp);
					if (bp) fp = PLD_strtok(&tx, NULL, " \n\r\t"); // Get the file-permissions

					if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Permissions/Name = '%s'\n", FL, fp);
					if (fp) fn = PLD_strtok(&tx, NULL, "\n\r"); // Get the file-name

					if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Name = '%s'\n", FL, fn);

					if (!fn)
					{
						bp = fp;
					}
					else bp = fn;

					if ((!bp)&&(!f))
					{
						LOGGER_log("%s:%d:UUENCODE_decode_uu:WARNING: unable to obtain filename from UUencoded text file header", FL);
						if (writebuffer) free(writebuffer);
						fclose(inf);
						uuencode_error = UUENCODE_STATUS_CANNOT_FIND_FILENAME;
						return -1;
					}

					if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Full path = (%s)\n",FL,bp);

					filename_found = 1;
					break;
				} // If line starts with BEGIN

			} // While more lines in the INPUT file.

		}

		/** 20041105-23H02:PLD: Stepan Kasal Patch **/
		// Filename from header has precedence:
		if (output_filename_supplied != 0)
		{
			filename_found = 1;
			bp = out_filename;
			if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Output filename set to '%s'",FL, bp);
		}




		// If we have a filename, and we have our bp as NON-null, then we shall commence
		//	to decode the UUencoded data from the stream.

		if ((filename_found != 0)&&(bp))
		{
			if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Located filename (%s), now decoding.\n", FL, bp);

			// Clean up the file name
			FNFILTER_filter( bp, out_filename_size );

			// If our filename wasn't supplied via the params, then copy it over here
			if (output_filename_supplied == 0) snprintf( out_filename, out_filename_size, "%s", bp );


			// Create the new output full path

			snprintf(fullpath, sizeof(fullpath), "%s/%s", unpackdir, bp );
			if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Filename = (%s)\n", FL, fullpath);

			outf = fopen(fullpath, "wb");
			if (!outf)
			{
				LOGGER_log("%s:%d:UUENCODE_decode_uu:ERROR: Cannot open file '%s' (%s)", FL, fullpath,strerror(errno));
				if (writebuffer) free(writebuffer);
				uuencode_error = UUENCODE_STATUS_CANNOT_OPEN_FILE;
				return -1;
			}
			else outfo = 1;

			// Allocate the write buffer.  By using the write buffer we gain an additional 10% in performance
			// due to the lack of function call (fwrite) overheads

			// Okay, now we have the UUDECODE data to decode...

			wbcount = 0;
			wbpos = writebuffer;

			while (outf)
			{

				// for each input line
				FFGET_fgets(buf, sizeof(buf), finf);
				if (UUENCODE_DPEDANTIC) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Read line:\n%s",FL,buf);
				if (FFGET_feof(finf) != 0)
				{
					if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:WARNING: Short file (%s)\n",FL, fullpath);
					if (writebuffer != NULL) free(writebuffer);
					uuencode_error = UUENCODE_STATUS_SHORT_FILE;
					return -1;
				}

				// If we've reached the end of the UUencoding

				if (strncasecmp(buf,"end",3)==0)
				{
					if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: End of UUencoding detected\n",FL);
					break;
				}

				if ( !strpbrk(buf,"\r\n") )
				{
					LOGGER_log("%s:%d:UUENCODE_decode_uu:WARNING: Excessive length line\n",FL);
				}

				// The first char of the line indicates how many bytes are to be expected

				n = uudec[(int)*buf];


				// If the line is a -blank- then break out.

				if ((start_found == 0)&&((*buf == '\n')||(*buf == '\r'))) continue;
				else start_found =1 ;

				if ((start_found != 0)&&((n <= 0) || (*buf == '\n'))) break;

				// Calculate expected # of chars and pad if necessary

				expected = ((n+2)/3)<<2;
				buflen = strlen(buf) -1;
				for (i = buflen; i <= expected; i++) buf[i] = ' ';
				bp = &buf[1];

				// Decode input buffer to output file.

				while (n > 0)
				{
					// In order to reduce function call overheads, we've bought the UUDecoding
					// bit shifting routines into the UUDecode main decoding routines. This should
					// save us about 250,000 function calls per Mb.
					// UUENCODE_outdec(bp, outf, n);

					char c[3];
					int m = n;

					c[0] = uudec[(int)*bp] << 2 | uudec[(int)*(bp+1)] >> 4;
					c[1] = uudec[(int)*(bp+1)] << 4 | uudec[(int)*(bp+2)] >> 2;
					c[2] = uudec[(int)*(bp+2)] << 6 | uudec[(int)*(bp+3)];

					if (m > 3) m = 3;

					if ( wbcount >= UUENCODE_WRITE_BUFFER_LIMIT )
					{
						size_t bc;
						bc = fwrite(writebuffer, 1, wbcount, outf);
						wbpos = writebuffer;
						wbcount = 0;
					}

					// Transfer the decoded data to the write buffer.
					// The reason why we use a loop, rather than just a set of if
					// statements is just for code-viewing simplicity.  It's a lot
					// easier to read than some nested chain of if's

					for (loop = 0; loop < m; loop++)
					{
						*wbpos = c[loop];
						wbpos++;
						wbcount++;
					}

					bp += 4;
					n -= 3;

				} // while (n > 0)

			} // While (1)

			if ((outfo)&&(wbcount > 0))
			{
				size_t bc;
				bc = fwrite(writebuffer, 1, wbcount, outf);
			}


			if (outfo) fclose(outf);
			// Call our reporting function, else, if no function is defined, use the default
			//		standard call

			if ((UUENCODE_VERBOSE) && (output_filename_supplied == 0))
			{
				if (glb.filename_decoded_report == NULL)
				{
					LOGGER_log("Decoded: %s\n", fullpath);
				} else {
					glb.filename_decoded_report( fullpath, (glb.verbosity_contenttype>0?"uuencoded":NULL) );
				}
			}

			filecount++;

		} // If valid filename was found for UUdecode

		else
		{
			out_filename[0] = '\0';
			if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: No FILENAME was found in data...\n",FL);
		}

		if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Segment completed\n",FL);

		// If this file was a result of the x-uuencode content encoding, then we need to exit out
		// as we're reading in the -stream-, and we dont want to carry on reading because we'll
		// end up just absorbing email data which we weren't supposed to.

		if ((f)&&( !decode_whole_file )) break;


	} // While !feof(inf)



	if (writebuffer) free(writebuffer);

	if (UUENCODE_DNORMAL) LOGGER_log("%s:%d:UUENCODE_decode_uu:DEBUG: Completed\n",FL);

	if (inf) fclose(inf);


	return filecount;
}






