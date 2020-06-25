/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{
#include <string>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE GREATGREAT TWOGREAT GREATGREATAMPERSAND PIPE AMPERSAND LESS GREATAMPERSAND

%{
#define yylex yylex
#include <cstdio>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include <unistd.h>
#include "shell.hh"
#include "command.hh"

void expandWildcardsIfNecessary(std::string * arg);
int cmpfunc (const void *file1, const void *file2);
void expandWildCards(char * prefix, char * arg);
void yyerror(const char * s);
int yylex();


%}

%%

goal:
  command_line
  ;

command_line:
  cmd
  | command_line cmd
  ;

cmd:
  simple_command
  ;

background_optional:
  AMPERSAND {
	/*printf("Yacc: background true\n");*/
	Shell::_currentCommand._background = 1;
  }
  | /*empty*/
  ;

pipe_list:
  pipe_list PIPE cmd_and_args
  | cmd_and_args
  ;

simple_command:
  pipe_list io_modifier_list background_optional NEWLINE {
	/*printf("Yacc: Execute command\n");*/
	Shell::_currentCommand.execute();
  }
  | NEWLINE
  | error NEWLINE { yyerrok; }
  ;

cmd_and_args:
  command arg_list {
	Shell::_currentCommand.insertSimpleCommand(Command::_currentSimpleCommand);
  }
  ;

arg_list:
  arg_list argument
  | /* can be empty */
  ;

argument:
  WORD {
	/*printf("Yacc: insert argument \"%s\"\n", $1->c_str());*/
	/*Command::_currentSimpleCommand->insertArgument( $1 );*/
  	expandWildcardsIfNecessary( $1 );
  }
  ;

command:
  WORD {
	/*printf("Yacc: insert command \"%s\"\n", $1->c_str());*/
	Command::_currentSimpleCommand = new SimpleCommand();
	Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

io_modifier_list:
  io_modifier_list io_modifier_opt
  |
  ;

io_modifier_opt:
  GREATGREAT WORD {
	if(Shell::_currentCommand._outFile == NULL)
	{
		/*printf("Yacc: append output \"%s\"\n", $2->c_str());*/
		Shell::_currentCommand._append = 1;
		Shell::_currentCommand._outFile = $2;
		Shell::_currentCommand._out_flag++;
  	}
	else
		printf("Ambiguous output redirect.\n");
  }
  | GREAT WORD {
	if(Shell::_currentCommand._outFile == NULL)
	{
		/*printf("Yacc: insert output \"%s\"\n", $2->c_str());*/
		Shell::_currentCommand._outFile = $2;
		Shell::_currentCommand._out_flag++;
  	}
	else
		printf("Ambiguous output redirect.\n");
  }
  | GREATGREATAMPERSAND WORD {
	if(Shell::_currentCommand._outFile == NULL)
	{
		/*printf("Yacc: append output and stderr \"%s\"\n", $2->c_str());*/
		Shell::_currentCommand._append = 1;
		Shell::_currentCommand._outFile = $2;
		Shell::_currentCommand._out_flag++;
  	}
	else
		printf("Ambiguous output redirect.\n");
	if(Shell::_currentCommand._errFile == NULL)
	{
		Shell::_currentCommand._append = 1;
		Shell::_currentCommand._errFile = $2;
		Shell::_currentCommand._err_flag++;
	}
	else
		printf("Ambiguous output redirect.\n");
  }
  | GREATAMPERSAND WORD {
	if(Shell::_currentCommand._outFile == NULL)
	{
		/*printf("Yacc: insert output and stderr \"%s\"\n", $2->c_str());*/
		Shell::_currentCommand._append = 1;
		Shell::_currentCommand._outFile = $2;
		Shell::_currentCommand._out_flag++;
  	}
	else
		printf("Ambiguous output redirect.\n");
	if(Shell::_currentCommand._errFile == NULL)
	{
		Shell::_currentCommand._append = 1;
		Shell::_currentCommand._errFile = $2;
		Shell::_currentCommand._err_flag++;
	}
	else
		printf("Ambiguous output redirect.\n");
  }
  | LESS WORD {
	if(Shell::_currentCommand._inFile == NULL)
	{
		/*printf("Yacc: insert input \"%s\"\n", $2->c_str());*/
		Shell::_currentCommand._inFile = $2;
  	}
	else
		printf("Ambiguous output redirect.\n");
  }
  | TWOGREAT WORD {
	/*error flag*/
	if(Shell::_currentCommand._errFile == NULL)
	{
		Shell::_currentCommand._errFile = $2;
		Shell::_currentCommand._err_flag++;
	}
	else
		printf("Ambiguous output redirect.\n");
  }
  ;

%%

int maxEntries = 20;
int nEntries = 0;
char ** entries;

void expandWildcardsIfNecessary(std::string * arg) 
{
	char * arg_new = (char *) malloc(arg->length()+1);
	strcpy(arg_new,arg->c_str());
	maxEntries = 20;
	nEntries = 0;
	entries = (char **) malloc (maxEntries * sizeof(char *));

	if (strchr(arg_new, '*') || strchr(arg_new, '?')) 
	{
		expandWildCards(NULL, arg_new);
		if(nEntries == 0)
		{
			Command::_currentSimpleCommand->insertArgument(arg);
			return;
		}
		qsort(entries, nEntries, sizeof(char *), cmpfunc);
		for (int i = 0; i < nEntries; i++) 
		{
			std::string * str = new std::string(entries[i]);
			Command::_currentSimpleCommand->insertArgument(str);
		}
	}
	else
		Command::_currentSimpleCommand->insertArgument(arg);
	return;
}

int cmpfunc (const void *file1, const void *file2) 
{
	const char *_file1 = *(const char **)file1;
	const char *_file2 = *(const char **)file2;
	return strcmp(_file1, _file2);
}

void expandWildCards(char * prefix, char * arg)
{
	char * temp = arg;
	char * save = (char *) malloc (strlen(arg) + 10);
	char * dir = save;

	if(temp[0] == '/')
		*(save++) = *(temp++);

	while (*temp != '/' && *temp) 
		*(save++) = *(temp++);
	
	*save = '\0';

	if (strchr(dir, '*') || strchr(dir, '?')) 
	{
		if (!prefix && arg[0] == '/') 
		{
			prefix = strdup("/");
			dir++;
		}  

		char * reg = (char *) malloc (2*strlen(arg) + 10);
		char * a = dir;
		char * r = reg;

		*r = '^';
		r++;
		while (*a) 
		{
			if (*a == '*') { *r='.'; r++; *r='*'; r++; }
			else if (*a == '?') { *r='.'; r++; }
			else if (*a == '.') { *r='\\'; r++; *r='.'; r++; }
			else { *r=*a; r++; }
			a++;
		}
		*r = '$';
		r++;
		*r = '\0';

		regex_t re;

		int expbuf = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);

		char * toOpen = strdup((prefix)?prefix:".");
		DIR * dir = opendir(toOpen);
		if (dir == NULL) 
		{
			perror("opendir");
			return;
		}

		struct dirent * ent;
		regmatch_t match;
		/*bool ismatch = false;*/
		while ((ent = readdir(dir)) != NULL) 
		{
			if (!regexec(&re, ent->d_name, 1, &match, 0)) 
			{
				/*ismatch = true;*/
				if (*temp) 
				{
					if (ent->d_type == DT_DIR) 
					{
						char * nPrefix = (char *) malloc (150);
						if (!strcmp(toOpen, ".")) nPrefix = strdup(ent->d_name);
						else if (!strcmp(toOpen, "/")) sprintf(nPrefix, "%s%s", toOpen, ent->d_name);
						else sprintf(nPrefix, "%s/%s", toOpen, ent->d_name);
						expandWildCards(nPrefix, (*temp == '/')?++temp:temp);
					}
				}
				else 
				{	
					if (nEntries == maxEntries) 
					{ 
						maxEntries *= 2; 
						entries = (char **) realloc (entries, maxEntries * sizeof(char *)); 
					}
					char * argument = (char *) malloc (100);
					argument[0] = '\0';
					if (prefix)
						sprintf(argument, "%s/%s", prefix, ent->d_name);

					if (ent->d_name[0] == '.') 
					{
						if (arg[0] == '.')
							entries[nEntries++] = (argument[0] != '\0')?strdup(argument):strdup(ent->d_name);
					}
					else
						entries[nEntries++] = (argument[0] != '\0')?strdup(argument):strdup(ent->d_name);
				}
			}
		}
		/*if(ismatch == false)
		{
			Command::_currentSimpleCommand->insertArgument(arg);
		}*/
		closedir(dir);
	} 
	else 
	{
		char * preToSend = (char *) malloc (100);
		if(prefix) 
			sprintf(preToSend, "%s/%s", prefix, dir);
		else
			preToSend = strdup(dir);

		if(*temp)
			expandWildCards(preToSend, ++temp);
	}
}


void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
