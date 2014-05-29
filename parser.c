/**
	@file parser.c

	The parser does the actual text processing of the file.
	
	It accumulates the incoming stream into a buffer, running it through
	a state machine	as it goes, remembering the position of significant
	syntax boundaries.

	When it detects certain syntax boundaries, it emitsthe accumulated text,
	along with additional annotations it generates from the analyized text.

	Thus it is able to insert annotations well before the text that caused
	them to be generated, in a single-pass 'streaming' fashion.

	@version 0.91
	@author Paul Chambers
	@date 2005-2006
*/
/* $Header$ */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#define __USE_BSD
#include <string.h>
#include <ctype.h>

#include "stringutils.h"
#include "bufferutils.h"

#include "parser.h"

static void parseComment(tBuffer *buf);
static void parseStatement(tBuffer *buf);

static void processBoilerplate(tBuffer *buf);
static void newFileComment(tBuffer *buf);
static void processFileComment(tBuffer *buf);

static void processTyped(	char *type, size_t size,
							bool *isStaticP, bool *inOnlyP,
							tRange *name );
static void processArgList(tBuffer *buf);
static void processDescription(tBuffer *buf);
static void processFunction(tBuffer *buf);

static void flushBuffer(tBuffer *buf);

/*
	private functions
*/

/*
	the 'parse' series of functions collect
	additional data during parsing.
*/

/**
	@internal

	@brief Mine the comments in a function's body.

	Look for 'todo', 'fixme' or 'note' comments embedded within
	comments and if found, record them for use later when generating
	the function's description.
	
	@param[in,out] 	buf 	the tBuffer to process
*/

static void parseComment(tBuffer *buf)
{
	char *p;
	bool match = false;

	if (buf->commentStart != NULL)
	{
		p = skipPunct(buf->commentStart,buf->ptr);
		
		/* look for todos */
		if (strncasecmp(p,"todo",4) == 0)
		{
			match = true;
			p += 4;
		}
		else if (strncasecmp(p,"fixme",5) == 0)
		{
			match = true;
			p += 5;
		}
		else if (strncasecmp(p,"fix-me",6) == 0)
		{
			match = true;
			p += 6;
		}

		if (match)
		{
			addString(&(buf->todos),
				skipPunct(p, buf->ptr),
				trimComment(buf->ptr, p));
		}
		else 
		{ /* look for notes */
			if (strncasecmp(p,"note",4) == 0)
			{
				match = true;
				p += 4;
			}
			else if (strncasecmp(p,"nb",2) == 0)
			{
				match = true;
				p += 2;
			}
			if (match)
			{
				addString(&(buf->notes),
					skipPunct(p, buf->ptr),
					trimComment(buf->ptr, p));
			}
		}

		
		buf->commentStart = NULL;
	}
}

/**
	@internal

	@brief Mine the statements inside a function's body.

	Look for return statements, and extract their return
	value so that they can be used to generate \@retval
	tags later.
	
	@param[in,out] 	buf 	the tBuffer to process
*/
static void parseStatement(tBuffer *buf)
{
	char *s,*e,*p;
	int count;

	s = buf->statementStart;
	if (s != NULL)
	{
		if (strncmp(s,"return",6) == 0)
		{
			s = skipSpace(s+6, buf->ptr);
			e = trimSpace(buf->ptr, s);

			/* if the value is surrounded by brackets, remove them */
			if (*s == '(')
			{
				/* check for situations like (a)+(b) */
				p = s;
				count = 0;
				while (p < e)
				{
					if (*p++ == '(') ++count;
				}

				if (count == 1 && e[-1] == ')')
				{
					s = skipSpace(&s[1], e);
					e = trimSpace(&e[-1], s);
				}
			}
			addString(&(buf->retvals),s,e);
		}

		buf->statementStart = NULL;
	}
}

/*
	the 'process' series of functions emit
	test based on data collected during parsing.
*/

/**
	@internal

	@brief	Injects the 'boilerplate' file into the output
			stream, if one has been provided.

	@param[in] 	buf	the tBuffer to process
*/

static void processBoilerplate(tBuffer *buf)
{
	FILE	*boilerplate;
	byte	*tmp;
	size_t	count;

	if (gOptions.boilerplate != NULL)
	{
		tmp = (byte *)malloc(32768);
		if (tmp == NULL)
		{
			fprintf(stderr,"### Error: unable to allocate memory");
		}
		else
		{
			boilerplate = fopen(gOptions.boilerplate,"r");
			if (boilerplate == NULL)
			{
				fprintf(stderr,
						"### error: unable to open '%s' to read\n",
						gOptions.boilerplate);
				exit(1);		
			}
			else
			{
				while ((count = fread(tmp,sizeof(byte),32768,boilerplate)) > 0)
					fwrite(tmp, sizeof(byte), count, buf->file);

				fclose(boilerplate);
			}
			free(tmp);			
		}		
	}
}

