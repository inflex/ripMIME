
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "mime_element.h"
#include "logger.h"
#include "pldstr.h"

#ifndef FL
#define FL __FILE__, __LINE__
#endif

#define _FS_PATH_MAX 1023
#define _FS_FILE_MAX 254

/* Dynamic array support*/
#define INITIAL_SIZE 8

// function prototypes
//  array container functions
void arrayInit(dynamic_array** arr_ptr);

// Basic Operation functions
void insertItem(dynamic_array* container, MIME_element* item);
void updateItem(dynamic_array* container, int i, MIME_element* item);
MIME_element* getItem(dynamic_array* container, int i);
void deleteItem(dynamic_array* container, int i);

struct MIME_globals {
	int debug;
};

static struct MIME_globals glb;

#define MIME_DNORMAL   (glb.debug)

int MIMEELEMENT_set_debug( int level )
{
    glb.debug = level;
    return glb.debug;
}


void all_MIME_elements_init (void)
{
	all_MIME_elements.mime_count = 0;
	arrayInit(&(all_MIME_elements.mime_arr));
}

all_MIME_elements_s all_MIME_elements;

static char * dup_ini(char* s)
{
	return (s != NULL) ? strdup(s) : "\0";
}

MIME_element* MIME_element_add(struct MIME_element* parent, RIPMIME_output *unpack_metadata,
							   char* filename, char* content_type_string, char* content_transfer_encoding, char* name,
							   int current_recursion_level, int attachment_count, int filecount, const char* func)
{
	MIME_element *cur = malloc(sizeof(MIME_element));
	int fullpath_len = 0;

	if (MIME_DNORMAL) LOGGER_log("%s:%d:%s:start\n",FL,__func__);

	fullpath_len = strlen(unpack_metadata->dir) + strlen(filename) + 3 * sizeof(char);
	insertItem(all_MIME_elements.mime_arr, cur);
	cur->parent = parent;
	cur->decode_result_code = -1;
	cur->id = all_MIME_elements.mime_count++;
	cur->directory = unpack_metadata->dir;
	cur->filename = dup_ini(filename);
	cur->content_type_string = dup_ini(content_type_string);
	cur->content_transfer_encoding = dup_ini(content_transfer_encoding);
	cur->name = dup_ini(name);

	cur->fullpath = (char*)malloc(fullpath_len);
	snprintf(cur->fullpath,fullpath_len,"%s/%s",unpack_metadata->dir,filename);
	if (unpack_metadata->unpack_mode == RIPMIME_UNPACK_MODE_TO_DIRECTORY)
		cur->f = fopen(cur->fullpath,"wb");
	else
		cur->f = open_memstream (&cur->mem_filearea, &cur->mem_filearea_l);

	if (cur->f == NULL) {
		LOGGER_log("%s:%d:%s:ERROR: cannot open %s for writing",FL,func,cur->fullpath);
		return cur;
	}

	if (MIME_DNORMAL) LOGGER_log("%s:%d:%s:DEBUG: Decoding [encoding=%d] to %s\n",FL,__func__, content_transfer_encoding, cur->fullpath);

	if (unpack_metadata != NULL && unpack_metadata->unpack_mode == RIPMIME_UNPACK_MODE_LIST_MIME && cur->f != NULL) {
		fprintf (stdout, "%d|%d|%d|%d|%s|%s\n", all_MIME_elements.mime_count, attachment_count, filecount, current_recursion_level, cur->content_type_string, cur->filename);
	}
	return cur;
}

MIME_element* MIME_element_add_root(RIPMIME_output *unpack_metadata,
							   char* filename)
{
	MIME_element *cur = malloc(sizeof(MIME_element));
	int fullpath_len = 0;

	if (MIME_DNORMAL) LOGGER_log("%s:%d:%s:start\n",FL,__func__);

	fullpath_len = strlen(unpack_metadata->dir) + strlen(filename) + 3 * sizeof(char);
	insertItem(all_MIME_elements.mime_arr, cur);
	cur->parent = NULL;
	cur->decode_result_code = 0;
	cur->id = 0;
	cur->directory = unpack_metadata->dir;
	cur->filename = dup_ini(filename);
	cur->content_type_string = NULL;
	cur->content_transfer_encoding = NULL;
	cur->name = NULL;

	cur->fullpath = (char*)malloc(fullpath_len);
	snprintf(cur->fullpath,fullpath_len,"%s/%s",unpack_metadata->dir,filename);
	cur->f = fopen(cur->fullpath,"r");
	if (cur->f == NULL) {
		LOGGER_log("%s:%d:%s:ERROR: cannot open %s for reading",FL,"main",cur->fullpath);
		return cur;
	}
	return cur;
}

static void dup_free(char *s)
{
	if ((s != NULL) && s[0])
		free(s);
}

