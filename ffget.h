
#ifndef __FFGET__
#define __FFGET__

/* DEFINES */
#ifndef FL
#define FL __FILE__,__LINE__
#endif

#define FFGET_VERSION	"1.0.0.8"
#define FFGET_LASTUPDATED "200811061423"

#define FFGET_DNORMAL   ((FFGET_debug >= FFGET_DEBUG_NORMAL  ))
#define FFGET_DPEDANTIC ((FFGET_debug >= FFGET_DEBUG_PEDANTIC))

#define FFGET_MAX_LINE_LEN 1024
#define FFGET_BUFFER_MAX 8192
#define FFGET_BUFFER_PADDING 1

#define FFGET_DEBUG_NORMAL 1
#define FFGET_DEBUG_PEDANTIC 10

#define FFGET_LINEBREAK_NONE 0
#define FFGET_LINEBREAK_LF 1
#define FFGET_LINEBREAK_CR 2

struct _FFGET_FILE
{
	FILE *f;
	char buffer[FFGET_BUFFER_MAX+4];
	char *startpoint;
	char *endpoint;
	char *buffer_end;
	size_t last_block_read_from;
	int FILEEND;
	int FFEOF;
	char c;
	unsigned long int bytes;
	unsigned long int linecount;
	int ungetcset;
	int trueblank;
	char lastchar;
	int linebreak;
	char lastbreak[10];


};

typedef struct _FFGET_FILE FFGET_FILE;

// Special Flag to indicate a Double CR Line.
extern int FFGET_doubleCR;
extern int FFGET_SDL_MODE;  // Single Char Delimeter
extern char SDL_MODE_DELIMITS[];
extern char NORM_MODE_DELIMITS[];
extern char *DELIMITERS;



int FFGET_setstream( FFGET_FILE *f, FILE *fi );
#ifdef sgi
short FFGET_fgetc( FFGET_FILE *f );
#else
char FFGET_fgetc( FFGET_FILE *f );
#endif
int FFGET_closestream( FFGET_FILE *f );
int FFGET_ungetc( FFGET_FILE *f, char c );
int FFGET_presetbuffer( FFGET_FILE *f, char *buffer, int size );
char *FFGET_fgets( char *linein, int max_size, FFGET_FILE *f );
int FFGET_raw( FFGET_FILE *f, unsigned char *buffer, int max );
int FFGET_feof( FFGET_FILE *f );
int FFGET_getnewblock( FFGET_FILE *f );
int FFGET_set_watch_SDL( int level );
int FFGET_set_allow_nul(int level );

long FFGET_ftell( FFGET_FILE *f );
int FFGET_fseek( FFGET_FILE *f, long offset, int whence );


#endif
