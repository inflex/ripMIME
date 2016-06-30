#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "logger.h"
#include "pldstr.h"

#include "bytedecoders.h"
#include "olestream-unwrap.h"

#define DUW if (oleuw->debug)

struct OLE10_header{
	unsigned char data[6];
	char *attach_name;
	unsigned char data2[8];
	char *fname_1;
	char *fname_2;
	size_t attach_size;
	size_t attach_size_1;
	size_t attach_start_offset;
};

struct ESCHER_header_fixed {
	int spid_max;
	size_t cidcl;
	size_t cspsaved;
	size_t cdgsaved;
};

struct typesig {
	char *sequence;
	int length;
	int offset;
};


/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_init
 Returns Type	: int
 	----Parameter List
	1. struct OLEUNWRAP_object *oleuw , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_init( struct OLEUNWRAP_object *oleuw )
{
	oleuw->debug = 0;
	oleuw->verbose = 0;
	oleuw->filename_report_fn = NULL;

	return OLEUW_OK;
}

/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_set_debug
 Returns Type	: int
 	----Parameter List
	1. struct OLEUNWRAP_object *oleuw, 
	2.  int level , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_set_debug( struct OLEUNWRAP_object *oleuw, int level )
{
	oleuw->debug = level;
	return OLEUW_OK;
}

/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_set_verbose
 Returns Type	: int
 	----Parameter List
	1. struct OLEUNWRAP_object *oleuw, 
	2.  int level , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_set_verbose( struct OLEUNWRAP_object *oleuw, int level )
{
	oleuw->verbose = level;
	return OLEUW_OK;
}

/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_set_save_unknown_streams
 Returns Type	: int
 	----Parameter List
	1. struct OLEUNWRAP_object *oleuw, 
	2.  int level , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_set_save_unknown_streams( struct OLEUNWRAP_object *oleuw, int level )
{
	oleuw->save_unknown_streams = level;
	return OLEUW_OK;
}


/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_save_stream
 Returns Type	: int
 	----Parameter List
	1. char *fname, 
	2.  char *stream, 
	3.  size_t bytes , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_save_stream( struct OLEUNWRAP_object *oleuw, char *fname, char *decode_path, char *stream, size_t bytes )
{
	char *full_name;
	FILE *f;
	int result = 0;

	DUW LOGGER_log("%s:%d:OLEUNWRAP_save_stream:DEBUG: fname=%s, decodepath=%s, size=%ld"
			,FL
			,fname
			,decode_path
			,bytes
			);

	full_name = PLD_dprintf("%s/%s", decode_path, fname );
	if (full_name == NULL)
	{
		LOGGER_log("%s:%d:OLEUNWRAP_save_stream:ERROR: Unable to create filename string from '%s' and '%s'",FL,fname,decode_path);
		return -1;
	}

	f = fopen(full_name,"w");
	if (f != NULL)
	{
		size_t write_count;

		write_count = fwrite( stream, 1, bytes, f );
		if (write_count != bytes)
		{
			LOGGER_log("%s:%d:OLEUNWRAP_save_stream:WARNING: Only wrote %d of %d bytes to file %s\n",FL, write_count, bytes, full_name );
		}

		fclose(f);


	} else {
		LOGGER_log("%s:%d:OLEUNWRAP_save_stream:ERROR: Unable to open %s for writing (%s)\n",FL,full_name, strerror(errno));
		result = -1;
	}

	if (full_name) free(full_name);

	DUW LOGGER_log("%s:%d:OLEUNWRAP_save_stream:DEBUG: Done saving '%s'",FL, fname);

	return result;
}

/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_sanitize_filename
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
int OLEUNWRAP_sanitize_filename( char *fname )
{

	if (fname == NULL) return 0;

		while (*fname)
		{
			if( !isalnum((int)*fname) && (*fname != '.') ) *fname='_';
			if( (*fname < ' ')||(*fname > '~') ) *fname='_';
			fname++;
		}
		return 0;
}