/**
	@internal

	@brief	Generates a new file comment.

	Used when no existing file comment is detected at the
	beginning of a file, this function injects a template
	that contains the basics, like a \@file tag, the
	boilerplate (if any), and a \@todo tag as a reminder
	to edit the comment.

	@param[in] 	buf 	the tBuffer to process
*/
static void newFileComment(tBuffer *buf)
{
	fprintf(buf->file, "/**\n\t@file %s",
		gOptions.filename != NULL ? gOptions.filename : "<unknown>" );

	fprintf(buf->file, "\n\n\tPut a description of the file here.\n");

	/* insert the boilerplate file, if there is one */
	processBoilerplate(buf);

	fprintf(buf->file,
		"\n\t@todo Edit file comment (automatically generated by insertdox)");
	fprintf(buf->file, "\n*/\n/* $Header$ */\n\n");
}

/**
	@internal

	@brief Process an existing file comment.
	
	If the file starts with a comment, this routine is
	called to process and output it. Mostly it just strips
	any extraneous punctuation (like rows of astrisks),
	injects the boilerplate, and wraps it with the right
	markers to make it a Doxygen comment block.

	@param[in,out] 	buf 	the tBuffer to process
*/
static void processFileComment(tBuffer *buf)
{
	char *s, *e;

	/* trim off any punctuation and whitespace */
	s = buf->data;
	e = buf->ptr;
	s = skipComment(s,e);
	e = trimComment(e,s);

	/* emit the original comment */
	fprintf(buf->file, "/**\n\t");
	dumpBlock(buf, s, e);
	fprintf(buf->file, "\n");

	/* emit boilerplate, if any */
	processBoilerplate(buf);

	fprintf(buf->file, "\n*/\n");
}

/**
	@internal

	@brief Turns a type declaration into a name and descriptive type string.

	This function is used on both the function name and each of its
	arguments. It also makes an educated guess about whether the type
	is input only, or may be used to modify the argument within the
	function.

	@note isStaticP and inOnlyP will only be set if type is non-NULL

	@param[out] 	type 		a buffer to fill with the type description
	@param[in]	 	size	 	space available in the type buffer
	@param[out] 	isStaticP 	set according to whether the type is static
	@param[out] 	inOnlyP 	set according to educated guess about an
								argument's direction (true if pass by value
								or const, false otherwise).
	@param[in,out] 	name 		On input, the range containing the type
								declaration to process.	On output, shrunk
								to only contain the identifier.

	@see processArgList()
	@see processFunction()

	@todo doesn't handle C++&ndash;style references
*/
static void processTyped(	char *type, size_t size,
							bool *isStaticP, bool *inOnlyP,
							tRange *name )
{
	char *p;
	char *s = skipSpace(name->start,name->end);
	char *e	= trimSpace(name->end, name->start);
	int  ptrCount = 0;
	size_t  remaining = size;
	bool loop = true;
	bool isArray = false;
	bool isStatic = false;
	bool isConst = false;
	bool inOnly;

	/* if [] is present, flag it and remove */
	--e;
	if (*e == ']')
	{
		isArray = true;
		do {
			--e;
		} while (e >= s && *e != '[');
		if (e > s) --e;
	}
	p = e;
	++e;

	/* isolate the actual parameter name */
	while (p >= s && (isalnum(*p) || *p == '_'))
	{
		--p;
	}
	p += 1;

	name->start = p;
	name->end = e;

	/* only do the following if asked */
	if (type != NULL && size > 0)
	{
		*type = '\0';

		while (loop)
		{
			/* strip off 'static' if it's present */
			if (strncmp(s,"static",6) == 0)
			{
				isStatic = true;
				s = skipSpace(s+6,e);
			}
			else if (strncmp(s,"const",5) == 0)			
			{
				isConst = true;
				s = skipSpace(s+5,e);
			}
			else loop = false;
		}
		if (isStaticP != NULL) *isStaticP = isStatic;

		e = trimSpace(p,s);
		--e; /* because it normally points after the actual character */
		while (e > s && *e == '*')
		{
			++ptrCount;
			--e;
			while (e > s && isspace(*e))
			{
				--e;
			}
		}
		++e; /* put it back to normal */

		 /* guess if the parameter cannot be modified by the function
			(basically if it's const, or pass by value).
			Very easily fooled by typdefs and #defines */
		inOnly = (bool)(isConst || (ptrCount == 0 && !isArray));
		if (inOnlyP != NULL) *inOnlyP = inOnly;

		while (ptrCount--)
		{
			strncat(type, "a pointer to ", remaining);
			remaining -= 13; /* length of "pointer to " */
		}

		if (isArray)
		{
			strncat(type, "an array of ", remaining);
			remaining -= 12; /* length of "an array of " */
		}
#ifdef qTypePrefixedWithA
		else
		{
			if (!isConst && strchr("aeiouAEIOU",*s) != NULL)
			{
				strncat(type, "an ", remaining);
				remaining -= 3;
			}
			else
			{
				strncat(type, "a ", remaining);
				remaining -= 2;			
			}
		}
#endif

		if (isConst)
		{
			strncat(type, "const ", remaining);
			remaining -= 6; /* length of "const " */
		}

		/* output the type */
		if ((size_t)(e - s) < remaining)
		{
			remaining = (e - s);
		}

		strncat(type, s, remaining);
	}
}