void MIME_element_free (MIME_element* cur)
{
	if (MIME_DNORMAL) LOGGER_log("%s:%d:%s:start\n",FL,__func__);

	if (cur == NULL) {
		if (MIME_DNORMAL) LOGGER_log("%s:%d:%s:NULL, nothing to free\n",FL,__func__);
		return;
	}

	if (cur->f != NULL) {
		fclose(cur->f);
	}
	dup_free(cur->fullpath);
	dup_free(cur->filename);
	dup_free(cur->content_type_string);
	dup_free(cur->content_transfer_encoding);
	dup_free(cur->name);

	free(cur);
	cur = NULL;
}

void MIME_element_deactivate(MIME_element* cur, RIPMIME_output *unpack_metadata)
{
	if (unpack_metadata->unpack_mode == RIPMIME_UNPACK_MODE_TO_DIRECTORY)
		MIME_element_free(cur);
}

static inline int get_random_value(void) {
	int randval;
	FILE *fp;
	size_t res = 0;

	fp = fopen("/dev/urandom", "r");
	res = fread(&randval, sizeof(randval), 1, fp);
	if (res == 0) {
		if (MIME_DNORMAL) LOGGER_log("%s:%d:%s: /dev/urandom Read error\n",FL,__func__);
			exit(1);
	}
	fclose(fp);
	if (randval < 0)
	{ randval = randval *( -1); };
	return randval;
}

/*------------------------------------------------------------------------
Procedure:     MIME_test_uniquename ID:1
Purpose:       Checks to see that the filename specified is unique. If it's not
unique, it will modify the filename
char *fname:   filename
Output:
Errors:
------------------------------------------------------------------------*/
int MIME_test_uniquename( RIPMIME_output *unpack_metadata, char *fname )
{
	struct stat buf;

	char newname[ _FS_PATH_MAX + 1];
	char scr[ _FS_PATH_MAX + 1]; /** Scratch var **/
	char *frontname, *extention;
	int cleared = 0;
	int count = 1;

	if (MIME_DNORMAL) LOGGER_log("%s:%d:%s:DEBUG: Start (%s)",FL,__func__,fname);

	frontname = extention = NULL;  // shuts the compiler up

	if (unpack_metadata->rename_method == _MIME_RENAME_METHOD_INFIX)
	{
		PLD_strncpy(scr,fname, _FS_PATH_MAX);
		frontname = scr;
		extention = strrchr(scr,'.');

		if (extention)
		{
			*extention = '\0';
			extention++;
		}
		else
		{
			unpack_metadata->rename_method = _MIME_RENAME_METHOD_POSTFIX;
		}
	}

	if (unpack_metadata->rename_method == _MIME_RENAME_METHOD_RANDINFIX)
	{
		PLD_strncpy(scr,fname, _FS_PATH_MAX);
		frontname = scr;
		extention = strrchr(scr,'.');

		if (extention)
		{
			*extention = '\0';
			extention++;
		}
		else
		{
			unpack_metadata->rename_method = _MIME_RENAME_METHOD_RANDPOSTFIX;
		}
	}

	snprintf(newname, _FS_PATH_MAX,"%s/%s",unpack_metadata->dir,fname);
	while (!cleared)
	{
		if ((stat(newname, &buf) == -1))
		{
			cleared++;
		}
		else
		{
			int randval = get_random_value();

			switch (unpack_metadata->rename_method) {
				case _MIME_RENAME_METHOD_PREFIX:
					snprintf(newname, _FS_PATH_MAX,"%s/%d_%s",unpack_metadata->dir,count,fname);
					break;
				case _MIME_RENAME_METHOD_INFIX:
					snprintf(newname, _FS_PATH_MAX,"%s/%s_%d.%s",unpack_metadata->dir,frontname,count,extention);
					break;
				case _MIME_RENAME_METHOD_POSTFIX:
					snprintf(newname, _FS_PATH_MAX,"%s/%s_%d",unpack_metadata->dir,fname,count);
					break;
				case _MIME_RENAME_METHOD_RANDPREFIX:
					snprintf(newname, _FS_PATH_MAX,"%s/%d_%d_%s",unpack_metadata->dir,count,randval,fname);
					break;
				case _MIME_RENAME_METHOD_RANDINFIX:
					snprintf(newname, _FS_PATH_MAX,"%s/%s_%d_%d.%s",unpack_metadata->dir,frontname,count,randval,extention);
					break;
				case _MIME_RENAME_METHOD_RANDPOSTFIX:
					snprintf(newname, _FS_PATH_MAX,"%s/%s_%d_%d",unpack_metadata->dir,fname,count,randval);
			}
			count++;
		}
	}
	if (count > 1)
	{
		frontname = strrchr(newname,'/');
		if (frontname) frontname++;
		else frontname = newname;

		PLD_strncpy(fname, frontname, _FS_PATH_MAX); //FIXME - this assumes that the buffer space is at least MIME_STRLEN_MAX sized.
	}
	if (MIME_DNORMAL) LOGGER_log("%s:%d:%s:DEBUG: Done (%s)",FL,__func__,fname);
	return 0;
}

