/***************************************************************************
 * tnef2txt
 *   A program to decode application/ms-tnef MIME attachments into text
 *   for those fortunate enough not to be running either a Microsoft
 *   operating system or mailer.
 *
 * 18/10/2001
 * Brutally cropped by Paul L Daniels (pldaniels@pldaniels.com) in order
 * to accommodate the needs of ripMIME/Xamime/Inflex without carrying too
 * much excess baggage.
 *
 * Brandon Long (blong@uiuc.edu), April 1997
 * 1.0 Version
 *   Supports most types, but doesn't decode properties.  Maybe some other
 *   time.
 *
 * 1.1 Version (7/1/97)
 *   Supports saving of attAttachData to a file given by attAttachTitle
 *   start of property decoding support
 *
 * 1.2 Version (7/19/97)
 *   Some architectures don't like reading 16/32 bit data on unaligned
 *   boundaries.  Fixed, losing efficiency, but this doesn't really
 *   need efficiency anyways.  (Still...)
 *   Also, the #pragma pack from the MSVC include file wasn't liked
 *   by most Unix compilers, replaced with a GCCism.  This should work
 *   with GCC, but other compilers I don't know.
 *
 * 1.3 Version (7/22/97)
 *   Ok, take out the DTR over the stream, now uses read_16.
 *
 * 1.5 Version (4/24/24)
 *   Rewritten from file utility to part of ripMIME
 *
 * NOTE: THIS SOFTWARE IS FOR YOUR PERSONAL GRATIFICATION ONLY.  I DON'T
 * IMPLY IN ANY LEGAL SENSE THAT THIS SOFTWARE DOES ANYTHING OR THAT IT WILL
 * BE USEFULL IN ANY WAY.  But, you can send me fixes to it, I don't mind.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include "logger.h"

#include "config.h"
#include "tnef.h"
#include "mapidefs.h"
#include "mapitags.h"
#include "tnef_api.h"

#define VERSION "pldtnef/0.0.2"

#ifndef FL
#define FL __FILE__, __LINE__
#endif

#define TNEF_VERBOSE ((TNEF_glb.verbose > 0))
#define TNEF_DEBUG ((TNEF_glb.debug > 0))

/** 20041207-1246:PLD: Added RT32 macro to allow for large numbers of read-tests **/
#define RT32( num_addr, offset ) if (read_32(num_addr, offset)==-1) return -1

struct TNEF_globals {
	int file_num;
	int verbose;
	int verbosity_contenttype;
	int debug;

	int TNEF_Verbose;

	uint8 *tnef_home;
	uint8 *tnef_limit;

	int (*filename_decoded_report)(char *, char *);	// Pointer to our filename reporting function
};

static struct TNEF_globals TNEF_glb;