/**
	@internal

	@brief Process a function's argument list.
	
	Generates a list of \@param tags from the list
	of arguments to a function.

	@note Does not emit 'void'.

	@param[in,out] 	buf 	the tBuffer to process
	
	@see processTyped()
*/
static void processArgList(tBuffer *buf)
{
	tRange	name;
	char	type[200];
	char	*s = buf->arglist.start;
	char	*e = buf->arglist.end;
	char	*p;
	bool	inOnly;

	bool saidSomething = false;

	while (s < e && (isspace(*s) || *s == '('))
	{
		++s;
	}
	p = s;

	while (p < e)
	{
		switch (*p)
		{
		case ',': /* list seperator */
		case ')': /* list terminator */
			name.start = s;
			name.end = p;
			processTyped(type, sizeof(type), NULL, &inOnly, &name);
			
			if ((name.end - name.start) != 4
			 || strncmp(name.start, "void", 4) != 0)
			{
				/* not 'void', so output it */
				fprintf(buf->file,
					"\n\t@param[in%s] \t", inOnly? "" : ",out");
				dumpBlock(buf, name.start, name.end);
				fprintf(buf->file," \t%s",type);
				
				saidSomething = true;
			}
			s = p + 1;
			break;
		}
		++p;
	}
	if (saidSomething)
		fprintf(buf->file, "\n");	
}

/**
	@internal

	@brief Processes the function's original comment (if any)
	
	This function just trims the original comment, or
	generates a placeholder if there wasn't one.
	
	@note	If you have an pre-existing comment formatting
			convention and want	to automatically convert it,
			this is the place to do it.

	@param[in,out] 	buf 	the tBuffer to process
*/
static void processDescription(tBuffer *buf)
{
	char *s, *e;

	if (buf->description.count > 0)
	{
		/* trim off any punctuation and whitespace */
		s = buf->description.start;
		e = buf->description.end;
		s = skipComment(s,e);
		e = trimComment(e,s);

		if ( s == e)
		{
			 /* there's nothing left after trimming, thus
				it's an empty comment! so we change our
				mind, and generate a placeholder after all */
			buf->description.count = 0;
		}
		else
		{
			/*>>>
				detect and convert any pre-existing
				comment	formatting convention here
			<<<*/
				
			/* emit the original comment */
			dumpBlock(buf, s, e);
		}
	}
	if (buf->description.count == 0)
	{
		/* inject a placeholder */
		fprintf(buf->file, "Brief description needed.");
		fprintf(buf->file, "\n\n\tFollowed by a more complete description.");
	}
	fprintf(buf->file, "\n");
}

