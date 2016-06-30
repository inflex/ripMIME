/*------------------------------------------------------------------------
Module:        ffget.c
Author:        pldaniels
Project:       ripmime
State:         development
Creation Date: 14/05/2001
Description:   ffget is a small module which will be used to (we hope) speed up the fgetc()
routine by line-buffering up first.

12/11/2002: Corrected input buffer termination with fgets where the last line
of the input file does not terminate with a \n or \r.

27/09/2001:  Added SGI specific compile time changes from char -> short
contributed by Don Lafontaine <lafont02@cn.ca>

------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "logger.h"
#include "ffget.h"


/* GLOBALS */
int ffget_linesize=0;
int FFGET_doubleCR = 0;
int FFGET_SDL_MODE = 0;  // Single Char Delimeter

int FFGET_SDL_WATCH = 0;	// Set if we want to watch for double-CR exploits
int FFGET_ALLOW_NUL = 0;	// Dont Convert \0's to spaces.

int FFGET_debug = 0;

char SDL_MODE_DELIMITS[]="\n\r";
char NORM_MODE_DELIMITS[]="\n";
char *DELIMITERS=SDL_MODE_DELIMITS;

/*------------------------------------------------------------------------
Procedure:     FFGET_set_watch_SDL ID:1
Purpose:       Set/Unset the flag to indicate that we should be watching out
for a double-CR potential exploit mode when decoding files.
Input:         int level: 0 = don't watch, !0 = watch
Output:
Errors:
------------------------------------------------------------------------*/
int FFGET_set_watch_SDL( int level )
{
	FFGET_SDL_WATCH = level;


	return FFGET_SDL_WATCH;
}

/*-----------------------------------------------------------------\
  Function Name	: FFGET_set_allow_nul
  Returns Type	: int
  ----Parameter List
  1. int level , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
This tells the FFGET_raw() if it needs to remove \0's or not
in the data it reads.  This was added so as to ensure that \0
sequences were preserved in form-data from WWW pages.

--------------------------------------------------------------------
Changes:
Added 2004-Aug-09 by PLD.

\------------------------------------------------------------------*/
int FFGET_set_allow_nul(int level )
{
	FFGET_ALLOW_NUL = level;

	return FFGET_ALLOW_NUL;
}

/*------------------------------------------------------------------------
Procedure:     FFGET_set_debug ID:1
Purpose:       Set debugging report/verbosity level
Input:         level
Output:        Returns the level set
Errors:
------------------------------------------------------------------------*/
int FFGET_set_debug( int level )
{
	FFGET_debug = level;

	return FFGET_debug;
}



/*------------------------------------------------------------------------
Procedure:     FFGET_getnewblock ID:1
Purpose:       Reads a new block of data from the input file
Input:         FFGET_FILE record
Output:        Returns number of bytes read
Errors:
------------------------------------------------------------------------*/
int FFGET_getnewblock( FFGET_FILE *f )
{
	int i;
	int bs = 0;
	char *p;

	// We read the maximum of FFGET_BUFFER_MAX -2, because later, when we
	// use fgets(), we may need to read in an /additional/ single byte
	// and if we dont allocate spare room, we may have a buffer overflow

	if (f->FILEEND > 0)
	{
		f->endpoint = f->buffer;
		f->startpoint = f->buffer +1;
		f->FFEOF = 1;
		return 0;

	} else {
		long block_pos;

		block_pos = ftell(f->f); /** Get our current read position so we can use it in FFGET_ftell if required **/

		bs = fread( f->buffer, 1, FFGET_BUFFER_MAX -FFGET_BUFFER_PADDING, f->f );

		if (bs < (FFGET_BUFFER_MAX -FFGET_BUFFER_PADDING))
		{
			if (feof(f->f))
			{
				f->FILEEND = 1;
			}
			else
			{
				LOGGER_log("%s:%d:FFGET_getnewblock:ERROR: File read failed with error:%s", FL, strerror(errno));
				return 0;
			}
		}

		if (bs > 0)
		{

			// If we read in some data, then adjust the buffer to deal with it
			//
			// First we set the start point back to the start of the buffer,
			// then we set the end point to be the start +datasize we read, -1
			// then we adjust the total bytes read (for the sake of record keeping
			// though it has no /real/ purpose)
			//

			f->buffer[bs] = '\0';	//20040208-1703:PLD:JS
			f->last_block_read_from = block_pos; // 200607150941:PLD
			f->startpoint = f->buffer;
			f->endpoint = f->startpoint +bs -1;
			f->bytes += bs;

			// Check the buffer for poisioning \0's
			//  As these routines are being used for 7-bit valid text data,
			// we have to filter out any nasty \0's.

			if (FFGET_ALLOW_NUL == 0)
			{
				p = f->startpoint;
				for (i = 0; i < bs; i++) {
					if (*p == '\0') *p = ' ';
					p++;
				}
				*p = '\0';
			}

			if (FFGET_DPEDANTIC) LOGGER_log("%s:%d:FFGET_getnewblock:DEBUG-PEDANTIC: Size: %ld bytes\n", FL, f->bytes);

		}

	}

	return bs;
}