/*-----------------------------------------------------------------\
  Function Name	: TNEF_init
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
void TNEF_init( void )
{
	TNEF_glb.file_num = 0;
	TNEF_glb.verbose = 0;
	TNEF_glb.verbosity_contenttype = 0;
	TNEF_glb.debug = 0;
	TNEF_glb.TNEF_Verbose = 0;
	TNEF_glb.filename_decoded_report = NULL;
}

/*------------------------------------------------------------------------
Procedure:     TNEF_set_verbosity ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int TNEF_set_verbosity( int level )
{
	TNEF_glb.verbose = level;
	return TNEF_glb.verbose;
}

/*-----------------------------------------------------------------\
  Function Name	: TNEF_set_verbosity_contenttype
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
int TNEF_set_verbosity_contenttype( int level )
{
	TNEF_glb.verbosity_contenttype = level;
	return TNEF_glb.verbosity_contenttype;
}

/*-----------------------------------------------------------------\
  Function Name	: TNEF_set_filename_report_fn
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
int TNEF_set_filename_report_fn( int (*ptr_to_fn)(char *, char *) )
{
	TNEF_glb.filename_decoded_report = ptr_to_fn;
	return 0;
}

/*------------------------------------------------------------------------
Procedure:     TNEF_set_debug ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int TNEF_set_debug( int level )
{
	TNEF_glb.debug = level;
	TNEF_set_verbosity( level );
	return TNEF_glb.debug;
}

/* Some systems don't like to read unaligned data */
/*------------------------------------------------------------------------
Procedure:     read_32 ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int read_32( uint32 *value, uint8 *tsp)
{
	uint8 a,b,c,d;

	if ((tsp +4) > TNEF_glb.tnef_limit)
	{
		if ((TNEF_VERBOSE)||(TNEF_DEBUG)) LOGGER_log("%s:%d:%s:ERROR: Attempting to read beyond end of memory block",FL,__func__);
		return -1;
	}

	a = *tsp;
	b = *(tsp+1);
	c = *(tsp+2);
	d = *(tsp+3);

	*value =  long_little_endian(a<<24 | b<<16 | c<<8 | d);

	return 0;
}

/*------------------------------------------------------------------------
Procedure:     read_16 ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int read_16( uint16 *value, uint8 *tsp)
{
	uint8 a,b;

	if (
			((tsp +2) > TNEF_glb.tnef_limit)
//PLD-20070707-17H20, how do we even compare tsp to a negative!?		||(tsp == -1)
			)
	{
		if ((TNEF_VERBOSE)||(TNEF_DEBUG)) LOGGER_log("%s:%d:%s:ERROR: Attempting to read past end\n",FL,__func__);
		return -1;
	}

	//	if (TNEF_DEBUG) fprintf(stderr,"Read_16: Offset read %d\n", tsp -tnef_home);

	a = *tsp;
	b = *(tsp + 1);

	*value = little_endian(a<<8 | b);

	return 0;
}

/*------------------------------------------------------------------------
Procedure:     make_string ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
char *make_string(uint8 *tsp, int size)
{
	static char s[256] = "";

	snprintf(s,sizeof(s),"%s",tsp);
	return s;
}

/*------------------------------------------------------------------------
Procedure:     save_attach_data ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int save_attach_data(char *title, uint8 *tsp, uint32 size, char * file_dir)
{
	FILE *out;
	char * fn;
	int fn_l = strlen(file_dir) + strlen(title) + sizeof(char) * 2;

	fn = malloc(fn_l);
	snprintf(fn,fn_l,"%s/%s",file_dir,title);        
	out = fopen(fn, "w");
	if (!out)
	{
		LOGGER_log("%s:%d:%s:ERROR: Failed opening file %s for writing (%s)\n", FL,__func__, fn, strerror(errno));
		free(fn);
		return -1;
	}

	free(fn);
	fwrite(tsp, sizeof(uint8), size, out);
	fclose(out);
	return 0;
}

/*------------------------------------------------------------------------
Procedure:     handle_props ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int handle_props(uint8 *tsp, char * file_dir)
{
	int bytes = 0;
	uint32 num_props = 0;
	uint32 x = 0;


	RT32(&num_props, tsp);
	bytes += sizeof(num_props);

	while (x < num_props)
	{
		uint32 prop_tag;
		uint32 num;
		char filename[256];

		RT32(&prop_tag, tsp+bytes);
		bytes += sizeof(prop_tag);

		switch (prop_tag & PROP_TYPE_MASK)
		{
			case PT_BINARY:
				RT32(&num, tsp+bytes);
				bytes += sizeof(num);
				RT32(&num, tsp+bytes);
				bytes += sizeof(num);
				if (prop_tag == PR_RTF_COMPRESSED)
				{
					sprintf (filename, "XAM_%d.rtf", TNEF_glb.file_num);
					TNEF_glb.file_num++;
					save_attach_data(filename, tsp+bytes, num, file_dir);
				}
				/* num + PAD */
				bytes += num + ((num % 4) ? (4 - num%4) : 0);
				break;
			case PT_STRING8:
				RT32(&num, tsp+bytes);
				bytes += sizeof(num);
				RT32(&num, tsp+bytes);
				bytes += sizeof(num);
				make_string(tsp+bytes,num);
				bytes += num + ((num % 4) ? (4 - num%4) : 0);
				break;
			case PT_UNICODE:
			case PT_OBJECT:
				break;
			case PT_I2:
				bytes += 2;
				break;
			case PT_LONG:
				bytes += 4;
				break;
			case PT_R4:
				bytes += 4;
				break;
			case PT_DOUBLE:
				bytes += 8;
				break;
			case PT_CURRENCY:
			case PT_APPTIME:
			case PT_ERROR:
				bytes += 4;
				break;
			case PT_BOOLEAN:
				bytes += 4;
				break;
			case PT_I8:
				bytes += 8;
			case PT_SYSTIME:
				bytes += 8;
				break;
		}
		x++;
	}
	return 0;
}

