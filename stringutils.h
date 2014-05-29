/**
	@file stringutils.h

	Public interface for stringutils.c

	@version 0.9
	@author Paul Chambers
	@date 2005-2006
*/
/* $Header$ */


char *filenameFromPath(char *path);

char *cpycat(const char *orig, const char *suffix);

char *skipSpace(char *ptr, char *end);
char *trimSpace(char *ptr, char *start);
char *skipComment(char *ptr, char *end);
char *trimComment(char *ptr, char *start);
char *skipPunct(char *ptr, char *end);

/**
	one element of a linked list of strings.
*/
typedef struct tStringList
{
	struct tStringList	*next;	/**< pointer to the next element in the linked list */
	char	*string;			/**< pointer to the actual string */
} tStringList;

void freeStringList(tStringList **sl);
void addString(tStringList **sl,char *start, char * end);
void dumpStringList(FILE *file, tStringList *sl, char *prefix);