/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_seach_for_file_sig
 Returns Type	: int
 	----Parameter List
	1. char *block , 
 	------------------
 Exit Codes	:  Returns the offset from the block to the 
					start of the signature.
					Returns -1 if not found.

 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_seach_for_file_sig( struct OLEUNWRAP_object *oleuw, char *block, size_t block_len )
{
	int result = -1;
	int hit = 0;
	char *p;		/** signature pointer **/
	char *bp; /** pointer in the block **/
	struct typesig sigs[]= {
		{ "\x89\x50\x4e\x47", 4, 0 }, /** PNG **/
		{ "\xff\xd8\xff", 3, 0 }, /** JPEG **/
		{ NULL, -1, -1 } /** End of array **/
	};

	bp = block;
	block_len -= 4;

	/** While there's more data in the block and we're not found a match **/
	while ((block_len > 0)&&(hit==0)) {
		struct typesig *tsp; /** Type signature pointer **/

		block_len--;

		tsp = sigs; /** tsp points to the first signature in the array **/

		/** While there's more valid signatures in the array **/
		while (tsp->length > 0) {
			int cmpresult = 0;

			p = tsp->sequence; /** set p to point to the start of the image signature sequence **/
			cmpresult = memcmp(bp, p, 3);
			if (cmpresult == 0) {
				DUW LOGGER_log("%s:%d:OLEUNWRAP_seach_for_file_sig:DEBUG: Hit at offset %d for signature %d",FL,(bp-block),(tsp -sigs));
				hit = 1;
				break;
			} /** If we had a match in the signatures **/

			tsp++; /** go to the next signature **/

		} /** While more signatures **/

		if (hit == 0) bp++; /** If we didn't get a hit, move to the next byte in the file **/

	} /** while more data in the block **/

	if (hit == 1) {
		result =  bp -block;
	} else { 
		result = -1;
	}

	return result;
}



	/** Look for PNG signature **/
/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_decode_attachment
 Returns Type	: int
 	----Parameter List
	1. char *stream , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_decode_attachment( struct OLEUNWRAP_object *oleuw, char *stream, size_t stream_size, char *decode_path )
{
	struct OLE10_header oh;
	char *sp = stream;
	char *data_start_point = stream;
	int result = OLEUW_OK;

	// Get the data size
	oh.attach_size_1 = (size_t)get_int32( sp );
	sp += 4;

	DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: attachsize = %d [ 0x%x ], stream length = %d [ 0x%x] \n", FL, oh.attach_size_1, oh.attach_size_1, stream_size, stream_size );

	oh.attach_start_offset = (stream_size -oh.attach_size_1);
	data_start_point = stream +oh.attach_start_offset;

	//if (oh.attach_start_offset == 4)
	if (oh.attach_start_offset < 4)
	{
		int cbheader; // number of bytes in PIC
		int mfpmm;
		int mfpxext;
		int mfpyext;
		int mfphmf;
			
		// check next 4 bytes.

		cbheader = get_uint16( sp );
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: cbHeader = %d [ 0x%x ]", FL, cbheader, cbheader);
		mfpmm = get_uint16( sp +2 );
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: mfp.mm = %d [ 0x%x ]", FL, mfpmm, mfpmm);
		mfpxext = get_uint16( sp +4 );
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: mfp.xext = %d [ 0x%x ]", FL, mfpxext, mfpxext);
		mfpyext = get_uint16( sp +8 );
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: mfp.yext = %d [ 0x%x ]", FL, mfpyext, mfpyext);
		mfphmf = get_uint16( sp +10 );
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: mfp.hmf = %d [ 0x%x ]", FL, mfphmf, mfphmf);
		// If we only had the stream byte-lenght in our header
		//		then we know we don't have a complex header.
		 
		DUW {
		switch (mfpmm) {
			case 100:
				LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Image is Escher format",FL);
				break;
			case 99:
				LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Image is Bitmapped",FL);
				break;
			case 98:
				LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Image is TIFF",FL);
				break;
			default:
				LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Unknown image type for code '%d'",FL, mfpmm);
		}
		}

		data_start_point = sp +cbheader -4;

		
		if (mfpmm == 100) {
			int imageoffset = 0;
			int search_size = 500;

			DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: searcing for image signatures",FL);
			if (stream_size < (search_size +68)) search_size = (stream_size -69); /** just make sure we don't over-search the stream **/

			imageoffset = OLEUNWRAP_seach_for_file_sig(oleuw, data_start_point, search_size);
			if (imageoffset >= 0) {
				DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Image data found at offset %d",FL,imageoffset);
				data_start_point += imageoffset;
			} else {
				DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Could not detect image signature, dumping whole stream",FL);
			}
		}
			
		oh.attach_name = PLD_dprintf("image-%ld",oh.attach_size_1);
		oh.attach_size = oh.attach_size_1;
		oh.fname_1 = oh.fname_2 = NULL;
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Setting attachment name to '%s', size = %d",FL,oh.attach_name, oh.attach_size);
	} else {

		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Decoding file information header",FL);
		// Unknown memory segment
		memcpy( oh.data, sp, 2 );
		sp += 2;
		
		// Full attachment string
		oh.attach_name = strdup( sp );
		sp = sp + strlen(oh.attach_name) +1;

		// Attachment full path
		oh.fname_1 = strdup( sp );
		sp += strlen(oh.fname_1) +1;

		// Unknown memory segment
		memcpy( oh.data2, sp, 8 );
		sp = sp +8;

		// Attachment full path
		oh.fname_2 = strdup( sp );
		sp += strlen(oh.fname_2) +1;

		oh.attach_size = (size_t)get_uint32( sp );
		sp += 4;

		if (oh.attach_size > stream_size) oh.attach_size = stream_size;
		
		data_start_point = sp;
	} 

	DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Attachment %s:%s:%s size = %d\n",FL, oh.attach_name, oh.fname_1, oh.fname_2, oh.attach_size );


	/** 20050119:2053:PLD - Added to sanitize 8-bit filenames **/
	/** Sanitize the output filename **/
	OLEUNWRAP_sanitize_filename(oh.attach_name);
	OLEUNWRAP_sanitize_filename(oh.fname_1);
	OLEUNWRAP_sanitize_filename(oh.fname_2);

	DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Sanitized attachment filenames",FL);

	result = OLEUNWRAP_save_stream( oleuw, oh.attach_name, decode_path, data_start_point, oh.attach_size );
	if (result == OLEUW_OK)
	{
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Calling reporter for the filename",FL);
		if ((oleuw->verbose > 0)&&(oleuw->filename_report_fn != NULL))
		{
			oleuw->filename_report_fn(oh.attach_name);
		}
		// Do call back to reporting function
	}

	DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Cleaning up",FL);
	// Clean up our previously allocated data
	if (oh.fname_1 != NULL) free(oh.fname_1);
	if (oh.attach_name != NULL) free(oh.attach_name);
	if (oh.fname_2 != NULL) free(oh.fname_2);
	
	DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: done.",FL);
	return OLEUW_OK;
}

