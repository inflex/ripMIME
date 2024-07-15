/*-----------------------------------------------------------------------
 **
 **
 ** mime.c
 **
 ** Written by Paul L Daniels, originally for the Xamime project
 ** (http://www.xamime.com) but since spawned off to the ripMIME/alterMIME
 ** family of email parsing tools.
 **
 ** Copyright PLD, 1999,2000,2001,2002,2003
 ** Licence: BSD
 ** For more information on the licence and copyrights of this code, please
 ** email copyright@pldaniels.com

 ** CHANGES
 ** 2003-Jun-24: PLD: Added subject retaining in the global struct
 **     this is useful for when you want to retrieve such information
 **     from an external application - without having to dive into
 **     the hinfo struct directly.  Also, the subject is retained only
 **     for the /primary/ headers, all subsequent headers which [may]
 **     contain 'subject:' are ignored.
 **
 ** 2020-Aug-05 ad-pro: Add new function to generate file names:
 **    randinfix, randprefix, randpostfix
 **    add random number after the counter.
 **    I have found a bug, that ripmime override extracted files if
 **    few copies are started for the same mail.
 **
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#ifdef MEMORY_DEBUG
#define DEBUG_MEMORY 1
#include "xmalloc.h"
#endif

#include "pldstr.h"
#include "boundary-stack.h"
#include "ffget.h"
#include "mime.h"
#include "tnef/tnef_api.h"
#include "ripOLE/ole.h"
#include "libmime-decoders.h"
#include "uuencode.h"
#include "filename-filters.h"
#include "logger.h"
#include "strstack.h"

#include "MIME_headers.h"

int MIME_unpack_stage2( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *hinfo, int current_recursion_level, struct SS_object *ss );
int MIME_unpack_single( char *unpackdir, char *mpname, int current_recursion_level, struct SS_object *ss );
int MIME_unpack_single_fp( char *unpackdir, FILE *fi, int current_recursion_level, struct SS_object *ss );
int MIME_unpack_mailbox( char *unpackdir, char *mpname, int current_recursion_level, struct SS_object *ss );
int MIME_handle_multipart( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *h, int current_recursion_level, struct SS_object *ss );
int MIME_handle_rfc822( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *h, int current_recursion_level, struct SS_object *ss );

// Predefined filenames
#define MIME_BLANKZONE_FILENAME_DEFAULT "_blankzone_"
#define MIME_HEADERS_FILENAME "_headers_"

#ifndef FL
#define FL __FILE__, __LINE__
#endif

#define _ENC_UNKNOWN 0
#define _ENC_BASE64 1
#define _ENC_PLAINTEXT 2
#define _ENC_QUOTED 3
#define _ENC_EMBEDDED 4
#define _ENC_NOFILE -1
#define _MIME_CHARS_PER_LINE 32
#define _MIME_MAX_CHARS_PER_LINE 76
#define _RECURSION_LEVEL_DEFAULT 20

#define _BOUNDARY_CRASH 2

// BASE64 / UUDEC and other binary writing routines use the write buffer (now in v1.2.16.3+)
//  The "limit" define is a check point marker which indicates that on our next run through
//  either the BASE64 or UUDEC routines, we need to flush the buffer to disk

#define MIME_MIME_READ_BUFFER_SIZE (8 *1024)
#define _MIME_WRITE_BUFFER_SIZE (8 *1024)
#define _MIME_WRITE_BUFFER_LIMIT (_MIME_WRITE_BUFFER_SIZE -4)



// Debug precodes
#define MIME_DPEDANTIC ((glb.debug >= _MIME_DEBUG_PEDANTIC))
#define MIME_DNORMAL   ((glb.debug >= _MIME_DEBUG_NORMAL  ))
#define MIME_VERBOSE      ((glb.verbosity > 0 ))
#define MIME_VERBOSE_12   ((glb.verbosity_12x_style > 0 ))
#define DMIME if (glb.debug >= _MIME_DEBUG_NORMAL)

#define FL __FILE__,__LINE__

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




struct MIME_globals {

    int header_defect_count;
    int filecount;
    char blankfileprefix[_MIME_STRLEN_MAX];
    int blankfileprefix_expliticly_set;
    int verbosity;
    int verbosity_12x_style;
    int verbosity_contenttype;
    int verbose_defects;
    int report_mime;
    int debug;
    int quiet;
    int syslogging;
    int stderrlogging;
    int unique_names;
    int rename_method;
    char headersname[_MIME_STRLEN_MAX];
    char tempdirectory[_MIME_STRLEN_MAX];
    int save_headers;
    int attachment_count;
    int current_line;
    int no_nameless;
    int name_by_type;
    int mailbox_format;

    int decode_uu;
    int decode_tnef;
    int decode_b64;
    int decode_qp;
    int decode_mht;
    int decode_ole;

    int multiple_filenames;

    int header_longsearch;
    int max_recursion_level;

    int blankzone_saved;
    int blankzone_save_option;
    char blankzone_filename[_MIMEH_STRLEN_MAX +1];

    int (*filename_decoded_reporter)(char *, char *); // Pointer to the reporting function for filenames as they are decoded

    // First subject is an important item, because this
    //      represents the "main" subject of the email, as
    //      seen by the MUA.  We have to store this locally
    //      rather than rely purely on hinfo, because other
    //      wise, any consequent parsing of sub-message bodies
    //      will result in the clobbering of the hinfo struct
    char subject[_MIME_STRLEN_MAX];
};

static struct MIME_globals glb;

//char OK[]="OKAY";
static char scratch[1024];

/* File pointer for the headers output */
FILE *headers;

