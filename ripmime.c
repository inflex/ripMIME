/*----------------------------------------
 * ripMIME -
 *
 * Written by Paul L Daniels
 * pldaniels@pldaniels.com
 *
 * (C)2001 P.L.Daniels
 * http://www.pldaniels.com/ripmime
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <dirent.h>

#include "buildcodes.h"
#include "logger.h"
#include "ffget.h"
#include "mime.h"
#include "strstack.h"
#include "MIME_headers.h"


#define RIPMIME_ERROR_CANT_CREATE_OUTPUT_DIR 1
#define RIPMIME_ERROR_CANT_OPEN_INPUT_FILE 2
#define RIPMIME_ERROR_NO_INPUT_FILE 3
#define RIPMIME_ERROR_INSUFFICIENT_PARAMETERS 4
#define RIPMIME_ERROR_TIMEOUT 5


struct RIPMIME_globals
{
	char *inputfile;
	char *dir;
	int use_return_codes;
	int timeout;
	int quiet;
	int verbose_defects;
	int verbose;
};

//-b [blankzone file name] : Dump the contents of the MIME blankzone to 'filename'


char defaultdir[] = ".";
struct RIPMIME_globals *ripmime_globals = NULL;

char version[] = "v1.4.0.9 - November 07, 2008 (C) PLDaniels http://www.pldaniels.com/ripmime";
char help[] = "ripMIME -i <mime file> -d <directory>"
	"[-p prefix] [-e [header file]] [-vVh] [--version]"
	"[--no_nameless] [--unique_names [--prefix|--postfix|--infix]]"
	"[--paranoid] [--mailbox] [--formdata] [--debug]"
	"[--no-tnef] [--no-quotedprintable] [--no-uudecode]\n"
	"Options available :\n"
	"-i : Input MIME encoded file (use '-' to input from STDIN)\n"
	"\tIf <mime file> is a directory, it will be recursed\n"
	"-d : Output directory\n"
	"-p : Specify prefix filename to be used on files without a filename (default 'text')\n"
	"-e [headers file name] : Dump headers from mailpack (default '_headers_')\n"
	"-v : Turn on verbosity\n"
	"-q : Run quietly, do no report non-fatal errors\n"
	"\n"
	"--verbose-contenttype : Turn on verbosity of file content type\n"
	"--verbose-oldstyle : Uses the v1.2.x style or filename reporting\n"
	"--verbose-defects: Display a summary of defects in the email\n"
	"--verbose-mime: Displays 'Email is MIME' if the email is MIME\n"
	"--stdout : All reporting goes to stdout (Default)\n"
	"--stderr : All reporting goes to stderr\n"
	"--syslog : All reporting goes to syslog\n"
	"\n"
	"--no-paranoid : [ Deprecated ] Turns off strict ascii-alnum filenaming\n"
	"--paranoid: Converts all filenames to strict 7-bit compliance\n"
	"--name-by-type: Saves a given attachment by its content-type if it has no other name\n"
	"--no-nameless : Do not save nameless attachments\n"
	"--overwrite : Overwrite files if they have the same name on extraction\n"
	"--unique-names : Dont overwrite existing files (This is the default behaviour)\n"
	"--prefix : rename by putting unique code at the front of the filename\n"
	"--postfix : rename by putting unique code at the end of the filename\n"
	"--infix : rename by putting unique code in the middle of the filename\n"
	"\n"
	"--mailbox : Process mailbox file\n"
	"--formdata : Process as form data (from HTML form etc)\n"
	"\n"
	"--no-tnef : Turn off TNEF/winmail.dat decoding\n"
	"--no-ole : Turn off OLE decoding\n"
	"--no-uudecode : Turns off the facility of detecting UUencoded attachments in emails\n"
	"--no-quotedprintable : Turns off the facility of decoding QuotedPrintable data\n"
	"--no-doublecr : Turns off saving of double-CR embedded data\n"
	"--no-mht : Turns off MHT (a Microsoft mailpack attachment format ) decoding\n"
	"--no-multiple-filenames : Turns off the multiple filename exploit handling\n"
	"\n"
	"--disable-header-fix : Turns off attempts to fix broken headers\n"
	"--disable-qmail-bounce : Turns off qmail bounced email testing\n"
	"--recursion-max <level> : Set the maximum recursion level to 'level'\n"
	"--timeout <seconds>	: Set the maximum number of CPU seconds ripMIME can run for\n"
	"\n"
	"--debug : Produces detailed information about the whole decoding process\n"
	"--extended-errors:	Produces more return codes, even for non-fatals\n"
	"-V --version : Give version information\n"
	"--buildcodes : Give the build information (tstamp, date and system information)\n"
	"-h : This message (help)\n\n\n";

/*-----------------------------------------------------------------\
 Function Name	: RIPMIME_report_filename_decoded
 Returns Type	: int
 	----Parameter List
	1. char *filename, 
	2.  char *contenttype, 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int RIPMIME_report_filename_decoded (char *filename, char *contenttype)
{
	char *p;

	p = strrchr(filename,'/');
	if (!p) p = filename; else p++;

	if (contenttype != NULL)
	{
		LOGGER_log ("Decoding content-type=%s filename=%s", contenttype, p);
	}
	else
	{
		LOGGER_log ("Decoding filename=%s", p);
	}

	return 0;
}



/*-----------------------------------------------------------------\
 Function Name	: RIPMIME_parse_parameters
 Returns Type	: int
 	----Parameter List
	1. struct RIPMIME_globals *glb, 
	2.  int argc, 
	3.  char **argv, 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int RIPMIME_parse_parameters (struct RIPMIME_globals *glb, int argc, char **argv)
{
	int i;
	int result = 0;

	MIME_set_filename_report_fn (RIPMIME_report_filename_decoded);

	for (i = 1; i < argc; i++)
	{
		// if the first char of the argument is a '-', then we possibly have a flag

		if (argv[i][0] == '-')
		{
			// test the 2nd char of the parameter

			switch (argv[i][1])
			{
				case 'i':
					if (argv[i][2] != '\0')
					{
						glb->inputfile = &argv[i][2];
					}
					else
					{
						i++;
						if (i < argc)
						{
							glb->inputfile = argv[i];
						}
						else
						{
							LOGGER_log("ERROR: insufficient parameters after '-i'\n");
						}
					}
					break;

				case 'd':
					if (argv[i][2] != '\0')
					{
						glb->dir = &(argv[i][2]);
					}
					else
					{
						i++;
						if (i < argc)
						{
							glb->dir = argv[i];
						}
						else
						{
							LOGGER_log("ERROR: insufficient parameters after '-d'\n");
						}
					}

					break;

				case 'p':
					if (argv[i][2] != '\0')
					{
						MIME_set_blankfileprefix (&argv[i][2]);
					}
					else
					{
						i++;
						if (i < argc)
						{
							MIME_set_blankfileprefix (argv[i]);
						}
						else
						{
							LOGGER_log("ERROR: insufficient parameters after '-p'\n");
						}
					}
					break;		// this is in mime.h

				case 'e':
					MIME_set_dumpheaders (1);
					if (argv[i][2] != '\0')
					{
						MIME_set_headersname (&argv[i][2]);
					}
					else
					{
						if ((i < (argc - 1)) && (argv[i + 1][0] != '-'))
							MIME_set_headersname (argv[++i]);
					}
					break;		// makes MIME dump out the headers to a file
#ifdef RIPMIME_BLANKZONE
				case 'b':
					if (argv[i][2] != '\0')
					{
						MIME_set_blankzone_filename (&argv[i][2]);
					}
					else
					{
						if ((i < (argc - 1)) && (argv[i + 1][0] != '-'))
							MIME_set_blankzone_filename (argv[++i]);
					}
					break;		// blankzone storage option
#endif

				case 'v':
					MIME_set_verbosity (1);
					glb->verbose = 1;
					break;

				case 'q':
					glb->quiet = 1;
					MIME_set_quiet(glb->quiet);
					break;

				case 'V':
					fprintf (stdout, "%s\n", version);
					exit (1);
					break;
				case 'h':
					fprintf (stdout, "%s\n", help);
					exit (1);
					break;

					// if we get ANOTHER - symbol, then we have an extended flag

				case '-':
					if (strncmp (&(argv[i][2]), "verbose-contenttype", strlen ("verbose-contenttype")) == 0) {
						MIME_set_verbosity_contenttype (1);

					/** 20051117-0927:PLD: 
					 ** If client uses --verbose-mime, then make ripMIME report when it
					 ** detects a MIME encoded email **/
					} else if (strncmp(&(argv[i][2]), "verbose-mime", strlen("verbose-mime"))==0) {
						MIME_set_report_MIME(1);

					} else if (strncmp (&(argv[i][2]), "verbose-oldstyle", strlen ("verbose-oldstyle")) == 0) {
							MIME_set_verbosity_12x_style (1);
							MIME_set_filename_report_fn (NULL);
						}
						else if (strncmp (&(argv[i][2]), "verbose-defects",15) == 0)
						{
							glb->verbose_defects = 1;
							MIME_set_verbose_defects(1);
						}
						else if (strncmp (&(argv[i][2]), "paranoid", 8) == 0)
						{
							MIME_set_paranoid (1);
						}
						else if (strncmp (&(argv[i][2]), "no_paranoid", 11) == 0)
						{
							MIME_set_paranoid (0);
						}
						else if (strncmp (&(argv[i][2]), "no-paranoid", 11) == 0)
						{
							MIME_set_paranoid (0);
						}
						else if (strncmp (&(argv[i][2]), "prefix", 6) == 0)
						{
							MIME_set_renamemethod (_MIME_RENAME_METHOD_PREFIX);
						}
						else if (strncmp (&(argv[i][2]), "postfix", 7) == 0)
						{
							MIME_set_renamemethod (_MIME_RENAME_METHOD_POSTFIX);
						}
						else if (strncmp (&(argv[i][2]), "infix", 5) == 0)
						{
							MIME_set_renamemethod (_MIME_RENAME_METHOD_INFIX);
						}
						else if (strncmp (&(argv[i][2]), "overwrite", 9) == 0)
						{
							MIME_set_uniquenames (0);
						}
						else if (strncmp (&(argv[i][2]), "unique_names", 12) == 0)
						{
							MIME_set_uniquenames (1);
						}
						else if (strncmp (&(argv[i][2]), "unique-names", 12) == 0)
						{
							MIME_set_uniquenames (1);
						}
						else if (strncmp(&(argv[i][2]), "name-by-type", 12) == 0)
						{
							MIME_set_name_by_type(1);
						}
						else if (strncmp (&(argv[i][2]), "syslog", 9) == 0)
						{
							LOGGER_set_output_mode (_LOGGER_SYSLOG);
							LOGGER_set_syslog_mode (LOG_MAIL | LOG_INFO);
						}
						else if (strncmp (&(argv[i][2]), "stderr", 10) == 0)
						{
							LOGGER_set_output_mode (_LOGGER_STDERR);
						}
						else if (strncmp (&(argv[i][2]), "stdout", 9) == 0)
						{
							LOGGER_set_output_mode (_LOGGER_STDOUT);
						}
						else if (strncmp (&(argv[i][2]), "no_nameless", 11) == 0)
						{
							MIME_set_no_nameless (1);
						}
						else if (strncmp (&(argv[i][2]), "no-nameless", 11) == 0)
						{
							MIME_set_no_nameless (1);
						}
						else if (strncmp (&(argv[i][2]), "debug", 5) == 0)
						{
							MIME_set_debug (1);
						}
						else if (strncmp (&(argv[i][2]), "mailbox", 7) == 0)
						{
							MIME_set_mailboxformat (1);
						}
						else if (strncmp(&(argv[i][2]), "formdata", 8) == 0)
						{
							// Form data usually contains embedded \0 sequences
							//		so we need to explicitly turn this off.
							FFGET_set_allow_nul(1);
						}
						else if (strncmp (&(argv[i][2]), "no_uudecode", 11) == 0)
						{
							// We are transitioning away from negative-logic function
							//	names because they can cause confusion when reading, so
							// from here on, we will use things like _set_foo() rather
							// than _set_no_foo()

							MIME_set_decode_uudecode(0);
						}
						else if (strncmp (&(argv[i][2]), "no-uudecode", 11) == 0)
						{
							MIME_set_decode_uudecode(0);
						}
						else if (strncmp (&(argv[i][2]), "no-tnef", 7) == 0)
						{
							MIME_set_decode_tnef (0);
						}
						else if (strncmp (&(argv[i][2]), "no-ole", 6) == 0)
						{
							MIME_set_decode_ole(0);
						}
						else if (strncmp (&(argv[i][2]), "no-base64", 9) == 0)
						{
							MIME_set_decode_base64(0);
						}
						else if (strncmp (&(argv[i][2]), "no-quotedprintable", strlen("no-quotedprintable")) == 0) 
						{
							MIME_set_decode_qp(0);
						}
						else if (strncmp(&(argv[i][2]), "no-doublecr", strlen("no-doublecr")) == 0)
						{
							MIME_set_decode_doubleCR(0);
						}
						else if (strncmp(&(argv[i][2]), "no-mht", strlen("no-mht")) == 0)
						{
							MIME_set_decode_mht(0);
						}
					else if (strncmp(&(argv[i][2]), "disable-header-fix", strlen("disable-headerfix")) == 0) {
							MIMEH_set_headerfix(0);
					}
						else if (strncmp(&(argv[i][2]), "qmail-bounce", strlen("qmail-bounce")) == 0)
						{
							MIME_set_header_longsearch(1);
						}
						else if (strncmp(&(argv[i][2]), "disable-qmail-bounce", strlen("disable-qmail-bounce")) == 0)
						{
							MIME_set_header_longsearch(0);
						}
						else if (strncmp(&(argv[i][2]), "no-multiple-filenames", strlen("no-multiple-filenames")) == 0)
						{
							MIME_set_multiple_filenames(0);
						}
						else if (strncmp(&(argv[i][2]), "recursion-max", strlen("recursion-max")) == 0)
						{
							if (argv[i+1] != NULL)
							{
								int level;

								level = atoi(argv[i+1]);
								if (level > 0)
								{
									MIME_set_recursion_level(level);
								}
							}
						}
						else if (strncmp(&(argv[i][2]), "timeout", strlen("timeout")) == 0)
						{
							if (argv[i+1] != NULL)
							{
								int seconds;

								seconds = atoi(argv[i+1]);
								if (seconds > 0)
								{
									glb->timeout = seconds;
								}
							}
						}
						else if (strncmp (&(argv[i][2]), "buildcodes", 10) == 0)
						{
							fprintf(stdout,"%s\n%s\n%s\n", BUILD_CODE, BUILD_DATE, BUILD_BOX);
							exit(0);
						}
						else if (strncmp (&(argv[i][2]), "version", 7) == 0)
						{
							fprintf (stdout, "%s\n", version);
							exit (0);
						}
						else if (strncmp(&(argv[i][2]), "extended-errors", strlen("extended-errors")) == 0)
						{
							glb->use_return_codes = 1;
						}
						else
						{
							LOGGER_log ("Cannot interpret option \"%s\"\n%s\n", argv[i],
									help);
							exit (1);
							break;
						}
					break;

					// else, just dump out the help message

				default:
					LOGGER_log ("Cannot interpret option \"%s\"\n%s\n", argv[i],
							help);
					exit (1);
					break;

			}			// Switch argv[i][1]

		}			// if argv[i][0] == -

	}				// for

	return result;
}



