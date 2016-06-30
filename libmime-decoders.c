#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#include "ffget.h"
#include "pldstr.h"
#include "logger.h"
#include "libmime-decoders.h"


#ifndef FL
#define FL __FILE__,__LINE__
#endif

#define MDECODE_ISO_CHARSET_SIZE_MAX 16

// Debug precodes
#define MDECODE_DPEDANTIC ((glb.debug >= MDECODE_DEBUG_PEDANTIC))
#define MDECODE_DNORMAL   ((glb.debug >= MDECODE_DEBUG_NORMAL  ))
#define DMD if ((glb.debug >= MDECODE_DEBUG_NORMAL))


/* our base 64 decoder table */
static unsigned char b64[256]={
	128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,   62,  128,  128,  128,   63,\
		52,   53,   54,   55,   56,   57,   58,   59,   60,   61,  128,  128,  128,    0,  128,  128,\
		128,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,\
		15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25,  128,  128,  128,  128,  128,\
		128,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,\
		41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,\
		128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128,  128 \
};

/**
 * Jan 17th 2007 jjohnston
 * Fixed a bug that decoded invalid QP sequences
 */
static unsigned char hexconv[256]={
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		 0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   20,   20,   20,   20,   20,   20,\
		20,   10,   11,   12,   13,   14,   15,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   10,   11,   12,   13,   14,   15,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,\
		20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20,   20 \
};



struct MDECODE_globals {
	int debug;
	int verbose;
	int decode_qp;
	int decode_b64;
};

static struct MDECODE_globals glb;




int MDECODE_init( void )
{
	glb.debug = 0;
	glb.verbose = 0;
	glb.decode_qp = 1;
	glb.decode_b64 = 1;

	return 0;
}




/*------------------------------------------------------------------------
Procedure:     MIME_set_debug ID:1
Purpose:       Sets the debug level for reporting in MIME
Input:         int level : What level of debugging to use, currently there
are only two levels, 0 = none, > 0 = debug info
Output:
Errors:
------------------------------------------------------------------------*/
int MDECODE_set_debug( int level )
{
	glb.debug = level;
	return glb.debug;
}


int MDECODE_set_verbose( int level )
{
	glb.verbose = level;
	return glb.verbose;
}

int MDECODE_set_decode_qp( int level )
{ 
	glb.decode_qp = level;
	return glb.decode_qp;
}

int MDECODE_set_decode_b64( int level )
{
	glb.decode_b64 = level;
	return glb.decode_b64;
}


/*------------------------------------------------------------------------
Procedure:     MDECODE_decode_short64 ID:1
Purpose:       Decodes a BASE64 encoded realm
Input:         char *realm : base64 encoded NUL terminated string
Output:			decoded data is written to the short64 char
Errors:
------------------------------------------------------------------------*/
int MDECODE_decode_short64( char *short64 )
{
	int i;
	int realm_size = strlen( short64 );
	int stopcount = 0; /* How many stop (=) characters we've read in */
	int c; /* a single char as retrieved using MDECODE_get_char() */
	int char_count = 0; /* How many chars have been received */
	char output[3]; /* The 4->3 byte output array */
	char input[4]; /* The 4->3 byte input array */
	char *outstring = short64;

	char_count = 0;
	while (char_count < realm_size)
	{

		/* Initialise the decode buffer */
		input[0] = input[1] = input[2] = input[3] = 0;

		/* snatch 4 characters from the input */
		for (i = 0; i < 4; i++) {

			/* get a char from the filestream */
			c = *short64;
			short64++;

			/* assuming we've gotten this far, then we increment the char_count */
			char_count++;

			/* if we detect the "stopchar" then we better increment the STOP counter */
			if (c == '=') {
				stopcount++;
			}

			/* test for and discard invalid chars */
			if (b64[c] == 0x80) {
				i--;
				continue;
			}

			/* do the conversion from encoded -> decoded */
			input[i] = (char)b64[c];

		} /* for */

		/* now that our 4-char buffer is full, we can do some fancy bit-shifting and get the required 3-chars of 8-bit data */
		output[0] = (input[0] << 2) | (input[1] >> 4);
		output[1] = (input[1] << 4) | (input[2] >> 2);
		output[2] = (input[2] << 6) | input[3];

		/* determine how many chars to write write and check for errors if our input char count was 4 then we did receive a propper 4:3 Base64 block, hence write it */
		if (i == 4) {
			for (i = 0; i < (3 -stopcount); i++){
				*outstring = output[i];
				outstring++;
			} /* copy our data across */
		} /* if 4 chars were inputted */
	} /* while more chars to proccess */


	*outstring = '\0';  // Set the last char to NULL

	return 0;
}