/*------------------------------------------------------------------------
Procedure:     FFGET_presetbuffer ID:1
Purpose:       Presets the FFGET buffer with defined data
Input:         FFGET_FILE record
Buffer to get data from
Quantity of data
Output:        None
Errors:
------------------------------------------------------------------------*/
int FFGET_presetbuffer( FFGET_FILE *f, char *buffer, int size )
{
	if (size > FFGET_BUFFER_MAX) size = FFGET_BUFFER_MAX;

	memcpy(f->buffer,buffer,size);
	f->startpoint = buffer;
	f->endpoint = buffer +size;
	return 0;
}


/*------------------------------------------------------------------------
Procedure:     FFGET_setstream ID:1
Purpose:       Sets the FILE * stream to the FFGET_FILE record
Input:         FFGET_FILE record
Stream to use.
Output:
Errors:
------------------------------------------------------------------------*/
int FFGET_setstream( FFGET_FILE *f, FILE *fi )
{

//	memset(f,0,sizeof(FFGET_FILE)); // be pedantic - clear the struct

	f->f = fi;
	f->bytes = 0;
	f->linecount = 0;
	f->endpoint = f->buffer;
	f->startpoint = f->endpoint +1;
	f->buffer_end = f->buffer +FFGET_BUFFER_MAX +FFGET_BUFFER_PADDING;
	f->trueblank = 0;
	f->ungetcset = 0;
	f->lastchar = '\0';
	memset(f->buffer,'\0',FFGET_BUFFER_MAX +FFGET_BUFFER_PADDING);
	f->c = '\0';
	f->FFEOF = 0;
	f->FILEEND = 0;
	f->last_block_read_from = -1;
	f->linebreak = FFGET_LINEBREAK_NONE;
	f->lastbreak[0] = '\0';
	return 0;
}


/*------------------------------------------------------------------------
Procedure:     FFGET_closestream ID:1
Purpose:       Closes the stream contained in a FFGET record
Input:         FFGET record containing the stream to close.
Output:
Errors:
------------------------------------------------------------------------*/
int FFGET_closestream( FFGET_FILE *f )
{
	f->startpoint = f->endpoint = NULL;
	f->f = NULL;
	return 0;
}

/*------------------------------------------------------------------------
Procedure:     FFGET_feof ID:1
Purpose:       Returns the status of FFGET's EOF
Input:         FFGET record
Output:        EOF status (0 == NOT eof, 1 == EOF has been reached)
Errors:
------------------------------------------------------------------------*/
int FFGET_feof( FFGET_FILE *f )
{
	return f->FFEOF;
}



/*-----------------------------------------------------------------\
  Function Name	: FFGET_seek
  Returns Type	: int
  ----Parameter List
  1. FFGET_FILE *f, 
  2.  size_t offset , 
  ------------------
  Exit Codes	: 
  -1 = error, check logs for reason of failure.

  Side Effects	: 
  --------------------------------------------------------------------
Comments:
Seeks to 'offset' bytes from the first byte of the file.
Reloads the buffer block.

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int FFGET_seek( FFGET_FILE *f, long offset, int whence )
{
	int result = 0;

	/** Move to the new block location **/
	result = fseek(f->f, offset, whence);
	if (result == -1) {
		LOGGER_log("%s:%d:FFGET_seek:ERROR: While attempting to seek to offset %ld from %d - [%s]", FL, offset, whence, strerror(errno));
		return -1;
	}

	/** Read a whole new block **/
	result = FFGET_getnewblock(f);

	return result;
}



