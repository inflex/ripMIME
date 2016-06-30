
int BS_init( void );
int BS_set_verbose( int level );
int BS_set_debug( int level );
int BS_set_boundary_detect_limit( int limit );
int BS_set_hold_limit( int limit );

int BS_clear( void );
int BS_push( char *boundary );
char *BS_pop( void );
char *BS_top( void );
int BS_cmp( char *boundary, int len );
int BS_count( void );

