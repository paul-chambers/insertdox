/**
	@file stringutils.c

	Several small string processing functions.

	@version 0.9
	@author Paul Chambers
	@date 2005-2006
*/
/* $Header$ */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "stringutils.h"

/**
	Microsoft likes their slashes one way, everyone else the other...
*/
#ifdef	WIN32
#define qOSPathSeparator	'\\'
#else
#define qOSPathSeparator	'/'
#endif

/**
	Returns a pointer to the filename element of a path.

	@param[in] 	path 	the full path

	@return pointer to the filename part of the path
*/
char *filenameFromPath(char *path)
{
	char *p = path;
	
	if (p != NULL)
	{
		p = strrchr(p, qOSPathSeparator);
		if (p != NULL) ++p;
		else p = path;
	}

	return p;
}

/**
	concatenate two strings into a new string.

	@param[in] 	prefix 	the first string
	@param[in] 	suffix 	the last string

	@return a pointer to the concatenated copy
	@retval NULL unable to allocate memory required for copy
*/
char *cpycat(const char *prefix, const char *suffix)
{
	size_t	len;
	char	*copy;
	
	len = strlen(prefix);
	copy = (char *)malloc(len + strlen(suffix) + 1);
	if (copy != NULL)
	{
		strcpy(copy, prefix);
		strcpy(&copy[len], suffix);
	}

	return copy;
}

/**
	utility function to advance a pointer
	past a run of whitespace.

	@param[in] 	ptr 	the starting point
	@param[in] 	end 	result must not point past this

	@return points to the first character not skipped, or returns 'end'
*/
char *skipSpace(char *ptr, char *end)
{
	char *p = ptr;

	while (p < end && isspace(*p))
		++p;
	return p;
}

/**
	utility function to move a pointer backwards
	past a run of whitespace.

	@note	the last character of the string
			is the one prior to the pointer.

	@param[in] 	ptr 	the starting point (just after)
	@param[in] 	start 	result must not point before this

	@return points just past the last skipped character, or returns 'start'
*/
char *trimSpace(char *ptr, char *start)
{
	char *p = ptr - 1;

	while ( p > start && isspace(*p))
		--p;
	return p + 1;
}

/**
	utility function to advance a pointer
	past a run of '*','/' and/or whitespace.

	@param[in] 	ptr 	the starting point
	@param[in] 	end 	result must not point past this

	@return points to the first character not skipped, or returns 'end'
*/
char *skipComment(char *ptr, char *end)
{
	char *p = ptr;

	while (p < end && (isspace(*p) || *p == '/' || *p == '*'))
		++p;
	return p;
}

/**
	utility function to move a pointer backwards
	past a run of '*','/' and/or whitespace.

	@note	the last character of the string
			is the one prior to the pointer.

	@param[in] 	ptr 	the starting point (just after)
	@param[in] 	start 	result must not point before this

	@return points just past the last skipped character, or returns 'start'
*/
char *trimComment(char *ptr, char *start)
{
	char *p = ptr - 1;

	while ( p > start && (isspace(*p) || *p == '/' || *p == '*'))
		--p;
	return p + 1;
}

/**
	utility function to advance a pointer past a run
	of whitespace and/or punctuation.

	@param[in] 	ptr 	the starting point
	@param[in] 	end 	result must not point past this

	@return points to the first character not skipped, or returns 'end'
*/
char *skipPunct(char *ptr, char *end)
{
	char *p = ptr;

	while (p < end && (isspace(*p) || ispunct(*p)) )
		++p;
	return p;
}


/**
	Frees all memory allocated to a tStringList.
	@note not thread safe

	@param[in,out] 	sl 	the first element of the tStringList to free
*/
void freeStringList(tStringList **sl)
{
	tStringList	*element;

	element = *sl;

	while (element != NULL)
	{
		/* remove it from the list (not thread safe) */
		*sl = element->next;

		/* free the string (if any) */
		if (element->string != NULL)
			free(element->string);

		/* free the structure itself */
		free(element);

		/* prepare for the next iteration */
		element = *sl;
	}
}

/**
	adds a new string to a tStringList.

	@param[in,out] 	sl 		the tStringList which will 
							recieve the new element
	@param[in] 		start 	points at the first character
							of the string to add
	@param[in]	 	end 	points just after the last
							character of the string to add
*/
void addString(tStringList **sl, char *start, char * end)
{
	tStringList *element;
	size_t len;
	
	element = (tStringList *)malloc(sizeof(tStringList));
	if (element != NULL)
	{
		len = (end - start);
		element->string = (char *)malloc(len + 1);
		if (element->string != NULL) 
		{
			memcpy(element->string, start, len);
			element->string[len] = '\0';
			
			/* note: not thread-safe! */
			element->next = *sl;
			*sl = element;
		}
	}
}

/**
	Writes out all the elements of a tStringList.

	Outputs one element per line, prefixed with the string provided.

	@param[in] 	file 	the output file
	@param[in] 	sl 		the tStringList to dump
	@param[in] 	prefix 	prefix each item with this string
*/
void dumpStringList(FILE *file, tStringList *sl, char *prefix)
{
	tStringList *element;
	
	element = sl;
	while (element != NULL)
	{
		if (element->string == NULL)
			fprintf(file, "\n%s<null>", prefix);
		else
			fprintf(file, "\n%s%s", prefix, element->string);
		element = element->next;
	}
}