/*-----------------------------------------------------------------\
  Function Name	: FFGET_tell
  Returns Type	: size_t
  ----Parameter List
  1. FFGET_FILE *f , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
Returns the position in the file that the current "file cursor"
is pointing to.


--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
long FFGET_ftell( FFGET_FILE *f )
{
	long pos;

	pos = f->last_block_read_from +(f->startpoint -f->buffer);

	return pos;

}




/*------------------------------------------------------------------------
Procedure:     FFGET_ungetc ID:1
Purpose:       Pushes back into the buffer (effectively) a single character
Input:         FFGET record
Character to retain for the next read.
Output:
Errors:
------------------------------------------------------------------------*/
int FFGET_ungetc( FFGET_FILE *f, char c )
{
	f->c = c;
	f->ungetcset = 1;
	return 0;
}




/*------------------------------------------------------------------------
Procedure:     FFGET_getc ID:1
Purpose:       Gets a single character from the FFGET buffer
Input:         FFGET record
Output:        Single character from the buffer, or EOF if end of file.
Errors:
------------------------------------------------------------------------*/
#ifdef sgi
short FFGET_fgetc( FFGET_FILE *f )
#else
char FFGET_fgetc( FFGET_FILE *f )
#endif
{
	int c;

	if (f->ungetcset)
	{
		f->ungetcset = 0;
		return f->c;
	}

	if ((!f->startpoint)||(f->startpoint > f->endpoint))
	{
		FFGET_getnewblock(f);
	}

	if (f->FFEOF == 0)
	{
		c = *f->startpoint;
		f->startpoint++;
	}
	else
	{
		c = EOF;
	}

	return c;
}