/*------------------------------------------------------------------------
Procedure:     MDECODE_decode_quoted_printable ID:1
Purpose:       Decodes quoted printable encoded data.
Input:         char *line : \0 terminated string possibly containing quoted printable data
int qpmode : Selects which decoding ruleset to use ( refer to RFC2047 )
Output:        Decoded string is superimposed over the provided line parameter
Returns: Returns the number of bytes decoded.
------------------------------------------------------------------------*/
int MDECODE_decode_quoted_printable( char *line, int qpmode, char esc_char )
{

	char c;								/* The Character to output */
	int op, ip; 						/* OutputPointer and InputPointer */
	int slen = strlen(line); /* Length of our line */

	DMD LOGGER_log("%s:%d:MDECODE_decode_quoted_printable:DEBUG: input string = '%s' Input length = %d\n",FL, line, slen);

	/* Initialise our "pointers" to the start of the encoded string */
	ip=op=0;

	/* for every character in the string... */

	for (ip = 0; ip < slen; ip++)
	{
		c = line[ip];

		/* if we have the quoted-printable esc char, then lets get cracking */
		if (c == esc_char)
		{

			/* if we have another two chars... */
			if ((ip +1) < slen )
			{
				int original_ip = ip;

				/* Is our next char a \n\r ?

					if it is, then we have to eliminate any further \r\n's etc
					so as to turn the =\n\r into a 'soft return', which basically
					means that we ignore it.  Soft-breaks are used so we can
					fit our long lines into the requirement of a maximum of 76 characters
					per line.

					So we move the input-pointer along skipping each character without
					incrementing the output pointer.

				 */

				/** Absorb any trailing whitespaces **/
				if (1)
				{
					char *w = &(line[ip +1]);
					while ((*w == '\t') || (*w == ' ')) {w++;ip++;}
				}

				/** Do we now have a line break ? **/
				if (( line[ip +1] == '\n') || (line[ip +1] == '\r' ))
				{
					ip++;
					if ((ip+1 < slen)&&(( line[ip +1] == '\n') || (line[ip +1] == '\r' )))
					{
						ip++;
					}
					continue;
				}

				else
				{
					/*
						if the characters following the '=' symbol are not
						of the \n or \r pair, then we will [currently]
						assume that the next two characters are in fact the
						hexadecimal encodings of the character we do want

					 */
					
					/** Revert to original position **/
					ip = original_ip;

					/* convert our encoded character from HEX -> decimal */

					if ( ip < slen-1 ) // was 2, proving - if there are 3 chars in string, =AB, slen = 3, ip = 1
					{
						/**
						 * Jan 17th 2007 jjohnston
						 * Fixed a bug that decoded invalid QP sequences
						 */
						if(hexconv[(int)line[ip+1]] == 20 || hexconv[(int)line[ip+2]] == 20) {
						
							//LOGGER_log("%s:%d:MIME_decode_quoted_printable:NOTICE: Invalid characters for quoted-printable at '=%c%c'\n", FL, (int)&line[ip+1], (int)&line[ip+2]);
							
						
						} else {
						
						c = (char)hexconv[(int)line[ip+1]]*16 +hexconv[(int)line[ip+2]];

						/* shuffle the pointer up two spaces */
						ip+=2;
					}
					}
					else {
						LOGGER_log("%s:%d:MIME_decode_quoted_printable:WARNING: Ran out of characters when decoding end of '%s'\n", FL, &line[ip] );
					}
				}

			} /* if there were two extra chars after the ='s */


			/* if we didn't have enough characters, then  we'll make the char the
			 * string terminator (such as what happens when we get a =\n
			 */
			else
			{
				/* 2002-12-16:18H31: changed from 'line[ip]' to 'line[op]' */
				line[op] = '\0';
				/* 2002-12-16:18H32: added break statement - if we're out of chars, then we quit the for loop */
				break;
			} /* else */

		} /* if c was a encoding char */


		else
			if (( c == '_' ) && ( qpmode == MDECODE_QPMODE_ISO ))
			{
				// RFC2047  (Section 4.2.(2)(3)) says that if we encounter a '_' character in our ISO encodings then
				//	we must convert that to a space ( as we are not allowed to have spaces in any
				c = ' ';
			}

		/* put in the new character, be it converted or not */
		line[op] = c;

		/* shuffle up the output line pointer */
		op++;


	} /* for loop */

	/* terminate the line */

	line[op]='\0';

	DMD LOGGER_log("%s:%d:MDECODE_decode_quoted_printable:DEBUG: Output = '%s' Output length = %d\n", FL, line, strlen(line));

	// 2003-01-26:PLD: Changed from (op -1) -=> op
	return op;
}