/*-----------------------------------------------------------------\
 Function Name	: RIPMIME_init
 Returns Type	: int
 	----Parameter List
	1. struct RIPMIME_globals *glb, 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int RIPMIME_init (struct RIPMIME_globals *glb)
{
	glb->dir = defaultdir;
	glb->inputfile = NULL;
	glb->use_return_codes = 0;
	glb->timeout = 0;
	glb->quiet = 0;
	glb->verbose_defects = 0;
	glb->verbose = 0;

	return 0;
}

/*-----------------------------------------------------------------\
 Function Name	: RIPMIME_signal_alarm
 Returns Type	: void
 	----Parameter List
	1. int sig , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
void RIPMIME_signal_alarm( int sig )
{
	if (ripmime_globals->quiet == 0) LOGGER_log("%s:%d:RIPMIME_signal_alarm: ripMIME took too long to complete. Mailpack is \"%s\", output dir is \"%s\"",FL, ripmime_globals->inputfile, ripmime_globals->dir );
	exit(RIPMIME_ERROR_TIMEOUT);
}


/*-----------------------------------------------------------------\
 Function Name	: RIPMIME_unpack_single
 Returns Type	: int
 	----Parameter List
	1. struct RIPMIME_globals *glb, 
	2.  char *fname , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int RIPMIME_unpack_single( struct RIPMIME_globals *glb, char *fname )
{
	int result = 0;

	/** Set the alarm timeout feature. **/
	if (glb->timeout > 0) {
		signal(SIGALRM, RIPMIME_signal_alarm);
		alarm(glb->timeout);
	}

	MIMEH_set_outputdir (glb->dir);
	result = MIME_unpack (glb->dir, fname, 0);

	// do any last minute things

	MIME_close ();

	return result;
}