/*-----------------------------------------------------------------\
  Function Name : MIME_version
  Returns Type  : int
  ----Parameter List
  1. void ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_version( void )
{
    fprintf(stderr,"ripMIME: %s\n", LIBMIME_VERSION);
    MIMEH_version();
#ifdef RIPOLE
    OLE_version();
#endif

    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_name_by_type
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:
Used to set (on/off) the name-by-type feature, which will
use the content-type paramter to name the attachment rather
than just the default textfile* naming

0 = don't name by type
1 = name by type.

We may consider adding more levels to this to account for
the different types of content-types.

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_name_by_type( int level )
{
    glb.name_by_type = level;

    return glb.name_by_type;
}

/*------------------------------------------------------------------------
Procedure:     MIME_set_debug ID:1
Purpose:       Sets the debug level for reporting in MIME
Input:         int level : What level of debugging to use, currently there
are only two levels, 0 = none, > 0 = debug info
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_set_debug( int level )
{
    glb.debug = level;

    BS_set_debug(level);
    TNEF_set_debug(level);
    MIMEH_set_debug(level);
    MDECODE_set_debug(level);
    UUENCODE_set_debug(level);
    FNFILTER_set_debug(level);
    return glb.debug;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_quiet
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_quiet( int level )
{
    glb.quiet = level;
    MIME_set_debug(0);
    MIME_set_verbosity(0);
    return glb.quiet;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_recursion_level
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_recursion_level( int level )
{
    glb.max_recursion_level = level;
    return glb.max_recursion_level;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_decode_tnef
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_decode_tnef( int level )
{
    glb.decode_tnef = level;
    TNEF_set_decode( level );

    return level;
}
/*-----------------------------------------------------------------\
  Function Name : MIME_set_decode_ole
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_decode_ole( int level )
{
    glb.decode_ole = level;

    return level;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_decode_uudecode
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_decode_uudecode( int level )
{
    glb.decode_uu = level;
    UUENCODE_set_decode( level );

    return glb.decode_uu;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_decode_base64
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_decode_base64( int level )
{
    glb.decode_b64 = level;

    return glb.decode_b64;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_decode_qp
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_decode_qp( int level )
{
    glb.decode_qp = level;
    MDECODE_set_decode_qp(level);

    return glb.decode_qp;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_decode_doubleCR
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_decode_doubleCR( int level )
{
    MIMEH_set_doubleCR_save(level); /** 20041106-0859:PLD: Was '0' **/

    return MIMEH_get_doubleCR_save();
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_decode_mht
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_decode_mht( int level )
{
    glb.decode_mht = level;
    return glb.decode_mht;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_header_longsearch
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_header_longsearch( int level )
{
    glb.header_longsearch = level;
    return glb.header_longsearch;
}

/*------------------------------------------------------------------------
Procedure:     MIME_set_tmpdir ID:1
Purpose:       Sets the internal Temporary directory name.
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_set_tmpdir( char *setto )
{
    PLD_strncpy(glb.tempdirectory,setto, _MIME_STRLEN_MAX);
    return 0;
}

/*------------------------------------------------------------------------
Procedure:     MIME_set_glb.blankfileprefix ID:1
Purpose:       Sets the filename prefix which is to be used for any files which are saved and do not have a defined filename.
Input:         char *prefix : \0 terminated character array defining the filename prefix
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_set_blankfileprefix( char *prefix )
{
    PLD_strncpy( glb.blankfileprefix, prefix, _MIME_STRLEN_MAX );
    glb.blankfileprefix_expliticly_set = 1;
    return 0;
}

/*------------------------------------------------------------------------
Procedure:     MIME_get_glb.blankfileprefix ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
char *MIME_get_blankfileprefix( void )
{
    return glb.blankfileprefix;
}

/*-------------------------------------------------------
 * MIME_setglb.verbosity
 *
 * By default, MIME reports nothing as its working
 * Setting the verbosity level > 0 means that it'll
 * report things like the name of the files it's
 * writing/extracting.
 *
 */
int MIME_set_verbosity( int level )
{
    glb.verbosity = level;
    MIMEH_set_verbosity( level );
    TNEF_set_verbosity( level );
    FNFILTER_set_verbose( level );
    UUENCODE_set_verbosity( level );
    MDECODE_set_verbose( level );
    BS_set_verbose( level );
    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_verbosity_12x_style
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_verbosity_12x_style( int level )
{
    glb.verbosity_12x_style = level;
    MIME_set_verbosity( level );
    return level;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_verbosity_contenttype
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_verbosity_contenttype( int level )
{
    glb.verbosity_contenttype = level;
    MIMEH_set_verbosity_contenttype( level );
    UUENCODE_set_verbosity_contenttype( level );
    TNEF_set_verbosity_contenttype( level );
    return glb.verbosity_contenttype;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_verbose_defects
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_verbose_defects( int level )
{
    glb.verbose_defects = level;
    return glb.verbose_defects;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_report_MIME
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_report_MIME( int level )
{
    glb.report_mime = level;
    /**MIMEH_set_report_MIME(glb.report_mime); - not yet implemented **/
    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_filename_report_fn
  Returns Type  : int
  ----Parameter List
  1. int (*ptr_to_fn)(char *,
  2.  char *) ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_filename_report_fn( int (*ptr_to_fn)(char *, char *) )
{
    glb.filename_decoded_reporter = ptr_to_fn;
    UUENCODE_set_filename_report_fn( ptr_to_fn );
    TNEF_set_filename_report_fn( ptr_to_fn );

    return 0;
}

/*-------------------------------------------------------
 * MIME_set_dumpheaders
 *
 * By default MIME wont dump the headers to a text file
 * but at times this is useful esp for checking
 * for new styles of viruses like the KAK.worm
 *
 * Anything > 0 will make the headers be saved
 *
 */
int MIME_set_dumpheaders( int level )
{

    glb.save_headers = level;

    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_multiple_filenames
  Returns Type  : int
  ----Parameter List
  1. int level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_multiple_filenames( int level )
{
    glb.multiple_filenames = level;

    return 0;
}

/*------------------------------------------------------
 * MIME_set_headersname
 *
 * by default, the headers would be dropped to a file
 * called '_headers_'.  Using this call we can make
 * it pretty much anything we like
 */
int MIME_set_headersname( char *fname )
{
    PLD_strncpy(glb.headersname, fname, _MIME_STRLEN_MAX);
    return 0;
}

/*------------------------------------------------------------------------
Procedure:     MIME_get_headersname ID:1
Purpose:       Returns a pointer to the current glb.headersname string.
Input:
Output:
Errors:
------------------------------------------------------------------------*/
char *MIME_get_headersname( void )
{
    return glb.headersname;
}

/*-----------------------------------------------------------------\
  Function Name : *MIME_get_subject
  Returns Type  : char
  ----Parameter List
  1. void ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
char *MIME_get_subject( void )
{
    return glb.subject;
}

#ifdef RIPMIME_BLANKZONE
/* The blankzone functions are responsbile for telling ripMIME what to do
    about the block of data which resides between the first/main headers
    and the first attachment/block of a MIME encoded email.

    Normally this would come out as textfile0, -except- for non-MIME
    encoded emails which would actually have their real body saved as
    textfile0.

    There are potential kludge options for doing this, but I am going to try
    and do it right.
    */
int MIME_set_blankzone_save_option( int option )
{
    switch ( option ) {
        case MIME_BLANKZONE_SAVE_TEXTFILE:
            glb.blankzone_save_option = option;
            break;

        case MIME_BLANKZONE_SAVE_FILENAME:
            glb.blankzone_save_option = option;
            snprintf( glb.blankzone_filename, _MIME_STRLEN_MAX, "%s", MIME_BLANKZONE_FILENAME_DEFAULT );
            break;

        default:
            LOGGER_log("%s:%d:MIME_set_blankzone_save_option:WARNING: Unknown option for saving method (%d). Setting to '%s'",FL, option, MIME_BLANKZONE_FILENAME_DEFAULT );
            glb.blankzone_save_option = MIME_BLANKZONE_SAVE_FILENAME;
            snprintf( glb.blankzone_filename, _MIME_STRLEN_MAX, "%s", MIME_BLANKZONE_FILENAME_DEFAULT );
    }
    return glb.blankzone_save_option;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_set_blankzone_filename
  Returns Type  : int
  ----Parameter List
  1. char *filename ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_set_blankzone_filename( char *filename )
{
    PLD_strncpy( glb.blankzone_filename, filename, _MIME_STRLEN_MAX);
    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : *MIME_get_blankzone_filename
  Returns Type  : char
  ----Parameter List
  1. void ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
char *MIME_get_blankzone_filename( void )
{
    return glb.blankzone_filename;
}
#endif

/*------------------------------------------------------------------------
Procedure:     MIME_setglb.no_nameless ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_set_no_nameless( int level )
{
    glb.no_nameless = level;
    return 0;
}

/*------------------------------------------------------------------------
Procedure:     MIME_set_uniquenames ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_set_uniquenames( int level )
{
    glb.unique_names = level;
    return 0;
}

/*------------------------------------------------------------------------
Procedure:     MIME_set_noparanoid ID:1
Purpose:       If set, will prevent MIME from clobbering what it considers
to be non-safe characters in the file name.
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_set_paranoid( int level )
{
    FNFILTER_set_paranoid( level );
    return level;
}

/*------------------------------------------------------------------------
Procedure:     MIME_set_mailboxformat ID:1
Purpose:       If sets the value for the _mailboxformat variable
in MIME, this indicates to functions later on
that they should be aware of possible mailbox
format specifiers.
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_set_mailboxformat( int level )
{
    glb.mailbox_format = level;
    MIMEH_set_mailbox( level );
    return 0;
}

/*------------------------------------------------------------------------
Procedure:     MIME_set_renamemethod ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_set_renamemethod( int method )
{
    if (( method >= _MIME_RENAME_METHOD_INFIX ) && ( method <= _MIME_RENAME_METHOD_RANDPOSTFIX ))
    {
        glb.rename_method = method;
    }
    else
    {
        LOGGER_log("%s:%d:MIME_set_renamemethod:ERROR: selected method not within %d > x > %d range", FL, _MIME_RENAME_METHOD_INFIX, _MIME_RENAME_METHOD_POSTFIX );
        return -1;
    }

    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_get_header_defect_count
  Returns Type  : int
  ----Parameter List
  1. void ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_get_header_defect_count( void )
{
    return glb.header_defect_count;
}

/*------------------------------------------------------------------------
Procedure:     MIME_getglb.attachment_count ID:1
Purpose:
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_get_attachment_count( void )
{
    return glb.attachment_count;
}

int get_random_value(void) {
    int randval;
    FILE *fp;
    fp = fopen("/dev/urandom", "r");
    fread(&randval, sizeof(randval), 1, fp);
    fclose(fp);
    if (randval < 0) { randval = randval *( -1); };
    return randval;
}

/*------------------------------------------------------------------------
Procedure:     MIME_test_uniquename ID:1
Purpose:       Checks to see that the filename specified is unique. If it's not
unique, it will modify the filename
Input:         char *path: Path in which to look for similar filenames
char *fname: Current filename
int method: Method of altering the filename (infix, postfix, prefix, randinfix, randpostfix, randprefix)
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_test_uniquename( char *path, char *fname, int method )
{
    struct stat buf;

    char newname[_MIME_STRLEN_MAX +1];
    char scr[_MIME_STRLEN_MAX +1]; /** Scratch var **/
    char *frontname, *extention;

    int cleared = 0;
    int count = 1;

    int randval = get_random_value();

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_test_uniquename:DEBUG: Start (%s)",FL,fname);

    frontname = extention = NULL;  // shuts the compiler up

    if (method == _MIME_RENAME_METHOD_INFIX)
    {
        PLD_strncpy(scr,fname, _MIMEH_STRLEN_MAX);
        frontname = scr;
        extention = strrchr(scr,'.');

        if (extention)
        {
            *extention = '\0';
            extention++;
        }
        else
        {
            method = _MIME_RENAME_METHOD_POSTFIX;
        }
    }

  if (method == _MIME_RENAME_METHOD_RANDINFIX)
    {
        PLD_strncpy(scr,fname, _MIMEH_STRLEN_MAX);
        frontname = scr;
        extention = strrchr(scr,'.');

        if (extention)
        {
            *extention = '\0';
            extention++;
        }
        else
        {
            method = _MIME_RENAME_METHOD_RANDPOSTFIX;
        }
    }

    snprintf(newname,_MIME_STRLEN_MAX,"%s/%s",path,fname);
    while (!cleared)
    {
        if ((stat(newname, &buf) == -1))
        {
            cleared++;
        }
        else
        {
            if (method == _MIME_RENAME_METHOD_PREFIX)
            {
                snprintf(newname,_MIME_STRLEN_MAX,"%s/%d_%s",path,count,fname);
            }
            else
                if (method == _MIME_RENAME_METHOD_INFIX)
                {
                    snprintf(newname,_MIME_STRLEN_MAX,"%s/%s_%d.%s",path,frontname,count,extention);
                }
                else
                    if (method == _MIME_RENAME_METHOD_POSTFIX)
                    {
                        snprintf(newname,_MIME_STRLEN_MAX,"%s/%s_%d",path,fname,count);
                    }
                    else
                        /* Handle randome methots*/
                        if (method == _MIME_RENAME_METHOD_RANDPREFIX)
                        {
                            snprintf(newname,_MIME_STRLEN_MAX,"%s/%d_%d_%s",path,count,randval,fname);
                        }
                        else
                            if (method == _MIME_RENAME_METHOD_RANDINFIX)
                            {
                                snprintf(newname,_MIME_STRLEN_MAX,"%s/%s_%d_%d.%s",path,frontname,count,randval,extention);
                            }
                            else
                                if (method == _MIME_RENAME_METHOD_RANDPOSTFIX)
                                {
                                    snprintf(newname,_MIME_STRLEN_MAX,"%s/%s_%d_%d",path,fname,count,randval);
                                }
            count++;
        }
    }
    if (count > 1)
    {
        frontname = strrchr(newname,'/');
        if (frontname) frontname++;
        else frontname = newname;

        PLD_strncpy(fname, frontname, _MIMEH_FILENAMELEN_MAX); //FIXME - this assumes that the buffer space is at least MIME_STRLEN_MAX sized.
    }
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_test_uniquename:DEBUG: Done (%s)",FL,fname);
    return 0;
}

/*------------------------------------------------------------------------
Procedure:     MIME_is_file_mime ID:1
Purpose:       Determines if the file handed to it is a MIME type email file.

Input:         file name to analyze
Output:        Returns 0 for NO, 1 for YES, -1 for "Things di
Errors:
------------------------------------------------------------------------*/
int MIME_is_file_RFC822( char *fname )
{
    char conditions[16][16] = {
        "Received: ", "From: ", "Subject: ", "Date: ", "Content-", "content-", "from: ", "subject: ", "date: ", "boundary=", "Boundary=", "MIME-Version"        };
    int result = 0;
    int flag_mime_version = 0;
    int hitcount = 0;
    int linecount = 100; // We should only need to read the first 10 lines of any file.
    char *line;
    FILE *f;

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_is_file_RFC822:DEBUG: Testing %s for RFC822 headers",FL,fname);

    f = fopen(fname,"r");
    if (!f)
    {
        if (glb.quiet == 0)
        {
            LOGGER_log("%s:%d:MIME_is_file_mime:ERROR: cannot open file '%s' for reading (%s)", FL, fname,strerror(errno));
        }
        return 0;
    }

    line = malloc(sizeof(char) *1025);
    if (!line)
    {
        LOGGER_log("%s:%d:MIME_is_file_mime:ERROR: cannot allocate memory for read buffer", FL);
        return 0;
    }

    while (((!flag_mime_version)||(hitcount < 2))&&(fgets(line,1024,f))&&(linecount--))
    {
        /** test every line for possible headers, until we get a blank line **/

        if ((glb.header_longsearch == 0)&&(*line == '\n' || *line == '\r')) break;

        for (result = 0; result < 12; result++)
        {
            /** Test for every possible MIME header prefix, ie, From: Subject: etc **/
            if (MIME_DPEDANTIC) LOGGER_log("%s:%d:MIME_is_file_mime:DEBUG: Testing for '%s' in '%s'", FL, line, conditions[result]);
            if (strncasecmp(line,conditions[result],strlen(conditions[result]))==0)
            {
                /** If we get a match, then increment the hit counter **/
                hitcount++;
                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_is_file_RFC822:DEBUG: Hit on %s",FL,conditions[result]);
                if (result == 11)
                {
                    flag_mime_version = 1;
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_is_file_RFC822:DEBUG: Find 'MIME Version' field",FL);
                }
            }
        }
    }

    fclose(f);
    if (hitcount >= 2 && flag_mime_version) result = 1;
    else result = 0;
    if (line) free(line);
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_is_file_RFC822:DEBUG: Hit count = %d, result = %d",FL,hitcount,result);
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_getchar_start ID:1
Purpose:       This function is used on a once-off basis. It's purpose is to locate a
non-whitespace character which (in the context of its use) indicates
that the commencement of BASE64 encoding data has commenced.
Input:         FFGET f: file stream
Output:        First character of the BASE64 encoding.
Errors:
------------------------------------------------------------------------*/
int MIME_getchar_start( FFGET_FILE *f )
{
    int c;
    /* loop for eternity, as we're "returning out" */
    while (1)
    {
        /* get a single char from file */
        c = FFGET_fgetc(f);
        /* if that char is an EOF, or the char is something beyond
         * ASCII 32 (space) then return */
        if ((c == EOF) || (c > ' '))
        {
            return c;
        }
    } /* while */
    /* Although we shouldn't ever actually get here, we best put it in anyhow */
    return EOF;
}

/*------------------------------------------------------------------------
Procedure:     MIME_decode_TNEF ID:1
Purpose:       Decodes TNEF encoded attachments
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_decode_TNEF( char *unpackdir, struct MIMEH_header_info *hinfo, int keep )
{
    int result=0;
    char fullpath[1024];

    snprintf(fullpath,sizeof(fullpath),"%s/%s",unpackdir,hinfo->filename);
    TNEF_set_path(unpackdir);
    result = TNEF_main( fullpath );

    if (result >= 0)
    {
        //      result = remove( fullpath );
        if (result == -1)
        {
            if (MIME_VERBOSE) LOGGER_log("%s:%d:MIME_decode_TNEF: Removing %s failed (%s)",FL,fullpath,strerror(errno));
        }
    }
    return result;
}

#ifdef RIPOLE
int MIME_report_filename_decoded_RIPOLE(char *filename)
{
    LOGGER_log("Decoding filename=%s", filename);
    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_decode_OLE
  Returns Type  : int
  ----Parameter List
  1. char *unpackdir,
  2.  struct MIMEH_header_info *hinfo,
  3.  int keep ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_decode_OLE( char *unpackdir, struct MIMEH_header_info *hinfo, int keep )
{
    struct OLE_object ole;
    char fullpath[1024];
    int result;

    snprintf(fullpath,sizeof(fullpath),"%s/%s",unpackdir,hinfo->filename);

    OLE_init(&ole);
    OLE_set_quiet(&ole,glb.quiet);
    OLE_set_verbose(&ole,glb.verbosity);
    OLE_set_debug(&ole,glb.debug);
    OLE_set_save_unknown_streams(&ole,0);
    OLE_set_filename_report_fn(&ole, MIME_report_filename_decoded_RIPOLE );

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_OLE:DEBUG: Starting OLE Decode",FL);
    result = OLE_decode_file(&ole, fullpath, unpackdir );
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_OLE:DEBUG: Decode done, cleaning up.",FL);
    OLE_decode_file_done(&ole);

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_OLE:DEBUG: Decode returned with code = %d",FL,result);
    return result;
}
#endif

/*------------------------------------------------------------------------
Procedure:     MIME_decode_raw ID:1
Purpose:       Decodes a binary type attachment, ie, no encoding, just raw data.
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_decode_raw( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *hinfo, int keep )
{
    int result = 0;
    char fullpath[1024];
    int bufsize=1024;
    char *buffer = malloc((bufsize +1)*sizeof(char));
    size_t readcount;
    int file_has_uuencode = 0;
    int decode_entire_file = 0;
    int fo;

    /* Decoding / reading a binary attachment is a real interesting situation, as we
     * still use the fgets() call, but we do so repeatedly until it returns a line with a
     * \n\r and the boundary specifier in it.... all in all, I wouldn't rate this code
     * as being perfect, it's activated only if the user intentionally specifies it with
     * --binary flag
     */

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: Start\n",FL);

    snprintf(fullpath,sizeof(fullpath),"%s/%s",unpackdir,hinfo->filename);
    fo = open(fullpath, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP);

    if (fo == -1)
    {
        LOGGER_log("%s:%d:MIME_decode_raw:ERROR: cannot open file %s for writing. (%s)\n\n",FL,fullpath,strerror(errno));
        return -1;
    }

    while ((readcount=FFGET_raw(f, (unsigned char *) buffer,bufsize)) > 0)
    {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: BUFFER[%p]= '%s'\n",FL,buffer, buffer);

        if ((!file_has_uuencode)&&(UUENCODE_is_uuencode_header( buffer )))
        {
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: UUENCODED is YES (buffer=[%p]\n",FL,buffer);

            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: File contains UUENCODED data(%s)\n",FL,buffer);

            file_has_uuencode = 1;
        }

        if (BS_cmp(buffer, readcount))
        {
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: Boundary located - breaking out.\n",FL);
            break;
        } else {
            size_t bc;
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: writing: %s\n",FL, buffer);
            bc = write( fo, buffer, readcount);
        }
    }

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: Completed reading RAW data\n",FL);
    free(buffer);
    close(fo);
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: Closed file and free'd buffer\n",FL);
    // If there was UUEncoded portions [potentially] in the email, the
    // try to extract them using the MIME_decode_uu()
    if (file_has_uuencode)
    {
        char full_decode_path[512];
        snprintf(full_decode_path,sizeof(full_decode_path),"%s/%s",unpackdir,hinfo->filename);
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: Decoding UUencoded data\n",FL);
        if ( hinfo->content_transfer_encoding == _CTRANS_ENCODING_UUENCODE ) decode_entire_file = 0;

        //result = UUENCODE_decode_uu(NULL, unpackdir, hinfo->filename, hinfo->uudec_name, sizeof(hinfo->uudec_name), decode_entire_file, keep );
        result = UUENCODE_decode_uu(NULL, unpackdir, full_decode_path, hinfo->uudec_name, sizeof(hinfo->uudec_name), decode_entire_file, keep );
        if (result == -1)
        {
            switch (uuencode_error) {
                case UUENCODE_STATUS_SHORT_FILE:
                case UUENCODE_STATUS_CANNOT_OPEN_FILE:
                case UUENCODE_STATUS_CANNOT_FIND_FILENAME:
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: Nullifying uuencode_error result %d",FL, uuencode_error);
                    result = 0;
                    break;

                case UUENCODE_STATUS_CANNOT_ALLOCATE_MEMORY:
                    LOGGER_log("%s:%d:MIME_decode_raw:ERROR: Failure to allocate memory for UUdecode process",FL);
                    return -1;
                    break;

                default:
                    LOGGER_log("%s:%d:MIME_decode_raw:ERROR: Unknown return code from UUDecode process [%d]",FL,uuencode_error);
                    return -1;
            }
        }
        if (result == UUENCODE_STATUS_SHORT_FILE) result = 0;
        glb.attachment_count += result;
        if (strlen(hinfo->uudec_name))
        {
            if (strcasecmp(hinfo->uudec_name,"winmail.dat")==0)
            {
                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: Decoding TNEF format\n",FL);
                snprintf(hinfo->filename, 128, "%s", hinfo->uudec_name);
                MIME_decode_TNEF( unpackdir, hinfo, keep);
            }
            else LOGGER_log("%s:%d:MIME_decode_raw:WARNING: hinfo has been clobbered.\n",FL);
        }
    }

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: End[result = %d]\n",FL,result);
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_decode_text ID:1
Purpose:       Decodes an input stream into a text file.
Input:         unpackdir : directory where to place new text file
hinfo : struct containing information from the last parsed headers
keep : if set, retain the file
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_decode_text( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *hinfo, int keep )
{

    FILE *of;                           // output file
    int linecount = 0;                  // The number of lines
    int file_has_uuencode = 0;          // Flag to indicate this text has UUENCODE in it
    char fullfilename[1024]="";     // Filename of the output file
    char line[1024];                    // The input lines from the file we're decoding
    char *get_result = &line[0];
    int lastlinewasboundary = 0;
    int result = 0;
    int decodesize=0;

    snprintf(fullfilename,sizeof(fullfilename),"%s/%s",unpackdir,hinfo->filename);
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: Decoding TEXT [encoding=%d] to %s\n",FL, hinfo->content_transfer_encoding, fullfilename);

    if (!f)
    {
        /** If we cannot open the file for reading, leave an error and return -1 **/
        LOGGER_log("%s:%d:MIME_decode_text:ERROR: print-quotable input stream broken.",FL);
        return _EXITERR_MIMEREAD_CANNOT_OPEN_INPUT;
    }
    if (f)
    {
        /** If we were able to open the input file, try opening the output file and process the data **/
        of = fopen(fullfilename,"w");
        if (!of)
        {
            /** If we were unable to open the output file, report the error and return -1 **/
            LOGGER_log("%s:%d:MIME_decode_text:ERROR: cannot open %s for writing",FL,fullfilename);
            return _EXITERR_MIMEREAD_CANNOT_WRITE_OUTPUT;
        }
        while ((get_result = FFGET_fgets(line,1023,f))&&(of))
        {
            int line_len = strlen(line);
            linecount++;
            //      if (MIME_DPEDANTIC) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: line=%s",FL,line);
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: line[len=%d]=%s",FL,line_len,line);
            //20041217-1529:PLD:
            if (line[0] == '-')
            {
                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_DNORMAL:DEBUG: Testing boundary",FL);
                if ((BS_count() > 0)&&(BS_cmp(line,line_len)))
                {
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_DNORMAL:DEBUG: Hit a boundary on the line",FL);
                    lastlinewasboundary = 1;
                    result = 0;
                    break;
                }
            }

            if (lastlinewasboundary == 0)
            {
                if (hinfo->content_transfer_encoding == _CTRANS_ENCODING_QP)
                {
                    size_t bc;
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_DNORMAL:DEBUG: Hit a boundary on the line",FL);
                    decodesize = MDECODE_decode_qp_text(line);
                    bc = fwrite(line, 1, decodesize, of);

                } else {
                    fprintf(of,"%s",line);
                }

                if ((!file_has_uuencode)&&( UUENCODE_is_uuencode_header( line )))
                {
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: UUENCODED data located in file.\n",FL);
                    file_has_uuencode = 1;
                }
            }
            //  linecount++;
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_DNORMAL:DEBUG: End processing line.",FL);
        } // while
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: Done writing output file '%s'...now attempting to close.",FL, fullfilename);
        // if the file is still safely open
        if (of)
        {
            fclose(of);
        } // if file still safely open

        if (linecount == 0)
        {
            result = MIME_STATUS_ZERO_FILE;
            return result;
        }
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: Closed.",FL);
    } // if main input file stream was open

    // If our input from the file was invalid due to EOF or other
    // then we return a -1 code to indicate that the end has
    // occured.
    //
    if (!get_result) {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: FFGET module ran out of file data while attempting to decode",FL);
        //      result = -1;
        result = MIME_ERROR_FFGET_EMPTY; // 20040305-1323:PLD
    }

    // If there was UUEncoded portions [potentially] in the email, the
    // try to extract them using the MIME_decode_uu()
    //
    if (file_has_uuencode)
    {
        char ffname[256];
        snprintf(ffname,256,"%s/%s", unpackdir, hinfo->filename);
        // PLD-20040627-1212
        // Make sure uudec_name is blank too
        //
        hinfo->uudec_name[0] = '\0';
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: Decoding UUencoded data in file '%s'\n",FL,hinfo->filename);
        //result = UUENCODE_decode_uu( NULL, unpackdir, hinfo->filename, hinfo->uudec_name, sizeof(hinfo->uudec_name), 1, keep );
        // Attempt to decode the UUENCODED data in the file,
        //      NOTE - hinfo->uudec_name is a blank buffer which will be filled by the UUENCODE_decode_uu
        //          function once it has located a filename in the UUENCODED data.  A bit of a problem here
        //          is that it can only hold ONE filename!
        //
        //      NOTE - this function returns the NUMBER of attachments it decoded in the return value!  Don't
        //          propergate this value unintentionally to parent functions (ie, if you were thinking it was
        //          an error-status return value

        result = UUENCODE_decode_uu( NULL, unpackdir, ffname, hinfo->uudec_name, sizeof(hinfo->uudec_name), 1, keep );
        if (result == -1)
        {
            switch (uuencode_error) {
                case UUENCODE_STATUS_SHORT_FILE:
                case UUENCODE_STATUS_CANNOT_OPEN_FILE:
                case UUENCODE_STATUS_CANNOT_FIND_FILENAME:
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_raw:DEBUG: Nullifying uuencode_error result %d",FL, uuencode_error);
                    result = 0;
                    break;
                case UUENCODE_STATUS_CANNOT_ALLOCATE_MEMORY:
                    LOGGER_log("%s:%d:MIME_decode_raw:ERROR: Failure to allocate memory for UUdecode process",FL);
                    return -1;
                    break;
                default:
                    LOGGER_log("%s:%d:MIME_decode_raw:ERROR: Unknown return code from UUDecode process [%d]",FL,uuencode_error);
                    return -1;
            }
        }
        if ( result > 0 ) { glb.attachment_count += result; result = 0; }
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: hinfo = %p\n",FL,hinfo);
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: Done. [ UUName = '%s' ]\n",FL,hinfo->uudec_name);
        if (strncasecmp(hinfo->uudec_name,"winmail.dat",11)==0)
        {
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: Decoding TNEF format\n",FL);
            snprintf(hinfo->filename, 128, "%s", hinfo->uudec_name);
            MIME_decode_TNEF( unpackdir, hinfo, keep );
        }
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: Completed decoding UUencoded data.\n",FL);
    }
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_text:DEBUG: result=%d ----------------Done\n",FL,result);
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_decode_64 ID:1
Purpose:       This routine is very very very important, it's the key to ensuring
we get our attachments out of the email file without trauma!
NOTE - this has been -slightly altered- in order to make provision
of the fact that the attachment may end BEFORE the EOF is received
as is the case with multiple attachments in email.  Hence, we
now have to detect the start character of the "boundary" marker
I may consider testing the 1st n' chars of the boundary marker
just incase it's not always a hypen '-'.
Input:         FGET_FILE *f: stream we're reading from
char *unpackdir: directory we have to write the file to
struct MIMEH_header_info *hinfo: Auxillairy information such as the destination filename
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_decode_64( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *hinfo )
{
    int i;
    int cr_total = 0;
    int cr_count = 0; /* the number of consecutive \n's we've read in, used to detect End of B64 enc */
    int ignore_crcount = 0; /* If we have a boundary in our emails, then ignore the CR counts */
    int stopcount = 0; /* How many stop (=) characters we've read in */
    int eom_reached = 0; /* flag to say that we've reached the End-Of-MIME encoding. */
    int status = 0; /* Overall status of decoding operation */
    int c; /* a single char as retrieved using MIME_get_char() */
    int char_count = 0; /* How many chars have been received */
    int boundary_crash = 0; /* if we crash into a boundary, set this */
    long int bytecount=0; /* The total file decoded size */
    char output[3]; /* The 4->3 byte output array */
    char input[4]; /* The 4->3 byte input array */
    char fullMIME_filename[_MIME_STRLEN_MAX]=""; /* Full Filename of output file */

    // Write Buffer routine

    unsigned char *writebuffer;
    unsigned char *wbpos;
    int wbcount = 0;
    int loop;

    int of; /* output file pointer */
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: attempting to decode '%s'", FL, hinfo->filename);

    /* generate the MIME_filename, and open it up... */
    if (glb.unique_names) MIME_test_uniquename( unpackdir, hinfo->filename, glb.rename_method );
    snprintf(fullMIME_filename,_MIME_STRLEN_MAX,"%s/%s",unpackdir,hinfo->filename);
    //of = fopen(fullMIME_filename,"wb");
    of = open(fullMIME_filename, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP);
    /* if we were unable to open the output file, then we better log an error and drop out */
    if (of < 0)
    {
        LOGGER_log("%s:%d:MIME_decode_64:ERROR: Cannot open output file %s for BASE64 decoding. (%s)",FL,fullMIME_filename, strerror(errno));
        //      exit(_EXITERR_BASE64_OUTPUT_NOT_OPEN);
        return -1;
    }

    // Allocate the write buffer.  By using the write buffer we gain an additional 10% in performance
    // due to the lack of function call (fwrite) overheads
    writebuffer = malloc( _MIME_WRITE_BUFFER_SIZE *sizeof(unsigned char));
    if (!writebuffer)
    {
        LOGGER_log("%s:%d:MIME_decode_64:ERROR: cannot allocate %dbytes of memory for the write buffer",FL, _MIME_WRITE_BUFFER_SIZE);
        return -1;
    }
    else {
        wbpos = writebuffer;
        wbcount = 0;
    }
    /* Set our ignore_crcount flag */
    if (BS_count() > 0)
    {
        //LOGGER_log("%s:%d:MIME_decode_64:DEBUG: Ignore CR set to 1",FL);
        ignore_crcount = 1;
    }
    /* collect prefixing trash (if any, such as spaces, CR's etc)*/
    //  c = MIME_getchar_start(f);
    /* and push the good char back */
    //  FFGET_ungetc(f,c);
    c = '\0';

    /* do an endless loop, as we're -breaking- out later */
    while (1)
    {
        int lastchar_was_linebreak=0;
        /* Initialise the decode buffer */
        input[0] = input[1] = input[2] = input[3] = 0; // was '0' - Stepan Kasal patch
        /* snatch 4 characters from the input */
        for (i = 0; i < 4; i++)
        {
            // Get Next char from the file input
            //
            // A lot of CPU is wasted here due to function call overheads, unfortunately
            // I cannot yet work out how to make this call (FFGET) operate effectively
            // without including a couple of dozen lines in this section.
            //
            // Times like this C++'s "inline" statement would be nice.
            //
            //----------INLINE Version of FFGET_getchar()
            //
            do {
                if ((c == '\n')||(c == '\r')) lastchar_was_linebreak = 1; else lastchar_was_linebreak = 0;
                if (f->ungetcset)
                {
                    f->ungetcset = 0;
                    c = f->c;
                }
                else
                {
                    if ((!f->startpoint)||(f->startpoint > f->endpoint))
                    {
                        FFGET_getnewblock(f);
                    }
                    if (f->startpoint <= f->endpoint)
                    {
                        c = *f->startpoint;
                        f->startpoint++;
                    }
                    else
                    {
                        c = EOF;
                    }
                }
            }
            while ( (c != EOF) && ( c < ' ' ) && ( c != '\n' ) && (c != '-') && (c != '\r') );
            //
            //-------END OF INLINE---------------------------------------------
            if ((ignore_crcount == 1)&&(c == '-'))
                //&&(lastchar_was_linebreak == 1))
            {
                int hit = 0;
                char *p;
                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: leader '-' found at %50s",FL,(f->startpoint -1));
                p = strchr((f->startpoint -1), '\n');
                if (p == NULL) {
                    /* The boundary test was failing because sometimes the full line
                     * wasn't available to test against in BM_cmp(), so if we can't
                     * see a \n (or \r even), then we should load up some more data
                     */
                    char scratch[1024];
                    FFGET_fgets(scratch,sizeof(scratch), f);
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: Scratch = '%s'", FL, scratch);
                    hit = BS_cmp(scratch,strlen(scratch) +1);
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: Boundary hit = %d", FL, hit);
                } else {
                    *p = '\0';
                    hit = BS_cmp((f->startpoint -1),strlen(f->startpoint) +1);
                    *p = '\n';
                }
                if (hit > 0) {
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: Boundary detected and breaking out ",FL);
                    //      FFGET_fgets(scratch,sizeof(scratch),f);
                    //      eom_reached = 1;
                    boundary_crash = 1;
                    break;
                } else {
                    /** 20041105-22H56:PLD: Applied Stepan Kasal patch **/
                    i--;
                    continue;
                }
            }
            // If we get a CR then we need to check a few things, namely to see
            //  if the BASE64 stream has terminated ( indicated by two consecutive
            //  \n's
            //
            // This should not be affected by \r's because the \r's will just pass
            //  through and be absorbed by the detection of possible 'invalid'
            //  characters
            if ((ignore_crcount == 0)&&(c == '\n'))
            {
                cr_count++;
                cr_total++;
                // Because there are some mail-agents out there which are REALLY naughty
                //      and do things like sticking random blank lines into B64 data streams
                //      (why o why!?) we need to have a settable option here to decide if
                //      we want to test for double-CR breaking or just ignore it
                //
                // To avoid this, we relax the double-CR extention, and say "Okay, we'll let
                // you have a single blank line - but any more than that and we'll slap you
                //  hand".  Honestly, I do not like this solution, but it seems to be the only
                //  way to preserve decoding accurancy without resorting to having to use
                //  a command line switch to dictate which method to use ( NO-CR's or ignore
                // CR's ).
                if (cr_count > 2)
                {
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: EOF Reached due to two consecutive CR's on line %d\n",FL,cr_total);
                    eom_reached++;
                    break;
                }
                else
                {
                    char_count = 0;
                    i--;
                    continue;
                } // else if it wasn't our 3rd CR
            } else {
                cr_count=0;
            }
            /* if we get an EOF char, then we know something went wrong */
            if ( c == EOF )
            {
                size_t bc;
                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: input stream broken for base64 decoding for file %s. %ld bytes of data in buffer to be written out\n",FL,hinfo->filename,wbcount);
                status = MIME_ERROR_B64_INPUT_STREAM_EOF;
                //fwrite(writebuffer, 1, wbcount, of);
                bc = write( of, writebuffer, wbcount);
                close(of);
                if (writebuffer) free(writebuffer);
                return status;
                break;
            } /* if c was the EOF */
            else if (c == '=')
            {
                // Once we've found a stop char, we can actually just "pad" in the rest
                // of the stop chars because we know we're at the end. Some MTA's dont
                // put in enough stopchars... at least it seems X-MIMEOLE: Produced By Microsoft MimeOLE V5.50.4133.2400
                // doesnt.
                if (i == 2)
                {
                    input[2] = input[3] = (char)b64[c];
                }
                else if (i == 3)
                {
                    input[3] = (char)b64[c];
                }
                // NOTE------
                // Previously we relied on the fact that if we detected a stop char, that FFGET()
                // would automatically absorb the data till EOL. This is no longer the case as we
                // are now only retrieving data byte at a time.
                // So, now we -absorb- till the end of the line using FFGET_fgets()
                stopcount = 4 -i;
                FFGET_fgets(scratch,sizeof(scratch),f);
                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: Stop char detected pos=%d...StopCount = %d\n",FL,i,stopcount);
                i = 4;
                break; // out of FOR.
            }
            /*
                else if ((char_count == 0)&&(c == '-' )) {
                if (FFGET_fgetc(f) == '-')
                {
                boundary_crash++;
                eom_reached++;
                break;
                }
                }
                */
            /* test for and discard invalid chars */
            if (b64[c] == 0x80)
            {
                i--;
                continue;
            }
            // Finally, if we get this far without the character having been picked
            //  out as some special meaning character, we decode it and place the
            //  resultant byte into the input[] array.
            input[i] = (char)b64[c];
            /* assuming we've gotten this far, then we increment the char_count */
            char_count++;
        } // FOR
        // now that our 4-char buffer is full, we can do some fancy bit-shifting and get the required 3-chars of 8-bit data
        output[0] = (input[0] << 2) | (input[1] >> 4);
        output[1] = (input[1] << 4) | (input[2] >> 2);
        output[2] = (input[2] << 6) | input[3];
        // determine how many chars to write write and check for errors if our input char count was 4 then we did receive a propper 4:3 Base64 block, hence write it
        if (i == 4)
        {
            // If our buffer is full beyond the 'write out limit', then we write the buffered
            //  data to the file -  We use this method in order to save calling fwrite() too
            //  many times, thus avoiding function call overheads and [ possibly ] disk write
            //  interrupt costs.
            if ( wbcount > _MIME_WRITE_BUFFER_LIMIT )
            {
                size_t bc;
                bc = write ( of, writebuffer, wbcount );
                //          fwrite(writebuffer, 1, wbcount, of);
                wbpos = writebuffer;
                wbcount = 0;
            }
            // Copy our converted bytes to the write buffer
            for (loop = 0; loop < (3 -stopcount); loop++)
            {
                *wbpos = output[loop];
                wbpos++;
                wbcount++;
            }
            // tally up our total byte conversion count
            bytecount+=(3 -stopcount);
        }
        else if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: could not attain 4 bytes input\n",FL);

        // if we wrote less than 3 chars, it means we were at the end of the encoded file thus we exit
        if ((eom_reached)||(stopcount > 0)||(boundary_crash)||(i!=4))
        {
            // Write out the remaining contents of our write buffer - If we don't do this
            //  we'll end up with truncated files.
            if (wbcount > 0)
            {
                size_t bc;
                //fwrite(writebuffer, 1, wbcount, of);
                bc = write( of, writebuffer, wbcount);
            }
            /* close the output file, we're done writing to it */
            close(of);
            /* if we didn't really write anything, then trash the  file */
            if (bytecount == 0)
            {
                //              unlink(fullMIME_filename);
                status = MIME_BASE64_STATUS_ZERO_FILE;
            }
            if (boundary_crash)
            {
                // Absorb to end of line
                //
                status = MIME_BASE64_STATUS_HIT_BOUNDARY; // was _BOUNDARY_CRASH
            }
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_64:DEBUG: File size = %ld bytes, Exit Status = %d, Boundary Crash = %d\n",FL, bytecount, status, boundary_crash);
            if (writebuffer) free(writebuffer);
            return status;
        } // if End-of-MIME or Stopchars appeared
    } // while
    if (writebuffer) free(writebuffer);
    return status;
}


/*-----------------------------------------------------------------\
  Function Name : MIME_decode_64_cleanup
  Returns Type  : int
  ----Parameter List
  1. FFGET_FILE *f,
  2.  char *unpackdir,
  3.  struct MIMEH_header_info *hinfo,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_decode_64_cleanup( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *hinfo)
{
    int result = 0;
    char buffer[128];

    while (FFGET_fgets(buffer, sizeof(buffer), f))
    {
        if (FFGET_feof(f) != 0) break;
        if (BS_cmp(buffer,strlen(buffer)) > 0) break;
    }
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_doubleCR_decode ID:1
Purpose:       Decodes a text sequence as detected in the processing of the MIME headers.
This is a specialised call, not really a normal part of MIME decoding, but is
required in order to deal with decyphering MS Outlook visable defects.
Input:         char *filename: Name of the encoded file we need to decode
char *unpackdir: Directory we need to unpack the file to
struct MIMEH_header_info *hinfo: Header information already gleaned from the headers
int current_recursion_level: How many nest levels we are deep
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_doubleCR_decode( char *filename, char *unpackdir, struct MIMEH_header_info *hinfo, int current_recursion_level )
{
    int result = 0;
    struct MIMEH_header_info h;
    char *p;
    // PLD:260303-1317
    //  if ((p=strrchr(filename,'/'))) p++;
    //  else p = filename;

    p = filename;
    // * Initialise the header fields
    h.uudec_name[0] = '\0';
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_doubleCR_decode:DEBUG: filename=%s, path=%s, recursion=%d", FL, filename, unpackdir, current_recursion_level );
    memcpy(&h, hinfo, sizeof(h));
    // Works for ripMIME    snprintf(h.filename, sizeof(h.filename), "%s/%s", unpackdir, p);
    snprintf(h.filename, sizeof(h.filename), "%s", p); /// Works for Xamime
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_doubleCR_decode:DEBUG: header.filename = %s", FL, h.filename );
    if (MIME_is_file_RFC822(filename))
    {
        if (MIME_VERBOSE) LOGGER_log("Attempting to decode Double-CR delimeted MIME attachment '%s'\n",filename);
        result = MIME_unpack( unpackdir, filename, current_recursion_level ); // 20040305-1303:PLD - Capture the result of the unpack and propagate up
    }
    else if (UUENCODE_is_file_uuencoded(h.filename))
    {
        if (MIME_VERBOSE) LOGGER_log("Attempting to decode UUENCODED attachment from Double-CR delimeted attachment '%s'\n",filename);
        UUENCODE_set_doubleCR_mode(1);
        result = UUENCODE_decode_uu(NULL, unpackdir, filename, h.uudec_name, _MIMEH_FILENAMELEN_MAX , 1, 1 );
        UUENCODE_set_doubleCR_mode(0);
        glb.attachment_count += result;
        result = 0;
    }
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_read ID:1
Purpose:       Reads data from STDIN and saves the mailpack to the filename
specified
Input:         char *mpname: full pathname of the file to save the data from STDIN
to
Output:
Errors:
28-Feb-2003
This function has been modified to use feof() and fread/fwrite
calls in order to ensure that binary data in the input does not
cause the reading to prematurely terminate [ as it used to
prior to the modification ]
------------------------------------------------------------------------*/
size_t MIME_read_raw( char *src_mpname, char *dest_mpname, size_t rw_buffer_size )
{
    size_t readcount, writecount;
    size_t fsize=-1;
    char *rw_buffer;
    int fin;
    int fout;

    rw_buffer = NULL;

    if (*src_mpname == '\0') {
        fin = STDIN_FILENO;
    } else {
        fin = open(src_mpname, O_RDONLY);
        if (fin == -1) {
            LOGGER_log("%s:%d:MIME_read_raw:ERROR: Cannot open '%s' for reading (%s)", FL, src_mpname, strerror(errno));
            return -1;
        }
    }

    rw_buffer = malloc( (rw_buffer_size +1) *sizeof(char) );
    if ( !rw_buffer )
    {
        LOGGER_log("%s:%d:MIME_read:ERROR: could not allocate %d of memory for file read buffer\n",FL, rw_buffer_size );
        return -1;
    }

    /* open up our input file */
    fout = open(dest_mpname,O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP);
    if (fout == -1) {
        LOGGER_log("%s:%d:MIME_read_raw:ERROR: Cannot open '%s' for writing. (%s)",FL, dest_mpname, strerror(errno));
        return -1;
    }

    fsize=0;
    /* while there is more data, consume it */
    do {
        readcount = read( fin, rw_buffer, rw_buffer_size );
        if (readcount > 0) {
            writecount = write( fout, rw_buffer, readcount );
            if (writecount == -1) {
                LOGGER_log("%s:%d:MIME_read_raw:ERROR: While attempting to write data to '%s' (%s)", FL, dest_mpname, strerror(errno));
                return -1;
            }
            if ( readcount != writecount )
            {
                LOGGER_log("%s:%d:MIME_read_raw:ERROR: Attempted to write %d bytes, but only managed %d to file '%s'",FL, readcount, writecount, dest_mpname );
            }
            fsize += writecount;
        }
    } while (readcount > 0);

    if ((*src_mpname != '\0')&&(readcount >= 0)) {
        close(fin);
    }
    if (readcount == -1) {
        LOGGER_log("%s:%d:MIME_read_raw:ERROR: read() '%s'",FL, strerror(errno));
        return -1;
    }
    close(fout);
    if ( rw_buffer != NULL ) free( rw_buffer );
    return (size_t)(fsize);
}

/*------------------------------------------------------------------------
Procedure:     MIME_read ID:1
Purpose:       Reads data from STDIN and saves the mailpack to the filename
specified
Input:         char *mpname: full pathname of the file to save the data from STDIN
to
Output:
Errors:
28-Feb-2003
This function has been modified to use feof() and fread/fwrite
calls in order to ensure that binary data in the input does not
cause the reading to prematurely terminate [ as it used to
prior to the modification ]
------------------------------------------------------------------------*/
int MIME_read( char *mpname )
{
    long int fsize=-1;
    char *buffer;
    size_t readcount, writecount;
    FILE *fout;
    buffer = malloc( MIME_MIME_READ_BUFFER_SIZE *sizeof(char) );
    if ( !buffer )
    {
        LOGGER_log("%s:%d:MIME_read:ERROR: could not allocate 4K of memory for file read buffer\n",FL );
        return -1;
    }
    /* open up our input file */
    fout = fopen(mpname,"w");
    /* check that out file opened up okay */
    if (!fout)
    {
        LOGGER_log("%s:%d:MIME_read:ERROR: Cannot open file %s for writing... check permissions perhaps?",FL,mpname);
        //exit(_EXITERR_MIMEREAD_CANNOT_OPEN_OUTPUT);
        return -1;
    }
    /* assuming our file actually opened up */
    if (fout)
    {
        fsize=0;
        /* while there is more data, consume it */
        while ( !feof(stdin) )
        {
            readcount = fread( buffer, 1, ((MIME_MIME_READ_BUFFER_SIZE -1) *sizeof(char)), stdin );
            if ( readcount > 0 )
            {
                writecount = fwrite( buffer, 1, readcount, fout );
                fsize += writecount;

                if ( readcount != writecount )
                {
                    LOGGER_log("%s:%d:MIME_read:ERROR: Attempted to write %d bytes, but only managed %d to file '%s'",FL, readcount, writecount, mpname );
                }

            } else {
                break;
            }
        }
        /* clean up our buffers and close */
        fflush(fout);
        fclose(fout);
        if ( feof(stdin) ) clearerr(stdin);
    } /* end if fout was received okay */
    if ( buffer ) free( buffer );
    /* return our byte count in KB */
    return (int)(fsize /1024);
}




/*------------------------------------------------------------------------
Procedure:     MIME_init ID:1
Purpose:       Initialise various required parameters to ensure a clean starting of
MIME decoding.
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_init( void )
{

    BS_init();          // Boundary-stack initialisations
    MIMEH_init();       // Initialise MIME header routines.
    UUENCODE_init();    // uuen:coding decoding initialisations
    FNFILTER_init();    // Filename filtering
    MDECODE_init(); // ISO filename decoding initialisation
    TNEF_init();        // TNEF decoder


    glb.header_defect_count = 0;
    glb.filecount = 0;
    glb.attachment_count = 0;
    glb.current_line = 0;
    glb.verbosity = 0;
    glb.verbose_defects = 0;
    glb.debug = 0;
    glb.quiet = 0;
    glb.syslogging = 0;
    glb.stderrlogging = 1;
    glb.unique_names = 0;
    glb.save_headers = 0;
    glb.no_nameless = 0;
    glb.mailbox_format = 0;
    glb.name_by_type = 0;
    glb.rename_method = _MIME_RENAME_METHOD_INFIX;

    glb.header_longsearch = 0;
    glb.max_recursion_level = _RECURSION_LEVEL_DEFAULT;

    glb.decode_qp = 1;
    glb.decode_b64 = 1;
    glb.decode_tnef = 1;
    glb.decode_ole = 1;
    glb.decode_uu = 1;
    glb.decode_mht = 1;

    glb.multiple_filenames = 1;

    glb.blankzone_save_option = MIME_BLANKZONE_SAVE_TEXTFILE;
    glb.blankzone_saved = 0;

    snprintf( glb.headersname, sizeof(glb.headersname), "_headers_" );
    snprintf( glb.blankfileprefix, sizeof( glb.blankfileprefix ), "textfile" );
    glb.blankfileprefix_expliticly_set = 0;

    glb.subject[0]='\0';

    return 0;
}



/*-----------------------------------------------------------------\
  Function Name : MIME_generate_multiple_hardlink_filenames
  Returns Type  : int
  ----Parameter List
  1. struct MIMEH_header_info *hinfo,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:
If there is more than one filename/name for a given attachment
due to an exploit attempt,then we need to generate the required
hardlinks to replicate this in our output.

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_generate_multiple_hardlink_filenames(struct MIMEH_header_info *hinfo, char *unpackdir)
{
    char *name;
    char oldname[1024];

    if (glb.multiple_filenames == 0) return 0;

    //LOGGER_log("%s:%d:MIME_generate_multiple_hardlink_filenames:DEBUG: Generating hardlinks for %s",FL, hinfo->filename);
    snprintf(oldname,sizeof(oldname),"%s/%s",unpackdir, hinfo->filename);

    if (SS_count(&(hinfo->ss_names)) > 1){
        do {

            name = SS_pop(&(hinfo->ss_names));
            if (name != NULL)
            {
                char *np;
                char newname[1024];
                int rv;

                /** Strip off any leading path **/
                np = strrchr(name, '/');
                if (np) np++; else np = name;

                snprintf(newname,sizeof(newname),"%s/%s",unpackdir, np);
                //LOGGER_log("%s:%d:MIME_generate_multiple_hardlink_filenames:DEBUG: Linking %s->%s",FL,newname, oldname);
                rv = link(oldname, newname);
                if (rv == -1)
                {
                    if (errno != EEXIST)
                    {
                        LOGGER_log("%s:%d:MIME_generate_multiple_hardlink_filenames:WARNING: While trying to create '%s' link to '%s' (%s)",FL, newname, oldname,strerror(errno));
                    }

                } else {
                    if ((glb.filename_decoded_reporter != NULL)&&(MIME_VERBOSE))
                    {
                        glb.filename_decoded_reporter( name, (MIMEH_get_verbosity_contenttype()?hinfo->content_type_string:NULL));
                    }
                }
            }

        } while(name != NULL);
    }

    if (SS_count(&(hinfo->ss_filenames)) > 1) {
        do {

            name = SS_pop(&(hinfo->ss_filenames));
            if (name != NULL)
            {
                char newname[1024];
                int rv;

                snprintf(newname,sizeof(newname),"%s/%s",unpackdir, name);
                //LOGGER_log("%s:%d:MIME_generate_multiple_hardlink_filenames:DEBUG: Linking %s->%s",FL,newname, oldname);
                rv = link(oldname, newname);
                if (rv == -1)
                {
                    if (errno != EEXIST)
                    {
                        LOGGER_log("%s:%d:MIME_generate_multiple_hardlink_filenames:WARNING: While trying to create '%s' link to '%s' (%s)",FL, newname, oldname,strerror(errno));
                    }

                } else {
                    if ((glb.filename_decoded_reporter != NULL)&&(MIME_VERBOSE))
                    {
                        glb.filename_decoded_reporter( name, (MIMEH_get_verbosity_contenttype()?hinfo->content_type_string:NULL));
                    }
                }
            }

        } while(name != NULL);
    }

    return 0;

}

/*------------------------------------------------------------------------
Procedure:     MIME_decode_encoding ID:1
Purpose:       Based on the contents of hinfo, this function will call the
required function needed to decode the contents of the file
which is contained within the MIME structure
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_decode_encoding( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *hinfo, struct SS_object *ss )
{
    int keep = 1;
    int result = -1;

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Start:DEBUG: (%s)\n",FL, hinfo->filename);

    // If we have a valid filename, then put it through the process of
    //  cleaning and filtering
    if (isprint((int)hinfo->filename[0]))
    {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Filename is valid, cleaning\n",FL);
        FNFILTER_filter(hinfo->filename, _MIMEH_FILENAMELEN_MAX);   /* check out thefilename for ISO filenames */
    }

    // If the filename is NOT valid [doesn't have a printable first char]
    //  then we must create a new file name for it.
    //
    if (hinfo->filename[0] == '\0')
    {
        if (glb.name_by_type == 1)
        {
            // If we're to name our nameless files based on the content-type
            //      then we need to get the content-type string from the hinfo
            //      and then strip it of any nasty characters (such as / and \)
            char *filename_prefix;
            filename_prefix = strdup( hinfo->content_type_string );
            if (filename_prefix != NULL)
            {
                char *pp;
                pp = filename_prefix;
                while (*pp) { if ((*pp == '/') || (*pp == '\\')) *pp = '-'; pp++; }
                snprintf( hinfo->filename, _MIMEH_FILENAMELEN_MAX, "%s%s%d", glb.blankfileprefix_expliticly_set?glb.blankfileprefix:"", filename_prefix, glb.filecount );
                free(filename_prefix);
            } else {
                snprintf( hinfo->filename, _MIMEH_FILENAMELEN_MAX, "%s%d", glb.blankfileprefix, glb.filecount );
            }
        } else {
            // If we don't care to rename our files based on the content-type
            //      then we'll simply use the blankfileprefix.
            snprintf( hinfo->filename, _MIMEH_FILENAMELEN_MAX, "%s%d", glb.blankfileprefix, glb.filecount );
        }
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Filename is empty, setting to default...(%s)\n",FL, hinfo->filename);
        if (glb.no_nameless) keep = 0;
        glb.filecount++;
    }
    else if (strncmp(hinfo->filename, glb.blankfileprefix, strlen(glb.blankfileprefix)) != 0)
    {
        // If the filename does not contain the blankfile prefix at the beginning, then we
        //  will consider it a normal attachment, thus, we
        // need to increment the attachment count
        glb.attachment_count++;
    }
    // If we are required to have "unique" filenames for everything, rather than
    //  allowing ripMIME to overwrite stuff, then we put the filename through
    //      its tests here
    if ((glb.unique_names)&&(keep))
    {
        MIME_test_uniquename( unpackdir, hinfo->filename, glb.rename_method );
    }
    // If the calling program requested verbosity, then indicate that we're decoding
    //      the file here
    if ((keep)&&(MIME_VERBOSE))
    {
        if (MIME_VERBOSE_12)
        {
            LOGGER_log("Decoding: %s\n", hinfo->filename);
        } else {
            // Now, please bare with me on this horrid little piece of tortured code,
            // ... basically, now that we're using LOGGER_log to generate our output, we
            // need to compose everything onto a single LOGGER_log() call, otherwise our
            // output will get split into several lines, not entirely the most plesant
            // thing we want to see when we've got another program most likely reading
            // the output.
            //
            // The %s%s pair is DELIBERATELY pressed up against the 'Decoding' word because
            // otherwise, if we are not outputting any data, we'll end up with a double
            // space between Decoding and filename, certainly not very good thing to do
            // if we're trying to present a consistant output.
            //
            // The parameters for the content-type output are decided on by the result
            // of the MIMEH_get_verbosity_contenttype() call.  If the call returns > 0
            // then the TRUE token is selected to be displayed, else the FALSE token
            // ( in this case, an empty string "" ) is selected.
            // The format of the evaluation is:
            //
            //      (logic-test?true-expression:false-expression)
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: About to execute callback [0x%p]",FL,glb.filename_decoded_reporter);
            if (glb.filename_decoded_reporter != NULL)
            {
                glb.filename_decoded_reporter( hinfo->filename, (MIMEH_get_verbosity_contenttype()?hinfo->content_type_string:NULL));
            }

        } // If we were using the new filename telling format
    } // If we were telling the filename (verbosity)
    if (1)
    {
        char *fp;
        /** Find the start of the filename. **/
        fp = strrchr(hinfo->filename, '/');
        if (fp) fp++; else fp = hinfo->filename;
        //LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Pushing filename %s to the stack",FL,fp);
        // 20040305-1419:PLD
        // Store the filename we're going to use to save the file to in the filename stack
        SS_push(ss, fp, strlen(fp));
    }
    // Select the decoding method based on the content transfer encoding
    //  method which we read from the headers

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: ENCODING = %d\n",FL, hinfo->content_transfer_encoding);
    switch (hinfo->content_transfer_encoding)
    {
        case _CTRANS_ENCODING_B64:
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding BASE64 format\n",FL);
            result = MIME_decode_64(f, unpackdir, hinfo);
            switch (result) {
                case MIME_ERROR_B64_INPUT_STREAM_EOF:
                    break;
                case MIME_BASE64_STATUS_HIT_BOUNDARY:
                    result = 0;
                    break;
                case 0:
                    result = MIME_decode_64_cleanup(f, unpackdir, hinfo);
                    break;
                default:
                    break;
            }
            break;
        case _CTRANS_ENCODING_7BIT:
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding 7BIT format\n",FL);
            result = MIME_decode_text(f, unpackdir, hinfo, keep);
            break;
        case _CTRANS_ENCODING_8BIT:
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding 8BIT format\n",FL);
            result = MIME_decode_text(f, unpackdir, hinfo, keep);
            break;
        case _CTRANS_ENCODING_BINARY:
        case _CTRANS_ENCODING_RAW:
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding RAW format\n",FL);
            result = MIME_decode_raw(f, unpackdir, hinfo, keep);
            break;
        case _CTRANS_ENCODING_QP:
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding Quoted-Printable format\n",FL);
            result = MIME_decode_text(f, unpackdir, hinfo, keep);
            break;
        case _CTRANS_ENCODING_UUENCODE:
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding UUENCODED format\n",FL);
            // Added as a test - remove if we can get this to work in a better way
            snprintf(hinfo->uudec_name,sizeof(hinfo->uudec_name),"%s",hinfo->filename);
            result = UUENCODE_decode_uu(f, unpackdir, hinfo->filename, hinfo->uudec_name, sizeof(hinfo->uudec_name), 0, keep );
            glb.attachment_count += result;
            // Because this is a file-count, it's not really an 'error result' as such, so, set the
            //      return code back to 0!
            result = 0;
            break;
        case _CTRANS_ENCODING_UNKNOWN:
            switch (hinfo->content_disposition) {
                case _CDISPOSITION_FORMDATA:
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding UNKNOWN format of FORMDATA disposition\n",FL);
                    result = MIME_decode_raw(f, unpackdir, hinfo, keep);
                    break;
                default:
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding UNKNOWN format\n",FL);
                    result = MIME_decode_text(f, unpackdir, hinfo, keep);
            }
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: UNKNOWN Decode completed, result = %d\n",FL,result);
            break;
        case _CTRANS_ENCODING_UNSPECIFIED:
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding UNSPECIFIED format\n",FL);
            result = MIME_decode_text(f, unpackdir, hinfo, keep);
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding result for UNSPECIFIED format = %d\n",FL, result);
            // 20040114-1236:PLD: Added nested mail checking
            //
            // Sometimes mailpacks have false headers at the start, resulting
            //      in ripMIME terminating the recursion process too early.  This
            //      little test here checks to see if the output of the file is
            //      a nested MIME email (or even an ordinary email
            //
            //  It should be noted, that the reason why the test occurs /here/ is
            //      because dud headers will always result in a UNSPECIFIED encoding
            //
            //  Original sample mailpack was sent by Farit - thanks.
            if (1)
            {
                snprintf(hinfo->scratch,sizeof(hinfo->scratch),"%s/%s",unpackdir,hinfo->filename);
                LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG:REMOVEME: Testing for RFC822 headers in file %s",FL,hinfo->scratch);
                if (MIME_is_file_RFC822(hinfo->scratch) > 0 )
                {
                    // 20040305-1304:PLD: unpack the file, propagate result upwards
                    result = MIME_unpack_single( unpackdir, hinfo->scratch, (hinfo->current_recursion_level+1),ss );
                }
            }
            break;
        default:
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding format is not defined (%d)\n",FL, hinfo->content_transfer_encoding);
            result = MIME_decode_raw(f, unpackdir, hinfo, keep);
            break;
    }
    // Analyze our results
    switch (result) {
        case 0:
            break;
        case MIME_STATUS_ZERO_FILE:
            return MIME_STATUS_ZERO_FILE;
            break;
        case MIME_ERROR_FFGET_EMPTY:
            return result;
            break;
        case MIME_ERROR_RECURSION_LIMIT_REACHED:
            return result;
            break;
		  default: 
				return result;
    }

    if ((result != -1)&&(result != MIME_STATUS_ZERO_FILE))
    {
#ifdef RIPOLE
        // If we have OLE decoding active and compiled in, then
        //      do a quick attempt at decoding the file, fortunately
        //      because the ripOLE engine detects if the file is not an
        //      OLE file, it does not have a major impact on the
        //      performance of the ripMIME decoding engine
        if (glb.decode_ole > 0)
        {
            MIME_decode_OLE( unpackdir, hinfo, 0 );
        }
#endif

        // If our content-type was TNEF, then we need to decode it
        //      using an external decoding engine (see tnef/tnef.c)
        // Admittingly, this is not the most ideal way of picking out
        //      TNEF files from our decoded attachments, but, for now
        //      it'll have to do, besides, it does work fine, without any
        //      side-effects
        if (hinfo->content_type == _CTYPE_TNEF)
        {
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Decoding TNEF format\n",FL);
            glb.attachment_count++;
            MIME_decode_TNEF( unpackdir, hinfo, 0 );
        } // Decode TNEF

        // Look for Microsoft MHT files... and try decode them.
        //  MHT files are just embedded email files, except they're usually
        //  encoded as BASE64... so, you have to -unencode- them, to which
        //  you find out that lo, you have another email.

        if ((strstr(hinfo->filename,".mht"))||(strstr(hinfo->name,".mht")) )
        {
            if (glb.decode_mht != 0)
            {
                //  Patched 26-01-03: supplied by Chris Hine
                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Microsoft MHT format email filename='%s'\n",FL, hinfo->filename);
                snprintf(hinfo->scratch,sizeof(hinfo->scratch),"%s/%s",unpackdir,hinfo->filename);

                // 20040305-1304:PLD: unpack the file, propagate result upwards
                result = MIME_unpack_single( unpackdir, hinfo->scratch, (hinfo->current_recursion_level+1),ss );
            }
        } // Decode MHT files
    } // If result != -1
    // End.

    MIME_generate_multiple_hardlink_filenames(hinfo,unpackdir);
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_decode_encoding:DEBUG: Done for filename = '%s'",FL,hinfo->filename);
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_postdecode_cleanup ID:1
Purpose:       Performs any cleanup operations required after the immediate completion of the mailpack decoding.
Input:         char *unpackdir - directory where the mailpack was unpacked to
Output:        none
Errors:
------------------------------------------------------------------------*/
int MIME_postdecode_cleanup( char *unpackdir, struct SS_object *ss )
{
    char fullpath[256];
    int result;
    result = 0;
    do {
        char *filename;
        if (MIME_DNORMAL) LOGGER_log("%s:%d: Popping file...",FL);
        filename = SS_pop(ss);
        if (filename == NULL) break;
        if (MIME_DNORMAL) LOGGER_log("%s:%d: Popped file '%s'",FL, filename);
        if ( strncmp( glb.blankfileprefix, filename, strlen( glb.blankfileprefix ) ) == 0 )
        {
            snprintf( fullpath, sizeof(fullpath), "%s/%s", unpackdir, filename );
            result = unlink( fullpath );
            if (MIME_VERBOSE)
            {
                if (result == -1) LOGGER_log("Error removing '%s'; %s", fullpath, strerror(errno));
                else LOGGER_log("Removed %s [status = %d]\n", fullpath, result );
            }
        }
    } while (1);
    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_handle_multipart
  Returns Type  : int
  ----Parameter List
  1. FFGET_FILE *f,
  2.  char *unpackdir,
  3.  struct MIMEH_header_info *hinfo,
  4.  int current_recursion_level,
  5.  struct SS_object *ss ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_handle_multipart( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *h, int current_recursion_level, struct SS_object *ss )
{
    int result = 0;
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_multipart:DEBUG: Decoding multipart/embedded \n",FL);
    // If there is no filename, then we have a "standard"
    // embedded message, which can be just read off as a
    // continuous stream (simply with new boundaries
    //
    if (( h->content_transfer_encoding != _CTRANS_ENCODING_B64)&&( h->filename[0] == '\0' ))
    {
        char *p;

        // If this is a simple 'wrapped' RFC822 email which has no encoding applied to it
        //      (ie, it's just the existing email with a new set of headers around it
        //      then rather than saving it to a file, we'll just peel off these outter
        //      layer of headers and get into the core of the message.  This is why we
        //      call unpack_stage2() because this function just reads the next set of
        //      headers and decodes.
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_multipart:DEBUG: Non base64 encoding AND no filename, embedded message\n",FL);
        h->boundary_located = 0;
        result = MIME_unpack_stage2(f, unpackdir, h, current_recursion_level , ss);
        p = BS_top();
        if (p) PLD_strncpy(h->boundary, p,sizeof(h->boundary));
    } else {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_multipart:DEBUG: Embedded message has a filename, decoding to file %s",FL,h->filename);
        result = MIME_decode_encoding( f, unpackdir, h, ss );
        if (result == 0)
        {
            // Because we're calling MIME_unpack_single again [ie, recursively calling it
            // we need to now adjust the input-filename so that it correctly is prefixed
            // with the directory we unpacked to.

            snprintf(scratch,sizeof(scratch),"%s/%s",unpackdir, h->filename);
            snprintf(h->filename,sizeof(h->filename),"%s",scratch);

            //result = MIME_unpack_single( unpackdir, h->filename, current_recursion_level +1, ss);
            result = MIME_unpack_single( unpackdir, h->filename, current_recursion_level, ss );
        }

    } // else-if transfer-encoding != B64 && filename was empty
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_multipart:DEBUG: done handling '%s' result = %d",FL,h->filename, result);
    return result;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_handle_rfc822
  Returns Type  : int
  ----Parameter List
  1. FFGET_FILE *f,                         Input stream
  2.  char *unpackdir,                      Directory to write files to
  3.  struct MIMEH_header_info *hinfo, Header information structure
  4.  int current_recursion_level,      Current recursion level
  5.  struct SS_object *ss ,                String stack containing already decoded file names
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_handle_rfc822( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *h, int current_recursion_level, struct SS_object *ss )
{
    /** Decode a RFC822 encoded stream of data from *f  **/
    int result = 0;
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_rfc822:DEBUG: Decoding RFC822 message\n",FL);
    /** If there is no filename, then we have a "standard"
     ** embedded message, which can be just read off as a
     ** continuous stream (simply with new boundaries
     **/
    DMIME LOGGER_log("%s:%d:MIME_handle_rfc822:DEBUG: Filename='%s', encoding = %d",FL, h->filename, h->content_transfer_encoding);

    //  if ((0)&&( h->content_transfer_encoding != _CTRANS_ENCODING_B64)&&( h->filename[0] == '0' )) 20041217-1635:PLD:
    if (( h->content_transfer_encoding != _CTRANS_ENCODING_B64)&&( h->filename[0] == '\0' ))
    {
        /** Handle a simple plain text wrapped RFC822 email with no encoding applied to it **/
        char *p;

        // If this is a simple 'wrapped' RFC822 email which has no encoding applied to it
        //      (ie, it's just the existing email with a new set of headers around it
        //      then rather than saving it to a file, we'll just peel off these outter
        //      layer of headers and get into the core of the message.  This is why we
        //      call unpack_stage2() because this function just reads the next set of
        //      headers and decodes.
        DMIME LOGGER_log("%s:%d:MIME_handle_rfc822:DEBUG: Non base64 encoding AND no filename, embedded message\n",FL);
        h->boundary_located = 0;
        result = MIME_unpack_stage2(f, unpackdir, h, current_recursion_level , ss);
        p = BS_top();
        if (p) PLD_strncpy(h->boundary, p,sizeof(h->boundary));
    } else {
        /** ...else... if the section has a filename or B64 type encoding, we need to put it through extra decoding **/
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_rfc822:DEBUG: Embedded message has a filename, decoding to file %s",FL,h->filename);
        result = MIME_decode_encoding( f, unpackdir, h, ss );
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_rfc822:DEBUG: Result of extracting %s is %d",FL,h->filename, result);
        if (result == 0) {
            /** Because we're calling MIME_unpack_single again [ie, recursively calling it
              we need to now adjust the input-filename so that it correctly is prefixed
              with the directory we unpacked to. **/

            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_rfc822:DEBUG: Now attempting to extract contents of '%s'",FL,h->filename);
            snprintf(scratch,sizeof(scratch),"%s/%s",unpackdir, h->filename);
            snprintf(h->filename,sizeof(h->filename),"%s",scratch);

            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_rfc822:DEBUG: Now attempting to extract contents of '%s'",FL,scratch);
            result = MIME_unpack_single( unpackdir, scratch, current_recursion_level, ss );
            result = 0;
        }
    } /** else-if transfer-encoding != B64 && filename was empty **/
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_rfc822:DEBUG: done handling '%s' result = %d",FL,h->filename, result);
    return result;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_handle_plain
  Returns Type  : int
  ----Parameter List
  1. FFGET_FILE *f,
  2.  char *unpackdir,
  3.  struct MIMEH_header_info *h,
  4.  int current_recursion_level,
  5.  struct SS_object *ss ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_handle_plain( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *h, int current_recursion_level, struct SS_object *ss )
{
    /** Handle a plain text encoded data stream from *f **/
    int result = 0;
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_handle_plain:DEBUG: Handling plain email",FL);
    result = MIME_decode_encoding( f, unpackdir, h, ss );
    if ((result == MIME_ERROR_FFGET_EMPTY)||(result == 0))
    {
        /** Test for RFC822 content... if so, go decode it **/
        snprintf(h->scratch,sizeof(h->scratch),"%s/%s",unpackdir,h->filename);
        if (MIME_is_file_RFC822(h->scratch)==1)
        {
            /** If the file is RFC822, then decode it using MIME_unpack_single() **/
            if (glb.header_longsearch != 0) MIMEH_set_header_longsearch(glb.header_longsearch);
            result = MIME_unpack_single( unpackdir, h->scratch, current_recursion_level, ss );
            if (glb.header_longsearch != 0) MIMEH_set_header_longsearch(0);
        }
    }
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_unpack_stage2 ID:1
Purpose:       This function commenced with the file decoding of the attachments
as required by the MIME structure of the file.
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_unpack_stage2( FFGET_FILE *f, char *unpackdir, struct MIMEH_header_info *hinfo, int current_recursion_level, struct SS_object *ss )
{
    int result = 0;
    struct MIMEH_header_info *h;
    char *p;

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Start, recursion %d\n",FL, current_recursion_level);
    if (current_recursion_level > glb.max_recursion_level)
    {
        /** Test for recursion limits **/
        if (MIME_VERBOSE) LOGGER_log("%s:%d:MIME_unpack_stage2:WARNING: Current recursion level of %d is greater than permitted %d"\
                ,FL, current_recursion_level, glb.max_recursion_level);
        return MIME_ERROR_RECURSION_LIMIT_REACHED; // 20040306-1301:PLD
    }
    h = hinfo;
    // Get our headers and determin what we have...
    //
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Parsing headers (initial)\n",FL);
    // Parse the headers, extracting what information we need
    /** 20041216-1102:PLD: Keep attempting to read headers until we get a sane set **/
    do {
        /** Read next set of headers, repeat until a sane set of headers are found **/
        result = MIMEH_parse_headers(f,h);
        DMIME LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Parsing of headers done, sanity = %d, result = %d",FL,h->sanity, result);
    } while ((h->sanity == 0)&&(result != -1));

    DMIME LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Repeat loop of header-reading done, sanity = %d, result = %d",FL,h->sanity, result);
    glb.header_defect_count += MIMEH_get_defect_count(h);
    // Test the result output
    switch (result) {
        case -1:
            return MIME_ERROR_FFGET_EMPTY;
            break;
    }
    // Copy the subject over to our global structure if the subject is not already set.
    //      this will give us the 'main' subject of the entire email and prevent
    //      and subsequent subjects from clobbering it.
    //if (glb.subject[0] == '\0') snprintf(glb.subject, _MIME_STRLEN_MAX, "%s", h->subject );
    if ((strlen(glb.subject) < 1)&&(h->subject != NULL)&&(strlen(h->subject) > 0))
    {
        snprintf(glb.subject, sizeof(glb.subject), "%s", h->subject );
    }
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Headers parsed, Result = %d, Boundary located = %d\n"\
            ,FL,result, hinfo->boundary_located);

    // Test to see if we encountered any DoubleCR situations while we
    //      were processing the headers.  If we did encounter a doubleCR
    //      then we need to decode that data and then reset the doubleCR
    //      flag.
    if (MIMEH_get_doubleCR() != 0)
    {
        MIME_doubleCR_decode( MIMEH_get_doubleCR_name(), unpackdir, h, current_recursion_level);
        MIMEH_set_doubleCR( 0 );
        FFGET_SDL_MODE = 0;
    }

    // If we located a boundary being specified (as apposed to existing)
    // then we push it to the BoundaryStack

    if (h->boundary_located)
    {
        char *lbc;
        char *fbc;

        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Boundary located, pushing to stack (%s)\n",FL,h->boundary);
        BS_push(h->boundary);

        // Get the first and last character positions of the boundary
        //      so we can test these for quotes.
        fbc = h->boundary;
        lbc = h->boundary +strlen(h->boundary) -1;

        // Circumvent attempts to trick the boundary
        //      Even though we may end up pushing 3 boundaries to the stack
        //      they will be popped off if they're not needed.
        if (*fbc == '"') { BS_push((fbc +1)); MIMEH_set_defect( hinfo, MIMEH_DEFECT_UNBALANCED_BOUNDARY_QUOTE); }
        if (*lbc == '"') { *lbc = '\0'; BS_push(h->boundary); *lbc = '"'; MIMEH_set_defect( hinfo, MIMEH_DEFECT_UNBALANCED_BOUNDARY_QUOTE); }
        h->boundary_located = 0;
    } else {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Decoding in BOUNDARY-LESS mode\n",FL);
        if (h->content_type == _CTYPE_RFC822)
        {
            // Pass off to the RFC822 handler
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Decoding with RFC822 decoder\n",FL);
            result = MIME_handle_rfc822(f,unpackdir,h,current_recursion_level,ss);
        } else if (MIMEH_is_contenttype(_CTYPE_MULTIPART, h->content_type)) {
            // Pass off to the multipart handler
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Decoding with Multipart decoder\n",FL);
            result = MIME_handle_multipart(f,unpackdir,h,current_recursion_level,ss);
        } else {
            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Decoding boundaryless file (%s)...\n",FL,h->filename);
            result = MIME_handle_plain( f, unpackdir,h,current_recursion_level,ss);
        } // else-if content was RFC822 or multi-part
        return result;
    } // End of the boundary-LESS mode ( processing the mail which has no boundaries in the primary headers )

    if ((BS_top()!=NULL)&&(result == 0))
    {
        // Commence decoding WITH boundary awareness.
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Decoding with boundaries (filename = %s)\n",FL,h->filename);

        // Decode the data in the current MIME segment
        // based on the header information retrieved from
        // the start of this function.
        result = MIME_decode_encoding(f, unpackdir, h, ss);
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Done decoding, result = %d",FL,result);
        if (result == 0)
        {
            if (BS_top()!=NULL)
            {
                // As this is a multipart email, then, each section will have its
                // own headers, so, we just simply call the MIMEH_parse call again
                // and get the attachment details
                while ((result == 0)||(result == MIME_STATUS_ZERO_FILE)||(result == MIME_ERROR_RECURSION_LIMIT_REACHED))
                {
                    h->content_type = -1;
                    h->filename[0] = '\0';
                    h->name[0]     = '\0';
                    h->content_transfer_encoding = -1;
                    h->content_disposition = -1;

                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Decoding headers...\n",FL);
                    do {
                        result = MIMEH_parse_headers(f,h);
                    } while ((h->sanity == 0)&&(result != -1));

                    glb.header_defect_count += MIMEH_get_defect_count(h);
                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Mime header parsing result = %d\n",FL, result);
                    // 20040305-1331:PLD
                    if (result == -1)
                    {
                        result = MIME_ERROR_FFGET_EMPTY;
                        return result;
                    }

                    if (h->boundary_located)
                    {
                        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Pushing boundary %s\n",FL, h->boundary);
                        BS_push(h->boundary);
                        h->boundary_located = 0;
                    }

                    if (result == _MIMEH_FOUND_FROM)
                    {
                        return _MIMEH_FOUND_FROM;
                    }

                    if (result == 0)
                    {
                        // If we locate a new boundary specified, it means we have a
                        // embedded message, also if we have a ctype of RFC822
                        if ( (h->boundary_located) \
                                || (h->content_type == _CTYPE_RFC822)\
                                || (MIMEH_is_contenttype(_CTYPE_MULTIPART, h->content_type))\
                            )
                        {
                            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Multipart/RFC822 mail headers found\n",FL);
                            /* If there is no filename, then we have a "standard"
                             * embedded message, which can be just read off as a
                             * continuous stream (simply with new boundaries */
                            if (( h->content_type == _CTYPE_RFC822 ))
                            {
                                // If the content_type is set to message/RFC822
                                // then we simply read off the data to the next
                                // boundary into a seperate file, then 'reload'
                                // the file into ripMIME.  Certainly, this is not
                                // the most efficent way of dealing with nested emails
                                // however, it is a rather robust/reliable way.
                                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Chose Content-type == RFC822 clause",FL);
                                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Calling MIME_decode_encoding()",FL);
                                result = MIME_handle_rfc822(f,unpackdir,h,current_recursion_level,ss);
                                // First up - extract the RFC822 body out of the parent mailpack
                                // XX result = MIME_decode_encoding( f, unpackdir, h, ss );

                                // Now decode the RFC822 body.
                                /**
                                  XX
                                  if (result == 0)
                                  {
                                  snprintf(scratch,sizeof(scratch),"%s/%s",unpackdir, h->filename);
                                  snprintf(h->filename,sizeof(h->filename),"%s",scratch);
                                  if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Now calling MIME_unpack_single() on the file '%s' for our RFC822 decode operation.",FL, scratch);
                                //result = MIME_unpack_single( unpackdir, h->filename, current_recursion_level +1, ss);
                                result = MIME_unpack( unpackdir, h->filename, current_recursion_level +1  );
                                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Unpack result = %d", FL, result);
                                result = 0;
                                } else return result; // 20040305-1312:PLD
                                */
                            } else if (( h->content_transfer_encoding != _CTRANS_ENCODING_B64)&&(h->filename[0] == '\0' )) {

                                // Decode nameless MIME segments which are not BASE64 encoded
                                //
                                // To be honest, i've forgotten what this section of test is supposed to pick out
                                // in terms of files - certainly it does pick out some, but I'm not sure what format
                                // they are.  Shame on me for not remembering, in future I must comment more.
                                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: NON-BASE64 DECODE\n",FL);
                                h->boundary_located = 0;
                                result = MIME_unpack_stage2(f, unpackdir, h, current_recursion_level , ss);

                                // When we've exited from decoding the sub-mailpack, we need to restore the original
                                // boundary
                                p = BS_top();
                                if (p) snprintf(h->boundary,_MIME_STRLEN_MAX,"%s",p);

                                /** 20041207-0106:PLD:
                                 ** changed to test for _INLINE **/
                                //} else if (( h->content_type = _CTYPE_MULTIPART_APPLEDOUBLE )&&(h->content_disposition != _CDISPOSITION_INLINE)) {
                        } else if (( h->content_type = _CTYPE_MULTIPART_APPLEDOUBLE )&&(h->content_disposition != _CDISPOSITION_INLINE)) {

                            // AppleDouble needs to be handled explicity - as even though it's
                            //      and embedded format, it does not have the normal headers->[headers->data]
                            //      layout of other nested emails

                            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Handle Appledouble explicitly",FL);
                            result = MIME_unpack_stage2(f, unpackdir, h, current_recursion_level , ss);

                        } else {
                            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: RFC822 Message to be decoded...\n",FL);
                            result = MIME_decode_encoding( f, unpackdir, h, ss );
                            if (result != 0) return result; // 20040305-1313:PLD
                            else
                            {
                                if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Now running ripMIME over decoded RFC822 message...\n",FL);

                                // Because we're calling MIME_unpack_single again [ie, recursively calling it
                                // we need to now adjust the input-filename so that it correctly is prefixed
                                // with the directory we unpacked to.

                                snprintf(scratch,sizeof(scratch),"%s/%s",unpackdir, h->filename);
                                snprintf(h->filename,sizeof(h->filename),"%s",scratch);
                                //result = MIME_unpack_single( unpackdir, h->filename, current_recursion_level +1, ss);
                                result = MIME_unpack_single( unpackdir, h->filename, current_recursion_level,ss );
                            }

                        } // else-if transfer-encoding wasn't B64 and filename was blank
                        } else {
                            // If the attachment included in this MIME segment is NOT a
                            // multipart or RFC822 embedded email, we can then simply use
                            // the normal decoding function to interpret its data.
                            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Decoding a normal attachment \n",FL);
                            result = MIME_decode_encoding( f, unpackdir, h, ss );

                            if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Decoding a normal attachment '%s' done. \n",FL, h->filename);
                            // See if we have an attachment output which is actually another
                            //      email.
                            //
                            // Added 24 Aug 2003 by PLD
                            //      Ricardo Kleemann supplied offending mailpack to display
                            //      this behavior
                            if (result != 0) return result; // 20040305-1314:PLD
                            else
                            {
                                char *mime_fname;

                                mime_fname = PLD_dprintf("%s/%s", unpackdir, h->filename);
                                if(mime_fname != NULL)
                                {
                                    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Testing '%s' for email type",FL,mime_fname);
                                    if (MIME_is_file_RFC822(mime_fname))
                                    {
                                        //MIME_unpack_single( unpackdir, mime_fname, (hinfo->current_recursion_level+1), ss);
                                        MIME_unpack_single( unpackdir, mime_fname, current_recursion_level+1,ss);
                                    }
                                    free(mime_fname);
                                }
                            }
                        } // if there was a boundary, RFC822 content or it was multi-part
                    } else {
                        // if the result is not 0
                        break;
                    } // result == 0 test
                } // While (result)

                // ???? Why do we bother to pop the stack ???
                //  The way BS is designed it will auto-pop the inner nested boundaries
                //      when a higher-up one is located.
                //if (result == 0) BS_pop(); // 20040305-2219:PLD
            } // if MIME_BS_top()
        } // if result == 0
    } // if (result)
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_stage2:DEBUG: Exiting with result=%d recursion=%d\n",FL,result, current_recursion_level);
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_decode_mailbox ID:1
Purpose:       Decodes mailbox formatted email files
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_unpack_mailbox( char *unpackdir, char *mpname, int current_recursion_level, struct SS_object *ss )
{
    FFGET_FILE f;
    FILE *fi;
    FILE *fo;
    char fname[1024];
    char line[1024];
    int mcount=0;
    int lastlinewasblank=1;
    int result;
    int input_is_stdin=0;

    snprintf(fname,sizeof(fname),"%s/tmp.email000.mailpack",unpackdir);
    if ((mpname[0] == '-')&&(mpname[1] == '\0'))
    {
        fi = stdin;
        input_is_stdin=1;
    } else {
        //      fi = fopen(mpname,"r");
        if (strcmp(mpname,"-")==0) fi = stdin; else fi = fopen(mpname,"r"); // 20040208-1715:PLD
        if (!fi)
        {
            LOGGER_log("%s:%d:MIME_unpack_mailbox:ERROR: Cannot open '%s' for reading (%s)",FL, mpname,strerror(errno));
            return -1;
        }
    }

    fo = fopen(fname,"w");
    if (!fo)
    {
        LOGGER_log("%s:%d:MIME_unpack_mailbox:ERROR: Cannot open '%s' for writing  (%s)",FL, fname,strerror(errno));
        return -1;
    }

    FFGET_setstream(&f, fi);
    while (FFGET_fgets(line,sizeof(line),&f))
    {
        // If we have the construct of "\n\rFrom ", then we
        //      can be -pretty- sure that a new email is about
        //      to start

        if ((lastlinewasblank==1)&&(strncasecmp(line,"From ",5)==0))
        {
            // Close the mailpack
            fclose(fo);
            // Now, decode the mailpack
            //MIME_unpack_single(unpackdir, fname, current_recursion_level, ss);
            // 20040317-2358:PLD
            MIME_unpack_single(unpackdir, fname, current_recursion_level ,ss );
            // Remove the now unpacked mailpack
            result = remove(fname);
            if (result == -1)
            {
                LOGGER_log("%s:%d:MIME_unpack_mailbox:ERROR: removing temporary mailpack '%s' (%s)",FL, fname,strerror(errno));
            }
            // Create a new mailpack filename, and keep on going...
            snprintf(fname,sizeof(fname),"%s/tmp.email%03d.mailpack",unpackdir,++mcount);
            fo = fopen(fname,"w");
        }
        else
        {
            fprintf(fo,"%s",line);
        }

        // If the line is blank, then note this down because
        //  if our NEXT line is a From, then we know that
        //      we have reached the end of the email
        //
        if ((line[0] == '\n') || (line[0] == '\r'))
        {
            lastlinewasblank=1;
        }
        else lastlinewasblank=0;
    } // While fgets()

    // Don't attempt to close STDIN if that's where the mailpack/mailbox
    //      has come from.  Although this should really cause problems,
    //      it's better to be safe than sorry.

    if (input_is_stdin == 0)
    {
        fclose(fi);
    }

    // Now, even though we have run out of lines from our main input file
    //  it DOESNT mean we dont have some more decoding to do, in fact
    //      quite the opposite, we still have one more file to decode
    // Close the mailpack
    fclose(fo);

    // Now, decode the mailpack
    //MIME_unpack_single(unpackdir, fname, current_recursion_level, ss);
    // 20040317-2358:PLD
    MIME_unpack_single(unpackdir, fname, current_recursion_level , ss );
    // Remove the now unpacked mailpack
    result = remove(fname);
    if (result == -1)
    {
        LOGGER_log("%s:%d:MIME_unpack_mailbox:ERROR: removing temporary mailpack '%s' (%s)",FL, fname,strerror(errno));
    }
    return 0;
}

/*-----------------------------------------------------------------\
  Function Name : MIME_unpack_single
  Returns Type  : int
  ----Parameter List
  1. char *unpackdir,
  2.  char *mpname,
  3.  int current_recursion_level ,
  ------------------
  Exit Codes    :
  Side Effects  :
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int MIME_unpack_single( char *unpackdir, char *mpname, int current_recursion_level, struct SS_object *ss )
{
    FILE *fi;           /* Pointer for the MIME file we're going to be going through */
    int result = 0;

    if (current_recursion_level > glb.max_recursion_level)
    {
        LOGGER_log("%s:%d:MIME_unpack_single:WARNING: Current recursion level of %d is greater than permitted %d",FL, current_recursion_level, glb.max_recursion_level);
        return MIME_ERROR_RECURSION_LIMIT_REACHED;
    }

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single:DEBUG: dir=%s packname=%s level=%d (max = %d)\n",FL, unpackdir, mpname, current_recursion_level, glb.max_recursion_level);
    /* if we're reading in from STDIN */
    if( mpname[0] == '-' && mpname[1] == '\0' )
    {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single:DEBUG: STDIN opened...\n",FL);
        fi = stdin;
    }
    else
    {
        fi = fopen(mpname,"r");
        if (!fi)
        {
            LOGGER_log("%s:%d:MIME_unpack_single:ERROR: Cannot open file '%s' for reading.\n",FL, mpname);
            return -1;
        }
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single:DEBUG: Input file (%s) opened...\n",FL, mpname);
    }
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single:DEBUG: Checking input streams...\n",FL);
    /* check to see if we had problems opening the file */
    if (fi == NULL)
    {
        LOGGER_log("%s:%d:MIME_unpack_single:ERROR: Could not open mailpack file '%s' (%s)",FL, mpname, strerror(errno));
        return -1;
    }
    // 20040317-2359:PLD
    result = MIME_unpack_single_fp(unpackdir,fi,current_recursion_level , ss);
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single:DEBUG: result = %d, recursion = %d, filename = '%s'", FL, result, current_recursion_level, mpname );
    if ((current_recursion_level > 1)&&(result == 241)) result = 0;
    fclose(fi);
    return result;
}

/*------------------------------------------------------------------------
Procedure:     MIME_unpack_single ID:1
Purpose:       Decodes a single mailpack file (as apposed to mailbox format) into its
possible attachments and text bodies
Input:         char *unpackdir: Directory to unpack the attachments to
char *mpname: Name of the mailpack we have to decode
int current_recusion_level: Level of recursion we're currently at.
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_unpack_single_fp( char *unpackdir, FILE *fi, int current_recursion_level, struct SS_object *ss )
{
    struct MIMEH_header_info h;
    int result = 0;
    int headers_save_set_here = 0;

    FFGET_FILE f;
    FILE *hf = NULL;
    // Because this MIME module gets used in both CLI and daemon modes
    //  we should check to see that we can report to stderr
    //
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single_fp:DEBUG: dir=%s level=%d (max = %d)\n",FL, unpackdir, current_recursion_level, glb.max_recursion_level);
    if (current_recursion_level > glb.max_recursion_level)
    {
        LOGGER_log("%s:%d:MIME_unpack_single_fp:WARNING: Current recursion level of %d is greater than permitted %d",FL, current_recursion_level, glb.max_recursion_level);
        //      return -1;
        return MIME_ERROR_RECURSION_LIMIT_REACHED; // 20040305-1302:PLD
        //return 0; // 20040208-1723:PLD
    }
    else h.current_recursion_level = current_recursion_level;
    glb.current_line = 0;
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single_fp:DEBUG: recursion level checked...%d\n",FL, current_recursion_level);
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single_fp:DEBUG: DumpHeaders = %d\n",FL, glb.save_headers);
    if ((!hf)&&(glb.save_headers)&&(MIMEH_get_headers_save()==0))
    {
        // Prepend the unpackdir path to the headers file name
        snprintf(scratch,sizeof(scratch),"%s/%s",unpackdir, glb.headersname);
        hf = fopen(scratch,"w");
        if (!hf)
        {
            glb.save_headers = 0;
            LOGGER_log("%s:%d:MIME_unpack_single_fp:ERROR: Cannot open '%s' for writing  (%s)",FL, glb.headersname,strerror(errno));
        }
        else
        {
            headers_save_set_here = 1;
            MIMEH_set_headers_save(hf);
        }
    }

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single_fp:DEBUG: Setting up streams to decode\n",FL);
    FFGET_setstream(&f, fi);
    /** Initialize the header record **/
    h.boundary[0] = '\0';
    h.boundary_located = 0;
    h.filename[0] = '\0';
    h.name[0]     = '\0';
    h.content_transfer_encoding = -1;
    h.content_disposition = -1;
    h.content_type = -1;
    h.x_mac = 0;
    SS_init(&(h.ss_filenames));
    SS_init(&(h.ss_names));
    if (MIME_DNORMAL) { SS_set_debug(&(h.ss_filenames), 1); SS_set_debug(&(h.ss_names), 1); }
    if (glb.verbose_defects) {
        int i;
        for (i = 0; i < _MIMEH_DEFECT_ARRAY_SIZE; i++)
        {
            h.defects[i] = 0;
        }
    }

    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single_fp:DEBUG: preparing to decode, calling stage2...\n",FL);
    // 20040318-0001:PLD
    result = MIME_unpack_stage2(&f, unpackdir, &h, current_recursion_level +1, ss);
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single_fp:DEBUG: done decoding ( in stage2 ) result=%d, to %s\n",FL, result, unpackdir);
    //  fclose(fi); 20040208-1726:PLD
    if ( headers_save_set_here > 0 )
    {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single_fp:DEBUG: Closing header file.\n",FL);
        fflush(stdout);
        MIMEH_set_headers_nosave();
        fclose(hf);
    }

    if (glb.verbose_defects) MIMEH_dump_defects(&h);
    /** Flush out the string stacks **/
    SS_done(&(h.ss_filenames));
    SS_done(&(h.ss_names));
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack_single_fp:DEBUG: Done. Result=%d Recursion=%d\n",FL, result, current_recursion_level);
    return result;
    //  return status; // 20040305-1318:PLD
}

/*------------------------------------------------------------------------
Procedure:     MIME_unpack ID:1
Purpose:       Front end to unpack_mailbox and unpack_single.  Decides
which one to execute based on the mailbox setting
Input:
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_unpack( char *unpackdir, char *mpname, int current_recursion_level )
{
    int result = 0;
    struct SS_object ss; // Stores the filenames that are created in the unpack operation

    if (current_recursion_level > glb.max_recursion_level) return MIME_ERROR_RECURSION_LIMIT_REACHED;
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack: Unpacking %s to %s, recursion level is %d",FL,mpname,unpackdir,current_recursion_level);
    MIMEH_set_outputdir(unpackdir);
    if (MIME_DNORMAL) SS_set_debug(&ss,1);
    SS_init(&ss);
    if (glb.mailbox_format > 0)
    {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack: Unpacking using mailbox format",FL);
        result = MIME_unpack_mailbox( unpackdir, mpname, (current_recursion_level), &ss );
    }
    else
    {
        if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack: Unpacking standard mailpack",FL,mpname,unpackdir,current_recursion_level);
        result = MIME_unpack_single( unpackdir, mpname, (current_recursion_level +1), &ss );
    }

    if (glb.no_nameless)
    {
        MIME_postdecode_cleanup( unpackdir, &ss );
    }

    if (MIME_DNORMAL)
    {
        LOGGER_log("%s:%d:MIME_unpack: Files unpacked from '%s' (recursion=%d);",FL,mpname,current_recursion_level);
        SS_dump(&ss);
    }

    SS_done(&ss);
    if (MIME_DNORMAL) SS_set_debug(&ss,1);
    // If the result is a non-critical one (ie, just running out of data from the
    //      file we attempted to decode - then don't propergate it, on the other
    switch (result) {
        case 1:
            result = 0;
            break;
        case MIME_STATUS_ZERO_FILE:
            result = 0;
            break;
        case MIME_ERROR_FFGET_EMPTY:
        case MIME_ERROR_RECURSION_LIMIT_REACHED:
            result = 0;
            break;
        default:
            break;
    }

    if (current_recursion_level == 0)
    {
        //LOGGER_log("%s:%d:MIME_unpack:DEBUG: Clearing boundary stack",FL);
        BS_clear();
    }
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack: Unpacking of %s is done.",FL,mpname);
    if (MIME_DNORMAL) LOGGER_log("%s:%d:MIME_unpack: -----------------------------------",FL);
    return result;
}

/*--------------------------------------------------------------------
 * MIME_close
 *
 * Closes the files used in MIME_unpack, such as headers etc */
int MIME_close( void )
{
    if (headers)
    {
        fclose(headers);
    }
    return 0;
}
/*----------END OF MIME.c------------*/