/*------------------------------------------------------------------------
Procedure:     default_handler ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int default_handler(uint32 attribute, uint8 *tsp, uint32 size)
{
	uint16 type = ATT_TYPE(attribute);

	switch (type) {
		case atpTriples:
			break;
		case atpString:
		case atpText:
			break;
		case atpDate:
			break;
		case atpShort:
			break;
		case atpLong:
			break;
		case atpByte:
			break;
		case atpWord:
			break;
		case atpDword:
			break;
		default:
			break;
	}
	return 0;
}

/*------------------------------------------------------------------------
Procedure:     read_attribute ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int read_attribute(uint8 *tsp, char *file_dir)
{

	int bytes = 0, header = 0;
	int rv = 0;
	uint32 attribute;
	uint32 size = 0;
	uint16 checksum = 0;
	static char attach_title[256] = {
		0				};
	static uint32 attach_size = 0;
	//static uint32 attach_loc  = 0; // 2003-02-22-1231-PLD
	static uint8 *attach_loc  = 0;

	bytes += sizeof(uint8);

	// Read the attributes of this component

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: Reading Attribute...\n",FL,__func__);
	rv = read_32(&attribute, tsp+bytes);
	if (rv == -1) return -1;
	bytes += sizeof(attribute);

	// Read the size of the information we have to read

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s: Reading Size...\n",FL,__func__);
	rv = read_32(&size, tsp+bytes);
	if (rv == -1) return -1;
	bytes += sizeof(size);

	// The header size equals the sum of all the things we've read
	//  so far.

	header = bytes;

	// The is a bit of a tricky one [if you're being slow
	//  it moves the number of bytes ahead by the amount of data of
	//  the attribute we're about to read, so that for next
	//  "read_attribute()"
	//  call starts in the right place.

	bytes += size;

	// Read in the checksum for this component
	//
	// AMMENDMENT - 19/07/02 - 17H01
	// Small code change to deal with strange sitations that occur with non
	//		english characters. - Submitted by wtcheuk@netvigator.com @ 19/07/02

	if ( bytes < 0 ) return -1;

	// --END of ammendment.

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: Reading Checksum...(offset %d, bytes=%d)\n", FL,__func__, tsp -TNEF_glb.tnef_home, bytes);
	if (read_16(&checksum, tsp+bytes) == -1) return -1;

	bytes += sizeof(checksum);

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: Decoding attribute %d\n", FL,__func__, attribute);

	switch (attribute) {
		case attNull:
			default_handler(attribute, tsp+header, size);
			break;
		case attFrom:
			default_handler(attribute, tsp+header, size);
			break;
		case attSubject:
			break;
		case attDateSent:
			break;
		case attDateRecd:
			break;
		case attMessageStatus:
			break;
		case attMessageClass:
			break;
		case attMessageID:
			break;
		case attParentID:
			break;
		case attConversationID:
			break;
		case attBody:
			default_handler(attribute, tsp+header, size);
			break;
		case attPriority:
			break;
		case attAttachData:
			attach_size=size;
			//		attach_loc =(int)tsp+header; // 2003-02-22-1232-PLD
			attach_loc =(uint8 *)tsp+header;
			if (strlen(attach_title)>0 && attach_size > 0) {
				if (!save_attach_data(attach_title, (uint8 *)attach_loc,attach_size,file_dir))
				{
					if (TNEF_VERBOSE) {
						if (TNEF_glb.filename_decoded_report == NULL)
						{
							LOGGER_log("Decoding: %s\n", attach_title);
						} else {
							TNEF_glb.filename_decoded_report( attach_title, (TNEF_glb.verbosity_contenttype>0?"tnef":NULL));
						}

					}
				}
				else
				{
					LOGGER_log("%s:%d:%s:ERROR: While saving attachment '%s'\n", FL,__func__, attach_title);
				}
			}
			break;
		case attAttachTitle:
			strncpy(attach_title, make_string(tsp+header,size),255);
			if (strlen(attach_title)>0 && attach_size > 0) {
				if (!save_attach_data(attach_title, (uint8 *)attach_loc,attach_size, file_dir))
				{
					if (TNEF_VERBOSE) {
						if (TNEF_glb.filename_decoded_report == NULL)
						{
							LOGGER_log("Decoding: %s\n", attach_title);
						} else {
							TNEF_glb.filename_decoded_report( attach_title, (TNEF_glb.verbosity_contenttype>0?"tnef":NULL));
						}

					}
				}
				else
				{
					LOGGER_log("%s:%d:%s:ERROR: While saving attachment '%s'\n", FL,__func__, attach_title);
				}
			}
			break;
		case attAttachMetaFile:
			default_handler(attribute, tsp+header, size);
			break;
		case attAttachCreateDate:
			break;
		case attAttachModifyDate:
			break;
		case attDateModified:
			break;
		case attAttachTransportFilename:
			default_handler(attribute, tsp+header, size);
			break;
		case attAttachRenddata:
			attach_title[0]=0;
			attach_size=0;
			attach_loc=0;
			default_handler(attribute, tsp+header, size);
			break;
		case attMAPIProps:
			if (handle_props(tsp+header, file_dir)==-1) return -1;
			break;
		case attRecipTable:
			default_handler(attribute, tsp+header, size);
			break;
		case attAttachment:
			default_handler(attribute, tsp+header, size);
			break;
		case attTnefVersion:
			{
				uint32 version;
				rv = read_32(&version, tsp+header);
				if (rv == -1) return -1;
			}
			break;
		case attOemCodepage:
			default_handler(attribute, tsp+header, size);
			break;
		case attOriginalMessageClass:
			break;
		case attOwner:
			default_handler(attribute, tsp+header, size);
			break;
		case attSentFor:
			default_handler(attribute, tsp+header, size);
			break;
		case attDelegate:
			default_handler(attribute, tsp+header, size);
			break;
		case attDateStart:
			break;
		case attDateEnd:
			break;
		case attAidOwner:
			default_handler(attribute, tsp+header, size);
			break;
		case attRequestRes:
			default_handler(attribute, tsp+header, size);
			break;
		default:
			default_handler(attribute, tsp+header, size);
			break;
	}
	return bytes;

}

/*------------------------------------------------------------------------
Procedure:     decode_tnef ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int TNEF_decode_tnef(uint8 *tnef_stream, int size, char* file_dir)
{

	int ra_response;
	uint32 tnefs;
	uint16 tnef_attachkey;
	uint8 *tsp;

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: Start. Size = %d\n", FL,__func__,size);

	// TSP == TNEF Stream Pointer (well memory block actually!)
	//
	tsp = tnef_stream;

	// Read in the signature of this TNEF
	//
	ra_response = read_32(&tnefs, tsp);
	if ((ra_response != -1)&&(TNEF_SIGNATURE == tnefs))
	{
		if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: TNEF signature is good\n",FL,__func__);
	} else {
		if (TNEF_VERBOSE) LOGGER_log("%s:%d:%s:WARNING: Bad TNEF signature, expecting %lx got %lx\n",FL,__func__,TNEF_SIGNATURE,tnefs);
	}

	// Move tsp pointer along
	//
	tsp += sizeof(TNEF_SIGNATURE);

	/** Read the TNEF Attach key **/
	if (read_16(&tnef_attachkey, tsp) == -1) return -1;
	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: TNEF Attach Key: %x\n",FL,__func__,tnef_attachkey);

	// Move tsp pointer along
	//
	tsp += sizeof(uint16);

	// While we still have more bytes to process,
	//		go through entire memory block and extract
	//		all the required attributes and files
	//
	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: TNEF - Commence reading attributes\n",FL,__func__);
	while ((tsp - tnef_stream) < size)
	{
		if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: Offset = %d\n", FL,__func__,tsp -TNEF_glb.tnef_home);
		ra_response = read_attribute(tsp, file_dir);
		if ( ra_response > 0 )
		{
			tsp += ra_response;
		} else {

			// Must find out /WHY/ this happens, and, how to rectify the issue.

			tsp++;
			if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:WARNING: TNEF - Attempting to read attribute at %d resulted in a sub-zero response, ending decoding to be safe\n",FL,__func__,tsp);
			break;
		}
	}

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: Done.\n",FL,__func__);

	return 0;
}

