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
//#define yylex yylex
#include <cstdio>
//#include <stdio.h>
#include <string.h>
#include "shell.hh"
//#include "command.hh"

void yyerror(const char * s);
int yylex();

%}

%%

goal:
  command_line
  ;

command_line:
  command
  | command_line command
  ;

command: simple_command
  ;

background_optional:
  AMPERSAND {
	printf("Yacc: background true\n");
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
	printf("Yacc: Execute command\n");
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
	printf("Yacc: insert argument \"%s\"\n", $1->c_str());
	Command::_currentSimpleCommand->insertArgument( $1 );	
  }
  ;

command:
  WORD {
	printf("Yacc: insert command \"%s\"\n", $1->c_str());
	Shell::_currentCommand._currentSimpleCommand = new SimpleCommand();
	Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

io_modifier_list:
  io_modifier_list io_modifier_opt
  |
  ;

io_modifier_opt:
  GREATGREAT WORD {
	printf("Yacc: append output \"%s\"\n", $2->c_str());
	Shell::_currentCommand._append = 1;
	Shell::_currentCommand._outFile = $2;
  	Shell::_currentCommand._out_flag++;
  }
  | GREAT WORD {
	printf("Yacc: insert output \"%s\"\n", $2->c_str());
	Shell::_currentCommand._outFile = $2;
  	Shell::_currentCommand._out_flag++;
  }
  | GREATGREATAMPERSAND WORD {
	printf("Yacc: append output and stderr \"%s\"\n", $2->c_str());
	Shell::_currentCommand._append = 1;
	Shell::_currentCommand._outFile = $2;
	Shell::_currentCommand._errFile = strdup($2);
	Shell::_currentCommand._out_flag++;
  }
  | GREATAMPERSAND WORD {
	printf("Yacc: insert output and stderr \"%s\"\n", $2->c_str());
	Shell::_currentCommand._outFile = $2;
	Shell::_currentCommand._errFile = strdup($2);
	Shell::_currentCommand._out_flag++;
  }
  | LESS WORD {
	printf("Yacc: insert input \"%s\"\n", $2->c_str());
	Shell::_currentCommand._inFile = $2;
  }
  | TWOGREAT WORD {
	/*error flag*/
	Shell::_currentCommand._errFile = strdup($2);
  }
  ;

%%

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
