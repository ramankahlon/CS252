/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include <string>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "y.tab.hh"
using namespace std;

static  void yyunput (int c,char *buf_ptr  );

extern char* dir;

extern "C" char * read_line();

void myunputc(int c) {
  unput(c);
}

int mygetc(FILE * f)
{
	static char *p;
	char ch;

	if(!isatty(0)){
		return getc(f);
	}
	if(p==NULL || *p==0)
	{
		char * s = read_line();
		p = s;
	}

	ch = *p;
	p++;
	return ch;
}
#undef getc
#define getc(f) mygetc(f)
%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
	//printf("flag 2 yytext = %s\n",yytext);
  	return GREAT;
}

"<" {
	return LESS;
}

">>" {
	return GREATGREAT;
}

">&" {
	return GREATAMPERSAND;
}

">>&" {
	return GREATGREATAMPERSAND;
}

"2>" {
	return TWOGREAT;
}

"|" {
	return PIPE;
}

"&" {
	//printf("flag 3 yytext = %s\n",yytext);
	return AMPERSAND;
}

[$][{][A-Za-z0-9$?!\/ \-\_]+[}] {
	char * c = (char *)malloc(strlen(yytext)+1);
	int j=0;
	for(int i=0;i<strlen(yytext);i++)
	{
		if(i > 1 && i < strlen(yytext)-1)
			c[j++] = yytext[i];
	}
	c[j] = '\0';
	
	char * value = getenv(c);
	if(value != NULL && strcmp(c, "SHELL")!=0 && strcmp(c, "_")!=0)
		yylval.cpp_string = new string(value);
	else
		yylval.cpp_string = new string(yytext);
	//printf("string=%s",yylval.cpp_string->c_str());
	return WORD;
}

\`[^\n\"]*\`|\$\([^\n\"]*\) {
	char * str = yytext;
	//printf("str = %s", str);
	if(str[0] == '`')
		str++;
	else
		str += 2;
	str[strlen(str)-1] = '\0';
        char * cmd = strdup(str);
	//printf("str = %s", cmd); 

         int pipein[2];
         int pipeout[2];
         int in = dup(0);
         int out = dup(1);

         int size = 4096;
         int shift = 0;
         char c;
         char * output = (char *)malloc(size);
	 strncpy(output, (char*)"", 10);

         pipe(pipein);
         pipe(pipeout);

         #define childWrite pipeout[1]
         #define childRead pipein[0]
         #define parentWrite pipein[1]
         #define parentRead pipeout[0]
/*	 write(pipeout[1], cmd, strlen(cmd));
	 write(pipeout[1], "\nexit\n", 6);
	 close(pipeout[1]); 
*/

	write(pipein[1], cmd, strlen(cmd));
	write(pipein[1], "\nexit\n", 6);
	int ret = fork();
         if(ret < 0)
         {
                 perror("fork");
                 exit(1);
         }
         else if(ret == 0)
         {
           /*    if(close(parentRead) < 0)
                 {
                         perror("close child pipe out");
			 exit(1);
                 }
                 if(dup2(childRead, 0) < 0)
                 {
                         perror("parent pipe in");
                         exit(1);
                 }
                 if(dup2(parentWrite, 1) < 0)
                 {
                         perror("child pipe out");
                         exit(1);
                 }
		close(childWrite);
		close(childRead);
             */ 
		//write(parentWrite, cmd, strlen(cmd));
		//write(parentWrite, "\nexit\n", 6);
		close(parentWrite);
		close(parentRead);

		//printf("cmd = %s", cmd);
		//printf("\nexit\n");
		//dup2(parentRead, 1);
		//dup2(parentWrite, 1);
		dup2(childWrite, 1);
		close(childWrite);

		dup2(childRead, 0);
		close(childRead);
		//close(pipeout[0]);

		execvp("/proc/self/exe", NULL);
		//perror("subshell");
		_exit(1);
         }
         else
         {
 /*
                 if(close(parentWrite) < 0)
                 {
                         perror("close parent pipe in");
                         exit(1);
                 }
                 waitpid(ret, NULL, 0);
 
                 if(close(childWrite) < 0)
                 {
                         perror("close child pipe out");
                         exit(1);
                 }
                 if(close(childRead) < 0)
                 {
                         perror("close child pipe in");
                         exit(1);
                 }
                 if(read(parentRead, &c, 1) < 0)
                 {
                         perror("Reading");
                         exit(0);
                 }
		 dup2(in, 0);
		 dup2(out, 1);
		 close(in);
		 close(out);
*/
		 close(pipein[1]);
		 close(pipeout[1]);
                close(pipein[0]); 
		//output[0] = c;
                 //shift++;
                 
                 while(read(pipeout[0], &c, 1) > 0)
                 {
                 	output[shift] = c;
			if(shift+1 > size)
                         {
                                 output = (char *)realloc(output,(sizeof(char*)*(size*2)));
                                 size += 2;
                         }
                         shift++;
			//printf("output = %c\n", c);
                 }
		 output[shift] = '\0';
         /*        if(close(parentRead) < 0)
                 {
                         perror("close child pipe out");
                         exit(1);
                 }*/
                 
                 if(close(pipeout[0]) < 0)
                 {
                         perror("close child pipe out");
                         exit(1);
                 }
       		//int i = 0;
		//for(i = 0; i<shift;i++)
			//printf("in loop output = %c\n",output[i]);
        
	         if(dup2(in, 0) < 0)
                         perror("stdin");
                 if(dup2(out, 1) < 0)
                         perror("stdout");

		//printf("shift = %d\n", shift); 
		for(int i=shift-1; i>=0; i--)
                 {
			//printf("in loop");
                         if(output[i] == '\n')
                                 output[i] = ' ';
			//printf("last output = %c\n", output[i]);
			myunputc(output[i]);
                 }
	}
}

\".*\" {
	//printf("yytext = %s\n",yytext);
	string str = yytext;
	str = str.substr(1,str.length()-2);
	yylval.cpp_string = new string(str);
	return WORD;
}


[^ \t\n<>&|]((\\.)|[^ \t\n<>&|])* {
	char * c = (char *)malloc(strlen(yytext)+1000);
	bool flag = false;
	char * word = (char *)malloc(strlen(yytext)+1000);
	int counter = 0;
	int counter2 = 0;
	for(int i=0;i<strlen(yytext);i++)
	{
		if(yytext[i] == '$')
		{
			if(yytext[i+1] == '{')
			{
				i++;
			}
			flag = true;
		}
		else if(yytext[i] == '}')
		{
			flag = false;
			c[counter] = '\0';
		}
		else if(flag == true)
		{
			word[counter2++] = yytext[i];
		}
		else
		{
			c[counter++] = yytext[i];
		}
	}
	yylval.cpp_string = new string(yytext);
	return WORD;
}

. {
	/* Invalid character in input */
	return NOTOKEN;
}
