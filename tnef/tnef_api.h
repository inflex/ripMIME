// API for external programs wanting to use TNEF decoding
//
#ifndef __TNEF_API__
#define __TNEF_API__

void TNEF_init( void );
int TNEF_main( char *filename, char* file_dir );
int TNEF_set_filename_report_fn( int (*ptr_to_fn)(char *, char *));
int TNEF_set_verbosity( int level );
int TNEF_set_verbosity_contenttype( int level );
int TNEF_set_debug( int level );
int TNEF_set_decode( int level );
#endif
