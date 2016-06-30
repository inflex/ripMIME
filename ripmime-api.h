
struct RIPMIME_object
{
	char *mailpack;
	char *outputdir;
};

int RIPMIME_init( struct RIPMIME_object *rm );
int RIPMIME_decode( struct RIPMIME_object *rm, char *mailpack, char *outputdir );

