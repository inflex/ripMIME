
/* Debug levels */
#define MDECODE_DEBUG_PEDANTIC 10
#define MDECODE_DEBUG_NORMAL 1

/* Filename Encoding characters */
#define MDECODE_ISO_ENCODING_Q 'Q'
#define MDECODE_ISO_ENCODING_q 'q'
#define MDECODE_ISO_ENCODING_B 'B'
#define MDECODE_ISO_ENCODING_b 'b'

/* Quoted-Printable decoding modes */
#define MDECODE_QPMODE_STD 0
#define MDECODE_QPMODE_ISO 1


int MDECODE_set_debug( int level );
int MDECODE_set_verbose( int level );
int MDECODE_set_decode_qp( int level );
int MDECODE_set_decode_b64( int level );
int MDECODE_init( void );

int MDECODE_decode_quoted_printable( char *line, int qpmode, char esc_char );
int MDECODE_decode_short64( char *short64 );
int MDECODE_decode_multipart( char *line );
int MDECODE_decode_qp_text( char *line );
int MDECODE_decode_qp_ISO( char *line );
int MDECODE_decode_ISO( char *isostring, int size );

