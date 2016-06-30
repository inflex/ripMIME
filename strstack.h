
#ifndef __STRSTACK__
#define __STRSTACK__

#define SS_STRLEN_MAX 1024


struct SS_node {
	char *data;
	size_t data_length;
	struct SS_node *next;
};

struct SS_object {
	int debug;
	int verbose;
	int count;
	int detect_limit;
	struct SS_node *stringstack;
	char datastacksafe[SS_STRLEN_MAX];
};


int SS_init( struct SS_object *ss );
int SS_set_verbose( struct SS_object *ss, int level );
int SS_set_debug( struct SS_object *ss, int level );

int SS_push( struct SS_object *ss, char *data, size_t data_length );
char *SS_pop( struct SS_object *ss );
char *SS_top( struct SS_object *ss );
char *SS_cmp( struct SS_object *ss, char *find_me, size_t find_me_len );

int SS_dump( struct SS_object *ss );
int SS_count( struct SS_object *ss );
int SS_done( struct SS_object *ss );

#endif
