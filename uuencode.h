#define UUENCODE_STATUS_SHORT_FILE 100
#define UUENCODE_STATUS_CANNOT_OPEN_FILE 101
#define UUENCODE_STATUS_CANNOT_ALLOCATE_MEMORY 102
#define UUENCODE_STATUS_CANNOT_FIND_FILENAME 103
#define UUENCODE_STATUS_OK 0

extern int uuencode_error;

int UUENCODE_init( void );

int UUENCODE_set_debug( int level );
int UUENCODE_set_verbosity( int level );
int UUENCODE_set_verbosity_contenttype( int level );
int UUENCODE_set_nodecode( int level );
int UUENCODE_set_decode( int level );
int UUENCODE_set_doubleCR_mode( int level );

int UUENCODE_set_filename_report_fn( int (*ptr_to_fn)(char *, char *) );

int UUENCODE_is_uuencode_header( char *line );
int UUENCODE_is_file_uuencoded( char *fname );

int UUENCODE_decode_uu( FFGET_FILE *f, char *unpackdir, char *input_filename, char *out_filename, int out_filename_size, int decode_whole_file, int keep );

