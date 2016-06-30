#ifndef MIMEHEADERS
#define MIMEHEADERS

#define MIMEH_VERSION "200812162102"

#define _CTYPE_RANGE 99

#define _CTYPE_UNSPECIFIED -1
#define _CTYPE_MESSAGE_START 1
#define _CTYPE_MESSAGE 1
#define _CTYPE_MESSAGE_END 100

#define _CTYPE_MULTIPART_START 100
#define _CTYPE_MULTIPART 100
#define _CTYPE_MULTIPART_MIXED 101
#define _CTYPE_MULTIPART_APPLEDOUBLE 102
#define _CTYPE_MULTIPART_RELATED 103
#define _CTYPE_MULTIPART_ALTERNATIVE 104
#define _CTYPE_MULTIPART_REPORT 105
#define _CTYPE_MULTIPART_SIGNED 106
#define _CTYPE_MULTIPART_END 199

#define _CTYPE_TEXT_START 200
#define _CTYPE_TEXT 200
#define _CTYPE_TEXT_PLAIN 201
#define _CTYPE_TEXT_UNKNOWN 202
#define _CTYPE_TEXT_HTML 203
#define _CTYPE_TEXT_CALENDAR 204
#define _CTYPE_TEXT_END 299

#define _CTYPE_IMAGE_START 300
#define _CTYPE_IMAGE 300
#define _CTYPE_IMAGE_GIF 301
#define _CTYPE_IMAGE_JPEG 302
#define _CTYPE_IMAGE_PNG 303
#define _CTYPE_IMAGE_END 399

#define _CTYPE_AUDIO_START 400
#define _CTYPE_AUDIO 400
#define _CTYPE_AUDIO_END 499

#define _CTYPE_OCTECT 800
#define _CTYPE_RFC822 500
#define _CTYPE_TNEF 600
#define _CTYPE_APPLICATION 700
#define _CTYPE_APPLICATION_APPLEFILE 701
#define _CTYPE_UNKNOWN 0

#define _CTRANS_ENCODING_UNSPECIFIED -1
#define _CTRANS_ENCODING_B64 100
#define _CTRANS_ENCODING_7BIT 101
#define _CTRANS_ENCODING_8BIT 102
#define _CTRANS_ENCODING_QP 103
#define _CTRANS_ENCODING_RAW 104
#define _CTRANS_ENCODING_BINARY 105
#define _CTRANS_ENCODING_UUENCODE 106
#define _CTRANS_ENCODING_UNKNOWN 0

#define _CDISPOSITION_UNSPECIFIED -1
#define _CDISPOSITION_INLINE 100
#define _CDISPOSITION_ATTACHMENT 200
#define _CDISPOSITION_FORMDATA 300
#define _CDISPOSITION_UNKNOWN 0

#define _MIMEH_FOUND_FROM 100

#define _MIMEH_STRLEN_MAX 1023
#define _MIMEH_FILENAMELEN_MAX 128
#define _MIMEH_CONTENT_TYPE_MAX 128
#define _MIMEH_SUBJECTLEN_MAX 128
#define _MIMEH_CONTENT_DESCRIPTION_MAX 128
#define _MIMEH_CONTENT_TRANSFER_ENCODING_MAX 256
#define _MIMEH_CONTENT_DISPOSITION_MAX 256
#define _MIMEH_DEBUG_NORMAL 1
#define _MIMEH_DEBUG_PEDANTIC 10
#define _MIMEH_DEFECT_ARRAY_SIZE 100
#define _MIMEH_CHARSET_MAX 128

// Errors to throw back
#define MIMEH_ERROR_DISK_FULL 128

// Defects
#define MIMEH_DEFECT_MULTIPLE_QUOTES 1
#define MIMEH_DEFECT_UNBALANCED_QUOTES 2
#define MIMEH_DEFECT_MULTIPLE_EQUALS_SEPARATORS 3
#define MIMEH_DEFECT_MULTIPLE_COLON_SEPARATORS 4
#define MIMEH_DEFECT_MULTIPLE_BOUNDARIES 5
#define MIMEH_DEFECT_UNBALANCED_BOUNDARY_QUOTE 6
#define MIMEH_DEFECT_MULTIPLE_FIELD_OCCURANCE 7
#define MIMEH_DEFECT_MISSING_SEPARATORS 8
#define MIMEH_DEFECT_MULTIPLE_NAMES 9
#define MIMEH_DEFECT_MULTIPLE_FILENAMES 10