/*------------------------------------------------------------------------
Procedure:     MDECODE_decode_text_line ID:1
Purpose:       Decodes a line of text, checking for Quoted-Printable characters
and converting them.  Note - if the character converted is a \0
(after decoding) it shouldn't affect the calling parent because the
calling parent should read back the returned string byte size and
use fwrite() or other non-\0 affected writing/processing functions
Input:         char *line: pointer to the buffer/line we wish to convert/scan
Output:        int: size of final buffer in bytes.
Errors:
------------------------------------------------------------------------*/
int MDECODE_decode_qp_text( char *line )
{
	if (glb.decode_qp == 0) return strlen(line);

	return MDECODE_decode_quoted_printable( line, MDECODE_QPMODE_STD, '=' );
}

int MDECODE_decode_qp_ISO( char *line )
{
//	return MDECODE_decode_quoted_printable( line, MDECODE_QPMODE_ISO, '=' );
	return MDECODE_decode_quoted_printable( line, MDECODE_QPMODE_STD, '=' );
}

int MDECODE_decode_multipart( char *line )
{
	return MDECODE_decode_quoted_printable( line, MDECODE_QPMODE_STD, '%' );
}



/*------------------------------------------------------------------------
Procedure:     MDECODE_decode_ISO ID:1
Purpose:       Decodes an ISO ( RFC2047 ) encoded string into native codepage dependent output
Input:         char *isostring : String containing =?code-page?encoding-type?string?= format
int length : length of the string we're decoding
Output:        isostring is overwritten with the decoded string.
Errors:
------------------------------------------------------------------------*/
int MDECODE_decode_ISO( char *isostring, int size )
{
	char *start_pair, *end_pair;
	char *iso, *iso_copy;
	char encoding_type='-';
	char encoding_charset[ MDECODE_ISO_CHARSET_SIZE_MAX ];
	char *iso_start, *iso_end;
	int iso_decoded;

	DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: ISO-string='%s'",FL,isostring);

	// Process of decoding the ISO encoded string sequence.
	//	( this process is repeated until we run out of ISO sequences )
	//
	//	1. Check that the string has a =? sequence within it ( indicates the start of the ISO encoding
	//
	//	2. tokenise the sequence succeeding the =? token into its three (3) parts, namely the code-page, encoding-type and string respectively
	//
	//	3. decode the string based on the encoding type, Q = Quoted-Printable, B = BASE64
	//

	iso_end = iso_start = NULL;

	start_pair = end_pair = NULL;

	iso_copy = malloc( sizeof(char) *( size +1 ) );

	do {

		iso_decoded = 0;

		start_pair = strstr( isostring, "=?" );
//		if ( start_pair ) end_pair   = strstr( start_pair +2, "?=" );

		if (( start_pair != NULL ))
		{
			iso_start = start_pair;

			// There's probably a better way of doing this, but, for us to find the end of this
			//	particular 'ISO' sequence, we need to hop past 3 more ?'s ( assuming we've already
			//	found the first one.
			DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: ISO start = %s",FL,iso_start);

			iso_end = strchr( iso_start +strlen("=?"), '?' ); // Jump past the encoding
			if (iso_end) iso_end = strchr( iso_end +1, '?' ); // Jump past the Q or B
			if (iso_end) iso_end = strpbrk( iso_end +1, "?\n\r\t;" );  // dropped the SPACE here.
			if ((iso_end != NULL)&&(*iso_end == '?')) iso_end+=2;


			if ( (iso_start) && (iso_end) )
			{
				char *token_end;
				char restore_char='\0';

				// Copy the Encoding page/code.
				iso = iso_start +strlen("=?");

				token_end = strchr(iso,'?');
				if (token_end) *token_end = '\0';
				snprintf( encoding_charset, sizeof( encoding_charset ), "%s", iso);
				DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: ISO char set = '%s'",FL,encoding_charset);

				iso = token_end +1;

				// Get the encoding _type_ (BASE64/QuotedPrintable etc)
				token_end = strchr(iso,'?');
				encoding_type = *iso;

				iso = token_end +1;

				DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: ISO encoding char = '%c'",FL,encoding_type);

				// Get the encoded string
				token_end = strpbrk(iso,"?;\n\r\t"); //DROPPED THE SPACE here
				if (token_end != NULL)
				{
					if ((*token_end != '?')&&(*token_end != ';'))
					{
						restore_char = *token_end;
					}
					*token_end = '\0';
				}







				if (iso)
				{
					DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: Encoded String = '%s'\n", FL, iso );
					switch ( encoding_type ) {

						case MDECODE_ISO_ENCODING_Q:
						case MDECODE_ISO_ENCODING_q:
							DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: Decoding filename using Quoted-Printable (%s)\n", FL, iso);
							MDECODE_decode_qp_ISO(iso);
							iso_decoded = 1;
							break;

						case MDECODE_ISO_ENCODING_B:
						case MDECODE_ISO_ENCODING_b:
							DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: Decoding filename using BASE64 (%s)\n", FL, iso);
							MDECODE_decode_short64( iso );
							iso_decoded = 1;
							break;

						default:
							if (glb.verbose) LOGGER_log("%s:%d:MDECODE_decode_ISO:ERROR: The encoding character '%c' is not a valid type of encoding\n", FL, encoding_type );
					}

					// If we decoded the string okay, then we need to recompose the string

					if ( iso_decoded == 1 )
					{
						char *new_end_pos;

						DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: Decoded String = '%s'\n", FL, iso );
						*iso_start = '\0'; // Terminate the original string before the start of the ISO data

						// Because sometimes ISO strings are broken over multiple lines
						//		due to wrapping requirements of RFC(2)822, we need to 
						//		sniff out these tab or spaces and crop them out of our
						//		final ISO string.  We cannot simply search for the next
						//		=? sequence using strstr() because it might traverse 
						//		beyond the end of the current 'line' (ie, \r\n termination)

						if (token_end)
						{
							iso_end = token_end +1;
							DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: iso_end = '%20s'",FL, iso_end);
							while ((*iso_end == '?')||(*iso_end == '=')) iso_end++;
							DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: iso_end = '%20s'",FL, iso_end);

							new_end_pos = iso_end;
							while ((*new_end_pos == ' ')||(*new_end_pos == '\t')) new_end_pos++;
							if (strncmp(new_end_pos,"=?",2)==0) iso_end = new_end_pos;
						} else {
							iso_end = NULL;
						}

						DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: ISO-END = '%20s'",FL,iso_end);


						
						/** We now have the string split into 3 peices,
						  **	isostring = pointing to the start
						  **	iso = newly decoded string
						  **	iso_end = start of string after the non-decoded ISO portion
						  **/
						
						/** Generate new string using the decoded ISO to a temporary string **/
						if (restore_char != '\0')
						{
							DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: Recomposing string with restore-char of '%c'",FL,restore_char);
							DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: ISO-end (start of end of string) is \n%s",FL,iso_end);
							snprintf( iso_copy, size, "%s%s%c%s", isostring, iso, restore_char, (iso_end?iso_end:"") );
						} else {
							DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: Recomposing string with NO restore-char",FL,restore_char);
						  	snprintf( iso_copy, size, "%s%s%s", isostring, iso, (iso_end?iso_end:"") );
						}

						/** Switch the new headers over to the original headers again **/
						snprintf( isostring, size, "%s", iso_copy );
						DMD LOGGER_log("%s:%d:MDECODE_decode_ISO:DEBUG: New ISO string = \n%s",FL,isostring);

					}

				}
			}

		} // if (iso_start)

	}
	while (iso_decoded == 1 );

	if (iso_copy) free(iso_copy);

	return 0;

}





//------------END libmime-decoders.c


