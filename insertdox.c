/**
	@file insertdox.c
	@brief Contains the main entry point, and command line parsing

	@version 0.91
	@author Paul Chambers
	@date 2005-2006
*/
/* $Header$ */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "stringutils.h"
#include "bufferutils.h"
#include "parser.h"


/** global options parsed from command line */
tAppOptions gOptions;


/*
	prototypes
*/
static void printVersion(const char *appName);
static void printUsage(const char *appName);

/**
	@internal

	Prints version information to stderr.	

	@param[in] 	appName 	the application's name (argv[0])
*/
static void printVersion(const char *appName)
{
	fprintf(stderr, "%s, version %s\n", appName, qVersion);
	fprintf(stderr, "  Copyright (c) Paul Chambers, 2005-06\n");
}

/**
@page Usage
@verbatim
 insertdox [-v|-h] [-p] [-b <filename>] <file list>
     -v, --version    print version message
     -h, --help       print usage message
     -p               only emit function comments and prototypes
     -b <filename>    provide a 'boilerplate' file for the file comment
 if <file list> is empty, process stdin to stdout. @endverbatim
*/
/**
	@internal

	Prints version and usage information to stderr.	

	@param[in] 	appName 	the application's name (argv[0])
*/
static void printUsage(const char *appName)
{
	printVersion(appName);
	fprintf(stderr,
		"Usage: %s [-v|-h] [-p] [-b <filename>] <file list>\n"
		"    -v, --version    print version message\n"
		"    -h, --help       print usage message\n"
		"    -p               only emit function comments and prototypes\n"
		"    -b <filename>    provide a 'boilerplate' file for the file comment\n"
		"if <file list> is empty, process stdin to stdout.\n"
	, appName );
}

/**
	The main entry point.
	Processes any command line arguments provided. Starts by scanning
	for options, then treats any remaining arguments as files to
	process. If there are no files, processes stdin to stdout.

	@param[in] 	argc 	count of arguments on command line
	@param[in] 	argv 	arguments from the command line

	@return int
	@retval 0	everything went smoothly
	@retval -1	couldn't read an input file
	@retval -2	couldn't write to an output file
	@retval -3	couldn't rename the input file
	@retval -4	couldn't rename the output file
*/

int main(int argc, char *argv[])
{
	int	result;
	int 	i, count;
	FILE	*inFile, *outFile;
	char	*tmpname, *bakname;
	bool	usageOnly;

	count = 1;
	gOptions.filename = NULL;
	gOptions.boilerplate = NULL;
	gOptions.onlyPrototypes = false;

	/* Run through the arguments, pulling out just the options.
	   While we're doing this, shuffle down any non-option args
	   (i.e. the filenames) so that they're contiguous */

	usageOnly = (argc > 1);

	for (i = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
			case 'V':
			case 'v':
				printVersion(argv[0]);
				break;

			case '?':
			case 'h':
				printUsage(argv[0]);
				break;
				
			case 'b':
				if (++i < argc)
				{
					gOptions.boilerplate = argv[i];
					usageOnly = false;
				}
				break;

			case 'p':
				gOptions.onlyPrototypes = true;
				usageOnly = false;
				break;

			case '-': /* the 'wordy' varients */
				if (strcmp(argv[i],"--version") == 0)
				{
					printVersion(argv[0]);
					break;					
				}
				else if (strcmp(argv[i],"--help") == 0)
				{
					printUsage(argv[0]);
					break;
				}
				/* fall through if there's no match */
			default:
				fprintf(stderr,
					"### error: unknown option \'%s\' given to %s\n",
					argv[i],argv[0]);
				break;			
			}
		}
		else
		{
			/* copy the pointer, if it's not at the same index */
			if (count != i)	argv[count] = argv[i];
			++count;
			usageOnly = false;
		}
	}
	
	/* argv[1] through argv[count] now contains only filenames */
	
	result = 0;

	if (!usageOnly)
	{
		if (count <= 1) /* one is really zero, since it started at 1 */
		{
			/* assume stdin to stdout */
			gOptions.filename = NULL;
			result = processFile(stdout, stdin);
		}
		else for (i = 1; i < count; ++i)
		{
			gOptions.filename = filenameFromPath(argv[i]);
			
			tmpname = NULL;

			inFile = fopen(argv[i],"r");
			if (inFile == NULL)
			{
				fprintf(stderr,
						"### error: unable to open '%s' for reading (in %s)\n",
						argv[i], argv[0]);
				result = -1;
			}
			else
			{
				tmpname = cpycat(argv[i],".tmp");
				if (tmpname != NULL)
				{
					outFile = fopen(tmpname,"w");
					if (outFile == NULL)
					{
						fprintf(stderr,
								"### error: unable to open '%s' for writing (in %s)\n",
								tmpname, argv[0]);
						result = -2;
					}
					else
					{
						/* this performs the actual processing */
						result = processFile(outFile, inFile);

						fclose(outFile);
					}
					/* don't free tmpname yet; need it below */
				}
				fclose(inFile);
			}
			/* only rename the files if everything went smoothly */
			if (tmpname != NULL)
			{
				if (result == 0)
				{
					bakname = cpycat(argv[i],".bak");
					if (bakname != NULL)
					{
						result = rename(argv[i], bakname);
						if (result != 0)
						{
							fprintf(stderr,
									"### error: unable to rename '%s' to '%s' (in %s)\n",
									argv[i], bakname, argv[0]);
							result = -3;
						}
						else
						{
							result = rename(tmpname, argv[i]);
							if (result != 0)
							{
								fprintf(stderr,
										"### error: unable to rename '%s' to '%s' (in %s)\n",
										tmpname, argv[i], argv[0]);
								result = -4;
							}
						}
						free(bakname);
					}
				}
				free(tmpname);
			}
		}
	}

	return result;
}