int TNEF_file_processing( FILE *fp, char *file_dir )
{
	uint8 *tnef_stream;
	int size, nread;

	// Get the filesize
	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	// Allocate enough memory to read in the ENTIRE file
	// FIXME - This could be a real consumer if multiple
	// instances of TNEF decoding is going on
	TNEF_glb.tnef_home = tnef_stream = (uint8 *)malloc(size);
	TNEF_glb.tnef_limit = TNEF_glb.tnef_home +size;

	// If we were unable to allocate enough memory, then we
	// should report this
	if (tnef_stream == NULL)
	{
		LOGGER_log("%s:%d:%s:ERROR: When allocating %d bytes for loading file (%s)\n", FL,__func__, size,strerror(errno));
		if (TNEF_glb.tnef_home) free(TNEF_glb.tnef_home);
		return -1;
	}

	// Attempt to read in the entire file
	nread = fread(tnef_stream, sizeof(uint8), size, fp);

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: Read %d bytes\n", FL,__func__, nread);

	// If we did not read in all the bytes, then let syslogs know!
	if (nread < size)
	{
		LOGGER_log("%s:%d:%s:ERROR: while reading stream from TNEF file (%s)\n", FL,__func__, strerror(errno));
		if (TNEF_glb.tnef_home) free(TNEF_glb.tnef_home);
		return -1;
	}

	// Proceed to decode the file
	TNEF_decode_tnef(tnef_stream,size, file_dir);

	if (TNEF_glb.tnef_home) free(TNEF_glb.tnef_home);

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: finished decoding.\n",FL,__func__);

	return 0;
}

/*------------------------------------------------------------------------
Procedure:     TNEF_main ID:1
Purpose:       Decodes a given TNEF encoded file
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int TNEF_main( char *filename, char *file_dir )
{
	FILE *fp;

	if (TNEF_DEBUG) LOGGER_log("%s:%d:%s:DEBUG: Start, decoding %s\n",FL,__func__, filename);

	// Attempt to open up the TNEF encoded file... if it fails
	// 	then report the failed condition to syslog
	if ((fp = fopen(filename,"r")) == NULL)
	{
		LOGGER_log("%s:%d:%s:ERROR: opening file %s for reading (%s)\n", FL,__func__, filename,strerror(errno));
		if (TNEF_glb.tnef_home) free(TNEF_glb.tnef_home);
		return -1;
	}
	TNEF_file_processing(fp, file_dir);
	
	// Close the file
	fclose(fp);
	return 0;
}

//--------------------------END.