struct MIMEH_header_info
{
	char scratch[_MIMEH_STRLEN_MAX +1];
	int content_type;
	char content_type_string[ _MIMEH_CONTENT_TYPE_MAX +1 ];
	char content_description_string[ _MIMEH_CONTENT_DESCRIPTION_MAX +1 ];
	char boundary[_MIMEH_STRLEN_MAX +1];
	int boundary_located;
	char subject[_MIMEH_SUBJECTLEN_MAX +1];
	char filename[_MIMEH_FILENAMELEN_MAX +1];
	char name[_MIMEH_STRLEN_MAX +1];

/** 20041217-1601:PLD: New header fields to keep **/
	char from[_MIMEH_STRLEN_MAX +1];
	char date[_MIMEH_STRLEN_MAX +1];
	char to[_MIMEH_STRLEN_MAX +1];
	char messageid[_MIMEH_STRLEN_MAX +1];
	char received[_MIMEH_STRLEN_MAX +1];
	/** end of new fields **/

	// Store multiple filenames
	struct SS_object ss_filenames;
	// Store multiple names
	struct SS_object ss_names;

	int content_transfer_encoding;
	char content_transfer_encoding_string[ _MIMEH_CONTENT_TRANSFER_ENCODING_MAX +1 ];
	int content_disposition;
	char content_disposition_string[ _MIMEH_CONTENT_DISPOSITION_MAX +1 ];
	//int charset;
	char charset[ _MIMEH_CHARSET_MAX +1 ];
	int format;
	int file_has_uuencode;
	char uudec_name[_MIMEH_FILENAMELEN_MAX +1];	// UUDecode name. This is a post-decode information field.
	int current_recursion_level;

	// Malformed email reporting
	int defects[_MIMEH_DEFECT_ARRAY_SIZE];
	int header_defect_count;

	// Special Exception flags
	int x_mac; // Set if the content type contains x-mac-* entries, which means a filename may contain /'s

	/** Header sanity level - indicates if any of the headers we apparently read are good **/
	int sanity;

/** 20051117-0932:PLD: Will be non-zero if email is MIME **/
	int is_mime;

	char delimeter[3];
	int crlf_count; // 200811151149:PLD: Tally's the number of CRLF lines
	int crcr_count; // 200811151149:PLD: Tally's the number of CRLF lines
	int lf_count; // 200811151149:PLD: Tally's the number of  LF only lines

};


#ifdef RIPMIME_V2XX
struct MIMEH_header_node {
	struct MIMEH_header_info *header_list;
	struct MIMEH_header_node *next;
};


struct MIMEH_email_info {
	char mailpack_name[1024];
	struct MIMEH_header_node *headers;
};
#endif





int MIMEH_version(void);

int MIMEH_init( void );
int MIMEH_set_debug( int level );
int MIMEH_set_verbosity( int level );
int MIMEH_set_verbosity_contenttype( int level );
int MIMEH_get_verbosity_contenttype( void );
int MIMEH_get_headers_sanity(void);

int MIMEH_is_contenttype( int range_type, int content_type );

int MIMEH_set_mailbox( int level );
int MIMEH_set_doubleCR( int level );
int MIMEH_set_doubleCR_save( int level );
int MIMEH_get_doubleCR_save( void );
int MIMEH_set_headerfix( int level );
int MIMEH_set_headers_save( FILE *f );
int MIMEH_set_headers_nosave( void );
int MIMEH_get_headers_save( void );
char *MIMEH_get_headers_ptr( void );

int MIMEH_set_headers_save_original( int level );
char *MIMEH_get_headers_original_ptr( void );

int MIMEH_set_headers_original_save_to_file( FILE *f );

int MIMEH_get_doubleCR( void );
char *MIMEH_get_doubleCR_name( void );

int MIMEH_set_header_longsearch( int level );
int MIMEH_headers_clearcount( struct MIMEH_header_info *hinfo );

int MIMEH_read_headers( struct MIMEH_header_info *hinfo, FFGET_FILE *f );

int MIMEH_headers_get( struct MIMEH_header_info *hinfo, FFGET_FILE *f );
int MIMEH_headers_process( struct MIMEH_header_info *hinfo, char *headers );
int MIMEH_headers_cleanup();
int MIMEH_parse_headers( FFGET_FILE *f, struct MIMEH_header_info *hinfo );


int MIMEH_display_info( struct MIMEH_header_info *hinfo );
int MIMEH_set_webform( int level );
int MIMEH_set_outputdir( char *dir );

int MIMEH_set_defect( struct MIMEH_header_info *hinfo, int defect );
int MIMEH_dump_defects( struct MIMEH_header_info *hinfo );
int MIMEH_get_defect_count( struct MIMEH_header_info *hinfo );

int MIMEH_set_report_MIME( int level );

int MIMEH_read_primary_headers( char *fname, struct MIMEH_header_info *hinfo );

#endif
