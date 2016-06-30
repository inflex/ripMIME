#include <stdio.h>
#include <stdlib.h>


int RAWGET_get( unsigned char *buffer, int max, FILE *f )
{

	unsigned char c;					// read buffer
	int count = 0;						// How many bytes read

	// Special situation here, if we have a return from MIME_headers which indicates
	// that we have data in a MIMEH_pushback, then we need to process that first, before we
	// go back into the data file.

	//

	// Whilst we've got less bytes than the maximum availabe
	// for the buffer, we keep on reading
	//
	while (count < max)
	{
		// If we do infact read in 1 bytes...
		if (fread(&c,1,1,f)==1)
		{

			*buffer = c;		// Set the byte in the buffer
			buffer++;			// move the buffer pointer
			count++;				// Increment the byte cound

			if (c == '\n')		// If we encounter a \n (or \r)
			{
				break;			// Hop out of while loop
			}
		}
		else break;				// if we didn't read right, then jump out as well
	}

	return count;
}

