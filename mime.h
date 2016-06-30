
#ifndef LIBMIME
#define LIBMIME
/* MIME.h */

#define LIBMIME_VERSION "200811050000"

/* Exit Error codes */
#define _EXITERR_UNDEFINED_BOUNDARY 100
#define _EXITERR_PRINT_QUOTABLE_INPUT_NOT_OPEN 200
#define _EXITERR_PRINT_QUOTABLE_OUTPUT_NOT_OPEN 201
#define _EXITERR_BASE64_OUTPUT_NOT_OPEN 210
#define _EXITERR_BASE64_UNABLE_TO_OUTPUT 211
#define _EXITERR_MIMEREAD_CANNOT_OPEN_OUTPUT 220
#define _EXITERR_MIMEREAD_CANNOT_WRITE_OUTPUT 221
#define _EXITERR_MIMEUNPACK_CANNOT_OPEN_INPUT_FILE 230
#define _EXITERR_MIMEUNPACK_CANNOT_OPEN_HEADERS_FILE 231

#define MIME_ERROR_RECURSION_LIMIT_REACHED 240
#define MIME_ERROR_FFGET_EMPTY 241
#define MIME_ERROR_B64_INPUT_STREAM_EOF 242

#define _MIME_RENAME_METHOD_INFIX 1
#define _MIME_RENAME_METHOD_PREFIX 2
#define _MIME_RENAME_METHOD_POSTFIX 3

#define _MIME_STRLEN_MAX 1023

/* Debug levels */
#define _MIME_DEBUG_PEDANTIC 10
#define _MIME_DEBUG_NORMAL 1

/* Filename Encoding characters */
#define MIME_ISO_ENCODING_Q 'Q'
#define MIME_ISO_ENCODING_q 'q'
#define MIME_ISO_ENCODING_B 'B'
#define MIME_ISO_ENCODING_b 'b'

#define MIME_BLANKZONE_SAVE_TEXTFILE 4
#define MIME_BLANKZONE_SAVE_FILENAME 8

/* Quoted-Printable decoding modes */
#define MIME_QPMODE_STD 0
#define MIME_QPMODE_ISO 1

/* BASE64 decode status return codes */
#define MIME_BASE64_STATUS_HIT_BOUNDARY 1
#define MIME_BASE64_STATUS_ZERO_FILE 2
#define MIME_BASE64_STATUS_OK 0

/* status return codes */
#define MIME_STATUS_ZERO_FILE 100

int MIME_version( void );
size_t MIME_read_raw( char *src_mpname, char *dest_mpname, size_t rw_buffer_size );
int MIME_read( char *mpname ); /* returns filesize in KB */
int MIME_unpack( char *unpackdir, char *mpname, int current_recusion_level );
//int MIME_unpack_single( char *unpackdir, char *mpname, int current_recusion_level );
//int MIME_unpack_single_fp( char *unpackdir, FILE *fi, int current_recusion_level );
//int MIME_unpack_mailbox( char *unpackdir, char *mpname, int current_recursion_level );
int MIME_insert_Xheader( char *fname, char *xheader );
int MIME_set_blankfileprefix( char *prefix );
int MIME_set_recursion_level(int level);
int MIME_set_verbosity( int level );
int MIME_set_verbosity_contenttype( int level );
int MIME_set_verbosity_12x_style( int level );
int MIME_set_verbose_defects( int level );
int MIME_set_report_MIME( int level );

int MIME_set_quiet( int level );
int MIME_set_filename_report_fn( int (*ptr_to_fn)(char *, char *) );
int MIME_set_debug( int level );
int MIME_set_dumpheaders( int level );
int MIME_set_headersname( char *fname );

int MIME_set_decode_uudecode( int level );
int MIME_set_decode_tnef( int level );
int MIME_set_decode_ole( int level );
int MIME_set_decode_qp( int level );
int MIME_set_decode_base64( int level );
int MIME_set_decode_doubleCR( int level );
int MIME_set_decode_mht( int level );

int MIME_set_header_longsearch( int level );

int MIME_set_blankzone_save_option( int option );
int MIME_set_blankzone_filename( char *filename );
char *MIME_get_blankzone_filename( void );

int MIME_set_syslogging( int level );
int MIME_set_stderrlogging( int level );
int MIME_set_no_nameless( int level );
int MIME_set_uniquenames( int level );
int MIME_set_renamemethod( int method );
int MIME_set_paranoid( int level );
int MIME_set_mailboxformat( int level );
int MIME_set_webform( int level );
int MIME_get_attachment_count( void );
int MIME_set_name_by_type( int level );
int MIME_set_multiple_filenames( int level );
int MIME_get_header_defect_count( void );

int MIME_is_file_mime( char *fname );
int MIME_is_file_uuenc( char *fname );


char *MIME_get_blankfileprefix( void );
char *MIME_get_headersname( void );
char *MIME_get_subject( void );
int MIME_init( void );
int MIME_close( void );
int MIME_set_tmpdir( char *tmpdir );
//int MIME_postdecode_cleanup( char *unpackdir, struct SS_object *ss );

//int MIME_decode_TNEF( FILE *f, char *unpackdir, struct _header_info *hinfo, int keep );

#endif