/*-----------------------------------------------------------------\
 Function Name	: RIPMIME_unpack
 Returns Type	: int
 	----Parameter List
	1. struct RIPMIME_globals *glb , 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int RIPMIME_unpack( struct RIPMIME_globals *glb )
{
	struct stat st;
	int stat_result = 0;
	int result = 0;
	int input_is_directory = 0;

	/** If we're not inputting from STDIN, check to see if the
	  ** input is a directory **/
	if (strcmp(glb->inputfile,"-")!=0) {
		stat_result = stat(glb->inputfile, &st);
		if (stat_result != 0) return -1;

		if (S_ISDIR(st.st_mode)) input_is_directory = 1;
	}

	if (input_is_directory == 1) {
		/** Unpack all files in directory **/
		DIR *dir;
		struct dirent *dir_entry;

		fprintf(stderr,"input file is a directory, recursing\n");
		dir = opendir(glb->inputfile);
		if (dir == NULL) return -1;
		
		do {
			/** Check every entry in the directory provided and if it's a file
			  ** try to unpack it **/
			char fullfilename[1024];

			dir_entry = readdir( dir );
			if (dir_entry == NULL) break;

			if (strcmp(dir_entry->d_name, ".")==0) continue;
			if (strcmp(dir_entry->d_name, "..")==0) continue;

			snprintf(fullfilename,sizeof(fullfilename),"%s/%s", glb->inputfile, dir_entry->d_name);
			stat_result = stat( fullfilename, &st );
			if (stat_result != 0) continue;
			if (S_ISREG(st.st_mode)) {
				/** If the directory entry is a file, then unpack it **/
				fprintf(stdout,"Unpacking mailpack %s\n",fullfilename);
				result = RIPMIME_unpack_single(glb, fullfilename);
			}
		} while (dir_entry != NULL); /** While more files in the directory **/

		closedir( dir );

	} else {
		/** If the supplied file was actually a normal file, then decode normally **/
		result = RIPMIME_unpack_single( glb, glb->inputfile );
	}

	return result;
}