/**
	@internal

	@brief Process a function.

	If the required parts are detected (see flushParser()),
	this function is called to process the collected
	information	and output the function in the tBuffer.

	@param[in,out] 	buf 	the tBuffer to process
*/
static void processFunction(tBuffer *buf)
{
	tRange name;
	char type[200];
	bool isStatic;

	/* fixup the description range, if there isn't one.
		makes subsequent code simpler. */
	if (buf->description.count == 0)
	{
		buf->description.start = buf->function.start - 1;
		buf->description.end = buf->description.start;
	}
	/* emit up to the beginning of the description */
	dumpBlock(buf, buf->data, buf->description.start);

	name.start = buf->function.start;
	name.end = buf->function.end;
	processTyped(type, sizeof(type), &isStatic, NULL, &name);

	fprintf(buf->file, "\n/**\n%s\t", isStatic ? "\t@internal\n\n" : "");

	processDescription(buf);

	processArgList(buf);

	if (strcmp(type,"void") != 0)
	{
		fprintf(buf->file, "\n\t@return %s", type);
		dumpStringList(buf->file, buf->retvals, "\t@retval ");
		fprintf(buf->file, "\n");
	}

	dumpStringList(buf->file, buf->todos,"\t@todo ");
	fprintf(buf->file, "\n\t@todo edit me (automatically generated by insertdox)\n*/");
	
	if (gOptions.onlyPrototypes)
	{
		dumpBlock(buf, buf->description.end, buf->arglist.end);
		fprintf(buf->file, ";\n\n");
	}
	else
	{
		dumpBlock(buf, buf->description.end, buf->ptr);
	}
}

/*
	public functions
*/

/**
	@internal

	@brief Flush a tBuffer, and check for special processing.
	
	This function determines if the tBuffer contains something
	that requires special processing, calls it if it does, or
	just outputs it unmodified if not.

	@param[in,out] 	buf 	the tBuffer to process
*/
static void flushBuffer(tBuffer *buf)
{
	if (buf->ptr > buf->data)
	{
		if (buf->fileComment) 
		{
			processFileComment(buf);
		}
		else if (buf->function.count == 1
			 && buf->arglist.count == 1
			 && buf->body.count == 1)
		{
			processFunction(buf);
		}
		else if (!gOptions.onlyPrototypes)
		{
			dumpBlock(buf, buf->data, buf->ptr);
		}
	}
	clearBuffer(buf);
}