static void copyFILEcontent(FILE* src, FILE* dst)
{
	int c; /* Not CHAR! for EOF */

	fseek(src, 0, SEEK_SET);
	// Read contents from file
	while (EOF != (c = fgetc(src)))
	{
		fputc(c, dst);
	}
}

void write_FS_file(RIPMIME_output *unpack_metadata, MIME_element* cur)
{
	char * wr_filename = NULL;
	FILE* wf = NULL;
	int fn_l = 0;

	MIME_test_uniquename(unpack_metadata, cur->filename);
	fn_l = strlen(unpack_metadata->dir) + strlen(cur->filename) + sizeof(char) * 2;
	wr_filename = malloc(fn_l);
	snprintf(wr_filename,fn_l,"%s/%s",unpack_metadata->dir,cur->filename);
	if (cur->f == NULL) {
		LOGGER_log("%s:%d:%s:ERROR: Cannot copy data from mime_element memory file, programming error (for %s)", FL,__func__, wr_filename, strerror(errno));
		free(wr_filename);
		return;
	}
	// Prepend the unpackdir path to the headers file name
	wf = fopen(wr_filename,"w");
	if (wf == NULL)
	{
		LOGGER_log("%s:%d:%s:ERROR: Cannot open '%s' for writing  (%s)", FL,__func__, wr_filename, strerror(errno));
		free(wr_filename);
		return;
	}
	copyFILEcontent(cur->f, wf);
	if (MIME_DNORMAL) LOGGER_log("%s:%d:%s:DEBUG: Memory FILE have cpoied to %s",FL,__func__,wr_filename);

	free(wr_filename);
	fclose(wf);
}

void write_all_to_FS_files(RIPMIME_output *unpack_metadata)
{
	int i;
	for (i = 0; i < all_MIME_elements.mime_count; i++)
	{
		MIME_element*  m = getItem(all_MIME_elements.mime_arr, i);
		write_FS_file(unpack_metadata, m);
	}
}

//------Dynamic array function definitions------
// Array initialization
void arrayInit(dynamic_array** arr_ptr)
{
	dynamic_array *container;
	container = (dynamic_array*)malloc(sizeof(dynamic_array));
	if(!container) {
		LOGGER_log("%s:%d:%s:ERROR: Memory allocation failed", FL,__func__);
		exit(0);
	}

	container->size = 0;
	container->capacity = INITIAL_SIZE;
	container->array = (MIME_element **)malloc(INITIAL_SIZE * sizeof(MIME_element*));
	if (!container->array){
		LOGGER_log("%s:%d:%s:ERROR: Memory allocation failed", FL,__func__);
		exit(0);
	}

	*arr_ptr = container;
}

//  Insertion Operation
void insertItem(dynamic_array* container, MIME_element* item)
{
	if (container->size == container->capacity) {
		MIME_element **temp = container->array;
		container->capacity <<= 1;
		container->array = realloc(container->array, container->capacity * sizeof(MIME_element*));
		if(!container->array) {
			LOGGER_log("%s:%d:%s:ERROR: Out of memory reallocation", FL,__func__);
			container->array = temp;
			return;
		}
	}
	container->array[container->size++] = item;
}

// Retrieve Item at Particular Index
MIME_element* getItem(dynamic_array* container, int index)
{
	if(index >= container->size) {
		LOGGER_log("%s:%d:%s:ERROR: Index %d is out of bounds", FL,__func__, index);
		return NULL;
	}
	return container->array[index];
}

// Update Operation
void updateItem(dynamic_array* container, int index, MIME_element* item)
{
	if (index >= container->size) {
		LOGGER_log("%s:%d:%s:ERROR: Index %d is out of bounds", FL,__func__, index);
		return;
	}
	container->array[index] = item;
}

// Delete Item from Particular Index
void deleteItem(dynamic_array* container, int index)
{
	if(index >= container->size) {
		LOGGER_log("%s:%d:%s:ERROR: Index %d is out of bounds", FL,__func__, index);
		return;
	}

	for (int i = index; i < container->size; i++) {
		container->array[i] = container->array[i + 1];
	}
	container->size--;
}

// Array Traversal
void printArray(dynamic_array* container)
{
	printf("Array elements: ");
	for (int i = 0; i < container->size; i++) {
		printf("%p ", container->array[i]);
	}
	printf("\nSize: ");
	printf("%lu", container->size);
	printf("\nCapacity: ");
	printf("%lu\n", container->capacity);
}

// Freeing the memory allocated to the array
void freeArray(dynamic_array* container, RIPMIME_output *unpack_metadata)
{
	if (unpack_metadata->unpack_mode != RIPMIME_UNPACK_MODE_TO_DIRECTORY)
		for (int i = 0; i < container->size; i++) {
			MIME_element_free(container->array[i]);
		}
	free(container->array);
	free(container);
}
/*---------- EOF -----------*/
