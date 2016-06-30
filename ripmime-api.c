/*----------------------------------------
 * ripmime-api
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

#include "logger.h"
#include "ffget.h"
#include "strstack.h"
#include "mime.h"
#include "MIME_headers.h"
#include "ripmime-api.h"

char defaultdir[] = ".";
char version[] = "v1.4.0.1 - 30/08/2004 (C) PLDaniels http://www.pldaniels.com/ripmime";

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
int RIPMIME_init (struct RIPMIME_object *rm)
{
	rm->outputdir = defaultdir;
	rm->mailpack = NULL;

	LOGGER_set_output_mode(_LOGGER_STDOUT);
	MIME_init();
	MIME_set_uniquenames(1);
	MIME_set_paranoid(0);
	MIME_set_renamemethod(_MIME_RENAME_METHOD_INFIX);
	MIME_set_verbosity(0);


	return 0;
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
int RIPMIME_decode( struct RIPMIME_object *rm, char *mailpack, char *outputdir )
{
	int result = 0;

	if (!mailpack)
	{
		LOGGER_log("%s:%d:RIPMIME_decode: mailpack filename is NULL\n",FL);
		return 1;
	} else {
		rm->mailpack = strdup(mailpack);
	}


	if (!outputdir)
	{
		LOGGER_log("%s:%d:RIPMIME_decode: output directory is NULL\n",FL);
		return 1;
	} else {
		rm->outputdir = strdup(outputdir);
	}

	// Fire up the randomizer

	srand (time (NULL));

	// clean up the output directory name if required (remove any trailing /'s, as suggested by James Cownie 03/02/2001

	if (rm->outputdir[strlen (rm->outputdir) - 1] == '/')
	{
		rm->outputdir[strlen (rm->outputdir) - 1] = '\0';
	}

	// Create the output directory required as specified by the -d parameter

	if (rm->outputdir != defaultdir)
	{
		result = mkdir (rm->outputdir, S_IRWXU);

		// if we had a problem creating a directory, and it wasn't just
		// due to the directory already existing, then we have a bit of
		// a problem on our hands, hence, report it.
		//

		if ((result == -1) && (errno != EEXIST))
		{
			fprintf (stderr, "ripMIME: Cannot create directory '%s' (%s)\n",
					rm->outputdir, strerror (errno));
			return -1;
		}
	}

	// Unpack the contents

	MIMEH_set_outputdir(rm->outputdir);
	MIME_unpack (rm->outputdir, rm->mailpack, 0);

	// do any last minute things

	MIME_close ();

	return 0;

}

/*-END-----------------------------------------------------------*/