/*------------------------------------------------------------------------
Procedure:     FFGET_fgets ID:1
Purpose:       Gets a single line from the input buffer. The line can be
either \r \n \r\n terminated based on the status flags set/unset
by previous reads.   This function is the key to making
tools like ripMIME be able to see double-vision, that is, to see
emails like Outlook does, and also like RFC.
Input:         line: Buffer to write to
max_size: Maximum number of bytes to write to line.
f: FFGET record to use to read.
Output:        Pointer to line.
Errors:
------------------------------------------------------------------------*/
char *FFGET_fgets( char *linein, int maxsize, FFGET_FILE *f )
{
	char *line = linein;
	char *crlfpos = NULL;
	int charstoCRLF = 0;
	int chardiff = 0;
	int result = 0;
	int max_size = maxsize;
	int endpoint_tainted = 0;
	int extra_char_kept=0;
	int c, nextchar;

	f->trueblank = 0;
	f->linebreak = FFGET_LINEBREAK_NONE;
	f->lastbreak[0] = '\0';

	if (f->FFEOF != 0)
	{
		return NULL;
	}

	if ((FFGET_SDL_WATCH > 0)||(FFGET_SDL_MODE != 0))
	{
		DELIMITERS = SDL_MODE_DELIMITS;
	}
	else DELIMITERS = NORM_MODE_DELIMITS;


	//	fprintf(stderr,"FFGET_called, SDLMODE = %d, Offset = %d, maxsize = %d, DATA left = %d, first char is '%02X'\n", FFGET_SDL_MODE, (f->startpoint -f->buffer), max_size, (f->endpoint -f->startpoint)+1, (int)(*f->startpoint));

	max_size = maxsize = maxsize -2;

	//	memset(line, 0, max_size+1);

	// If we dont have enough data in the buffer to fill up the fgets request
	// we'll have to do a two step fill

	//fprintf(stderr,"DATA Reminianing : %d\n", (int)(f->endpoint -f->startpoint)+1);

	if ((f->startpoint > f->endpoint))
	{
		result = FFGET_getnewblock(f);
		if (result == 0)
		{
			*linein = '\0';
			return NULL;
		}
	}



	// This loop does not go around too many times, once, maybe twice max.

	while ((max_size > 0)&&(f->FFEOF == 0))
	{

		crlfpos = strpbrk( f->startpoint, DELIMITERS);
		if (crlfpos)
		{
				extra_char_kept = 0;
				endpoint_tainted = 0;
				nextchar = -1;
			// if our next char is a CR or LF, then pick it up and
			// return it with the line.  NOTE - this is to deal with
			// CRLF pairings which are common on DOS files.  In fact,
			// this is a case of where UNIX is actually -wrong-.  It
			// should have also used CRLF pairing to mark line ends, but
			// someone obviously (and understandably, to save space)
			// thought they'd leave make LF imply a CR as well.
			//   Well done... another bugger up in life.


			// The logic of this nested IF statement is as follows...
			//	If we do have another char available...
			//		and if the pairing is not \n\n (which should be treated as two lines
			//			and if the next char is a \n or a \r,
			//				THEN we should increment the end of line pointer so that we
			//				include the additional \n or \r into the line we're going to
			//				return

			// If we are NOT in the Single-delimeter mode (SDL_MODE), and the next
			// char is available, then commence the delimeter testing routines

			if ((0==f->FILEEND)&&(0==f->FFEOF)&&( ((crlfpos +1) >  f->endpoint)))
			{

				// We have an EOL character, get 1 more from the stream to test the next character


				nextchar = c = fgetc(f->f);
				if (c==EOF)
				{
					//					fprintf(stderr,"EOF hit due to fgetc()\n");
					f->FILEEND = 1;
				}
				else
				{
					if (c == '\0') c = ' ';

					// Check for character value vadality

					if ((c > 0) && (c <= 255))
					{
						if (FFGET_DNORMAL) LOGGER_log("%s:%d:FFGET_fgets:DEBUG: Tainting endpoint +1 (%p -> %p,  hard buffer end = %p, file read bytes = %ld)", FL, f->endpoint, f->endpoint+1, f->buffer_end, f->bytes);
						f->endpoint++;
						*(f->endpoint) = c;
						*(f->endpoint+1) = '\0';
						endpoint_tainted = 1;
					}

				}
			} // If (crlfpos +1) is /not/ within our buffer bounds


			// If the next char from our CRLF pickup is within the bounds of
			// our endpoint, then proceed to test the CRLF combo

			if ( ((crlfpos +1) <= f->endpoint))
			{

				//				fprintf(stderr,"Found '%02X' [next is '%02X']\n",*crlfpos, *(crlfpos+1));

				if ( *crlfpos == '\n' )
				{
					f->linebreak = FFGET_LINEBREAK_LF;
					snprintf(f->lastbreak,sizeof(f->lastbreak),"\n");

					if ( *(crlfpos +1) == '\r' )
					{
						f->linebreak |= FFGET_LINEBREAK_CR;
						snprintf(f->lastbreak,sizeof(f->lastbreak),"\n\r");
						crlfpos++;
						extra_char_kept = 1;
					}
				}


				// If our combo starts with a \r, then test it to see
				// if we have another \r after it, in which case, we
				// turn on SINGLE_DELIMETER_MODE.

				if ( (*crlfpos == '\r') )
				{
					f->linebreak = FFGET_LINEBREAK_CR;
					snprintf(f->lastbreak,sizeof(f->lastbreak),"\r");

					if ( *(crlfpos +1) == '\r' )
					{
						// A \r\r sequence has just been detected, set our doubleCR flag
						// 	so that MIME_headers can read it and react accordingly.
						// Look out for single \r's from here on, as they are now seen as
						// 	EOL markers in Outlook.

						f->linebreak = FFGET_LINEBREAK_CR;
						FFGET_doubleCR=1;
						FFGET_SDL_MODE=1;
						crlfpos++;
						extra_char_kept = 1;

					} else if ( *(crlfpos +1) == '\n' ) {
						// If we see a \n after our \r, then treat this as a single
						// 	line delimeter if we are NOT in Single Delimeter mode
						//

						snprintf(f->lastbreak,sizeof(f->lastbreak),"\r\n");
						f->linebreak |= FFGET_LINEBREAK_LF;
						if (!FFGET_SDL_MODE) {
						  	crlfpos++;
							extra_char_kept = 1;
					  	}// 20040208-1706:PLD
						//crlfpos++;// 20040208-1706:PLD	 // 20040306-0003:PLD - this line causes a CRCR test to fail; mailpack.virus.badtrans
					} else {
						// If we saw a \r, but then there was no other EOL type char (\r or \n)
						//	then switch to SDL mode (Single delimeter).

						FFGET_SDL_MODE=1;

					}

				} // If combo starts with a \r

			} // If crlfpos +1  is within the bounds of our buffer.

			// Determine how many characters/bytes there are from the startpoint,
			// to the CRLF position.


			charstoCRLF	= crlfpos -f->startpoint;

			// If the number of chars is -less- than that of the maximum line read
			// size which our calling program has specified, then we set the max_size
			// to be the number of chars.

			//DEBUG			fprintf(stderr, "MAX_size = %d, charstoCRLF = %d\n", max_size, charstoCRLF);

			if ((charstoCRLF >= 0)&&(charstoCRLF < max_size)) max_size = charstoCRLF;

			if ((extra_char_kept == 0) && (nextchar != -1)) ungetc(nextchar,f->f);

		} // If CRLF pos found.


		//		else crlfpos = (f->endpoint +1);




		// If the buffer amount remaining in our FFGET buffer is greater than
		// the maximum size available in our line buffer, then we
		//  only copy the max_size amount across

		if (( f->endpoint -f->startpoint) >= max_size)
		{
			if (max_size < 0) LOGGER_log("%s:%d:FFGET_fgets:ERROR: Max size < 0\n", FL);
			memcpy(line, f->startpoint, max_size +1);//+1
			f->startpoint += (max_size +1); //+1
			*(line +max_size +1) = '\0'; //+1
			max_size = 0;

		} else {

			// else, if the amount of data available is /LESS/ than what we can
			// accept in the line buffer then copy what remains out to the line
			// buffer and then tack on the results of a new read.

			chardiff = f->endpoint -f->startpoint;

			//			fprintf(stderr,"CHARDiff = %d, FFEOF = %d, FILEEND = %d\n",chardiff, f->FFEOF, f->FILEEND);

			if (chardiff >= 0)
			{
				memcpy(line, f->startpoint, chardiff +1);
				*(line +chardiff +1) = '\0';  // 12-11-2002: Added this line to terminate the input buffer incase it wasn't already flushed with \0's
				line += (chardiff +1);
				max_size -= (chardiff +1);
				f->startpoint = f->endpoint +1;
				if (max_size < 0) max_size = 0;
			}

			FFGET_getnewblock(f);
			endpoint_tainted=0;

		} // If there wasn't enough data to satisfy ends.

		if (endpoint_tainted) {
			FFGET_getnewblock(f);
			endpoint_tainted = 0;
		}

	} // While we've got space to fill, and we've got data to read

	line = linein;

	f->trueblank = 0;

	if ((f->lastchar == '\n')||(f->lastchar == '\r'))
	{
		if ((line[0] == '\n')||(line[0] == '\r'))
		{
			f->trueblank = 1;
		}
	}

	f->lastchar = line[strlen(line) -1];

	f->linecount++;

	//	LOGGER_log("%s:%d:LINE='%s'",FL,linein);

	return linein;
}