/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_decodestream
 Returns Type	: int
 	----Parameter List
	1. char *element_string, 
	2.  char *stream , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_decodestream( struct OLEUNWRAP_object *oleuw, char *element_string, char *stream, size_t stream_size, char *decode_path )
{
	int result = OLEUW_OK;

	if (strstr(element_string, OLEUW_ELEMENT_10NATIVE_STRING) != NULL) 
	{
		
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decodestream:DEBUG: Debugging element '%s'",FL, element_string);
		OLEUNWRAP_decode_attachment( oleuw, stream, stream_size, decode_path );

	} else if (strstr(element_string, OLEUW_ELEMENT_DATA) != NULL)  {

		DUW LOGGER_log("%s:%d:OLEUNWRAP_decodestream:DEBUG: Debugging element '%s'",FL, element_string);
		OLEUNWRAP_decode_attachment( oleuw, stream, stream_size, decode_path );

	} else {
		DUW LOGGER_log("%s:%d:OLEUNWRAP_decode_attachment:DEBUG: Unable to decode stream with element string '%s'\n", FL, element_string);
		result = OLEUW_STREAM_NOT_DECODED;
	}

	return result;
}


/*-----------------------------------------------------------------\
 Function Name	: OLEUNWRAP_set_filename_report_fn
 Returns Type	: int
 	----Parameter List
	1. struct OLEUNWRAP_object *oleuw, 
	2.  int (*ptr_to_fn)(char *) , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int OLEUNWRAP_set_filename_report_fn( struct OLEUNWRAP_object *oleuw, int (*ptr_to_fn)(char *) )
{

	oleuw->filename_report_fn = ptr_to_fn;

	return 0;
}