/*-----------------------------------------------------------------\
 Function Name	: main
 Returns Type	: int
 	----Parameter List
	1. int argc, 
	2.  char **argv, 
 	------------------
 Exit Codes	: 
 Side Effects	: 
--------------------------------------------------------------------
 Comments:
 
--------------------------------------------------------------------
 Changes:
 
\------------------------------------------------------------------*/
int main (int argc, char **argv)
{
	struct RIPMIME_globals glb;
	int result = 0;

	/* if the user has just typed in "ripmime" and nothing else, then we had better give them
	 * the rundown on how to use this program */

	if (argc < 2)
	{
		fprintf (stderr, "%s\n%s", version, help);
		return RIPMIME_ERROR_INSUFFICIENT_PARAMETERS;
	}

	// Set the global pointer ripmime_globals to point to
	//		the glb struct, so that if we have a timeout condition
	//		we can use the data
	ripmime_globals = &glb;


	// Set up our initial logging mode - so that we can always get
	//              report messages if need be.

	LOGGER_set_output_mode (_LOGGER_STDOUT);

	// Perform system initialisations

	MIME_init ();
	RIPMIME_init (&glb);

	// Setup our default behaviours */

	MIME_set_uniquenames (1);
	MIME_set_paranoid (0);
	MIME_set_header_longsearch(1); // 20040310-0117:PLD - Added by default as it seems stable, use --disable-qmail-bounce to turn off
	MIME_set_renamemethod (_MIME_RENAME_METHOD_INFIX);


	RIPMIME_parse_parameters (&glb, argc, argv);


	// if our input filename wasn't specified, then we better let the user know!
	if (!glb.inputfile)
	{
		LOGGER_log("Error: No input file was specified\n");
		return RIPMIME_ERROR_NO_INPUT_FILE;
	}

	// Fire up the randomizer

	srand (time (NULL));

	// clean up the output directory name if required (remove any trailing /'s, as suggested by James Cownie 03/02/2001

	if (glb.dir[strlen (glb.dir) - 1] == '/')
	{
		glb.dir[strlen (glb.dir) - 1] = '\0';
	}

	// Create the output directory required as specified by the -d parameter

	if (glb.dir != defaultdir)
	{
		result = mkdir (glb.dir, S_IRWXU);

		// if we had a problem creating a directory, and it wasn't just
		// due to the directory already existing, then we have a bit of
		// a problem on our hands, hence, report it.
		//

		if ((result == -1) && (errno != EEXIST))
		{
			LOGGER_log("ripMIME: Cannot create directory '%s' (%s)\n",
					glb.dir, strerror (errno));

			return RIPMIME_ERROR_CANT_CREATE_OUTPUT_DIR;
		}
	}

	// Unpack the contents
	RIPMIME_unpack(&glb);



	// Possible exit codes include;
	//		0 - all okay
	//		240 - processing stopped due to recursion limit

	if (glb.use_return_codes == 0) result = 0;

	return result;

}

/*-END-----------------------------------------------------------*/