/**
	@brief Controls the meat of the processing.

	Basically a state machine, which accumulates the incoming
	stream of characters and chops them up into meaningful
	chunks, then passes	those chunks off to other routines
	to further process and output.

	@param[in] 	inFile		the file to process
	@param[out] outFile 	where to write the result

	@return int
	@retval 0		everything went smoothly
	@retval -108	unable to allocate memory
*/
int processFile(FILE *outFile, FILE *inFile)
{
	tBuffer *buf;
	int		prevc, c, nextc;	/* needs to be int to hold EOF */
	int		depthCurly,depthRound;
	bool	inComment, inCppComment;
	bool	inPreprocessor;
	bool	inSingleQuotes, inDoubleQuotes;
	bool	inBetween, isLiteral, isChar1;
	bool	atStart, doFlush;

	buf = (tBuffer *)malloc(sizeof(tBuffer));
	if (buf == NULL)
	{
		fprintf(stderr,
				"### error: unable to allocate a buffer\n");
		return (-108);
	}

	initBuffer(buf,outFile);

	depthCurly = 0;
	depthRound = 0;
	isLiteral = false;
	inComment = false;
	inCppComment = false;
	inPreprocessor = false;
	inSingleQuotes = false;
	inDoubleQuotes = false;
	atStart = true;
	inBetween = true;
	isChar1 = true;
	doFlush = false;

	prevc = '\0';

	c = fgetc(inFile);

	while (c != EOF)
	{
		nextc = fgetc(inFile);

		/*
			the main state machine
		*/

		if (isLiteral)
		{
			 /* normally the 'literal' is a single character.
				but in the odd case of DOS cr/lf pairs, they
				should be treated as one logical character */
			if ((c != '\r' || nextc != '\n')
			 && (c != '\n' || nextc != '\r'))
			{
				isLiteral = false;
			}
		}
		else if (inComment)
		{
			if (inCppComment)
			{
				if (c == '\n' || c == '\r')
				{
					inCppComment = false;
					inComment = false;
					inPreprocessor = false;
					isChar1 = true;
					if (depthCurly == 0)
					{
						buf->description.end = buf->ptr;
						++buf->description.count;
						if (buf->fileComment)
						{
							flushBuffer(buf);
							buf->fileComment = false;
						}
					}
					else
					{
						parseComment(buf);
					}
				}
			}
			else
			{
				if (prevc == '*' && c == '/')
				{
					inComment = false;
					if (depthCurly == 0)
					{
						buf->description.end = buf->ptr + 1;
						++buf->description.count;
						if (buf->fileComment)
						{
							doFlush = true;
						}
					}
					else
					{
						parseComment(buf);
					}
				}
			}
		}
		else if (inPreprocessor)
		{
			switch (c)
			{
			case '\n':
			case '\r':
				if (inCppComment)
				{
					inCppComment = false;
					inComment = false;
					if (depthCurly == 0)
					{
						buf->description.end = buf->ptr;
						++buf->description.count;
					}
					else
					{
						parseComment(buf);
					}
				}
				if (depthCurly == 0 && !inComment)
				{
					doFlush = true;
				}

				inPreprocessor = false;
				isChar1 = true;
				break;

			case '/':
				switch (nextc)
				{
				case '/':
					inCppComment = true;
					/* fall through */
				case '*':
					inComment = true;
					break;
				}
				if (inComment)
				{
					if (depthCurly == 0)
					{
						flushBuffer(buf);
						buf->description.start = buf->ptr;
					}
					else
					{
						buf->commentStart = buf->ptr;
					}
				}
				break;
			}		
		}
		else if (inSingleQuotes)
		{
			switch (c)
			{
			case '\'':
				inSingleQuotes = false;
				break;
			case '\\':
				isLiteral = true;
				break;
			}
		}
		else if (inDoubleQuotes)
		{
			switch (c)
			{
			case '"':
				inDoubleQuotes = false;
				break;
			case '\\':
				isLiteral = true;
				break;
			}
		}
		else {
			switch (c)
			{
			case '\\':
				isLiteral = true;
				break;

			case '/':
				switch (nextc)
				{
				case '*':
					inComment = true;
					break;
				case '/':
					inCppComment = true;
					inComment = true;
					break;
				}
				if (inComment)
				{
					if (depthCurly == 0)
					{
						flushBuffer(buf);
						buf->description.start = buf->ptr;
					}
					else
					{
						buf->commentStart = buf->ptr;
					}
				}
				break;

			case '#':
				/* is this the first non-whitepace char on the line? */
				if (isChar1)
				{
					if (depthCurly == 0)
					{
						/*  if there was a comment preceeding this pre-
							processor directive, it wasn't a description */
						buf->description.count = 0;
						buf->description.start = NULL;
						buf->description.end = NULL;
					}

					inPreprocessor = true;
				}
				break;

			case '\'':
				inSingleQuotes = true;
				break;

			case '"':
				inDoubleQuotes = true;
				break;

			case '(':
				if (depthCurly == 0 && depthRound == 0)
				{
					buf->function.end = buf->ptr;
					++buf->function.count;
					buf->arglist.start = buf->ptr;
				}
				++depthRound;
				break;

			case ')':
				--depthRound;
				if (depthCurly == 0 && depthRound == 0)
				{
					buf->arglist.end = buf->ptr + 1;
					++buf->arglist.count;
				}
				break;

			case '{':
				if (depthCurly == 0)
				{
					buf->body.start = buf->ptr;
				}
				else
					parseStatement(buf);
				++depthCurly;
				inBetween = true;
				break;

			case '}':
				--depthCurly;
				if (depthCurly == 0)
				{
					buf->body.end = buf->ptr + 1;
					++buf->body.count;
					doFlush = true;
				}
				else
					parseStatement(buf);
				inBetween = true;
				break;

			case ';':
				if (depthCurly == 0)
					doFlush = true;
				else
					parseStatement(buf);
				inBetween = true;
				break;

			case '\n':
			case '\r':
				isChar1 = true;
				break;

			default:
				if (inBetween && !isspace(c))
				{
					if (depthCurly == 0)
						buf->function.start = buf->ptr;
					else
						buf->statementStart = buf->ptr;
					inBetween = false;
				}
				break;
			}
		}

		/* check the first non-whitespace characters to see if
			it's the start of a comment. If so, assume it's the
			file's header comment and flag to handle it specially */
		if (atStart && !isspace(c))
		{
			atStart = false;
			if (inComment)
			{
				/* set flag to act at the end of the comment */
				buf->fileComment = true;
			}
			else
			{
				/* it's not a comment, so insert a new file comment */
				newFileComment(buf);
			}
		}

		if (emitChar(buf, c) != 0)
		{
			 /* buffer overflowed! hopefully this won't
				happen very often... no choice but to flush */
			doFlush = true;
		}

		if (doFlush)
		{
			doFlush = false;
			flushBuffer(buf);
			buf->fileComment = false;
		}

		 /* if the currect character isn't whitespace, we're
			not at the first character of the line any more. */ 
		if (isChar1 && !isspace(c))
			isChar1 = false;

		prevc = c;
		c = nextc;

	} /* end while */

	/* flush whatever may be left in the buffer */
	flushBuffer(buf);
	free(buf);
	
	return 0;
}
