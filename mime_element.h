
#ifndef MIME_ELEMENT
#define MIME_ELEMENT
/* mime_element.h */

/* unpack modes */
#define RIPMIME_UNPACK_MODE_TO_DIRECTORY	0
#define RIPMIME_UNPACK_MODE_IN_MEMORY		1
#define RIPMIME_UNPACK_MODE_LIST_MIME		2

#define _MIME_RENAME_METHOD_INFIX			1
#define _MIME_RENAME_METHOD_PREFIX			2
#define _MIME_RENAME_METHOD_POSTFIX			3
#define _MIME_RENAME_METHOD_RANDINFIX		4
#define _MIME_RENAME_METHOD_RANDPREFIX		5
#define _MIME_RENAME_METHOD_RANDPOSTFIX		6

struct mime_output
{
	char *dir;
	int unpack_mode;
	// int fragment_number; will be used later
	int rename_method;
	int unique_names; // flague
};
typedef struct mime_output RIPMIME_output;

typedef struct {
	struct MIME_element* parent;
	int id;
	char* directory;
	char* filename;
	char* fullpath;
	FILE* f;
	char* content_type_string;
	char* content_transfer_encoding;
	char* name;
	char* mem_filearea;
	size_t mem_filearea_l;
	int decode_result_code;
} MIME_element;

typedef struct {
	size_t size;
	size_t capacity;
	MIME_element** array;
} dynamic_array;

typedef struct {
	int mime_count;
	dynamic_array* mime_arr;
} all_MIME_elements_s;

extern all_MIME_elements_s all_MIME_elements;

int MIMEELEMENT_set_debug( int level );

void all_MIME_elements_init (void);
MIME_element* MIME_element_add (
	struct MIME_element* parent,
	RIPMIME_output *unpack_metadata,
	char* filename,
	char* content_type_string,
	char* content_transfer_encoding,
	char* name,
	int current_recursion_level,
	int attachment_count,
	int filecount,
	const char* func);
// void MIME_element_free (MIME_element* cur);
void MIME_element_deactivate (MIME_element* cur, RIPMIME_output *unpack_metadata);
void printArray(dynamic_array* container);
void freeArray(dynamic_array* container, RIPMIME_output *unpack_metadata);
void write_all_to_FS_files(RIPMIME_output *unpack_metadata);

int MIME_test_uniquename( RIPMIME_output *unpack_metadata, char *fname );

#endif