/*------------------------------------------------------------------------
Procedure:     FFGET_raw ID:1
Purpose:       This is a hybrid binary-read and fgets type read.  This function
reads data from the input buffer until it encounters a \r \n \r\n
at which point it will return to the calling parent with its buffer
containing that line.  This is required so that we dont miss any
boundary specifiers which are located on new-lines.
Input:         f: FFGET record
buffer: memory location to write data to
max: maximum holding capacity of the raw buffer
Output:        Returns the number of bytes placed into the buffer.
Errors:
------------------------------------------------------------------------*/
int FFGET_raw( FFGET_FILE *f, unsigned char *buffer, int max )
{

	unsigned char c;					// read buffer

	int bytestogo = 0;
	int count = 0;						// How many bytes read

	// Special situation here, if we have a return from MIME_headers which indicates
	// that we have data in a MIMEH_pushback, then we need to process that first, before we
	// go back into the data file.


	if ((!f->startpoint)||(f->startpoint > f->endpoint))
	{
		bytestogo = FFGET_getnewblock(f);
	}
	else
	{
		bytestogo = f->endpoint -f->startpoint +1;
	}

	// Whilst we've got less bytes than the maximum availabe
	// for the buffer, we keep on reading
	//

	while (count < max)
	{

		if (!bytestogo)
		{
			bytestogo = FFGET_getnewblock(f);
		}

		if (!f->FFEOF)
		{
			c = *f->startpoint;
			f->startpoint++;
			*buffer = c;

			buffer++;

			count++;

			bytestogo--;

			// If we get a line delimeter, check to see that the next char (which is now
			// pointed to at f->startpoint isn't a delimeter as well which perhaps we should
			// be including in our line were' going to return
			//
			// 25/05/02 - Silly mistake, I had (!\n || !\r) when it should be && (ie, if the next
			// char is NEITHER of the \n or \r chars, then break.
			//

			if ((c == '\n')||(c == '\r'))
			{
				if (  (*(f->startpoint) != '\n') && (*(f->startpoint) != '\r') )
					break;
			}
		}
		else break;
	}

	*buffer = '\0';

	return count;
}



//--------------END.




