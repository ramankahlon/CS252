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
#include <string>
#include "y.tab.hh"
using namespace std;

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

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
	return AMPERSAND;
}

\".*\" {
	//printf("yytext = %s\n",yytext);
	string str = yytext;
	str = str.substr(1,str.length()-2);
	yylval.cpp_string = new string(str);
	return WORD;
}

[^ \t\n<>|&\"][^ \t\n<>|&\"]* {
  /* Assume that file names have only alpha chars */
/*	printf("yytext = %s\n",yytext);
	char * str = (char *)malloc(strlen(yytext)+1);
	bool esc = false;
	int index = 0;
	int i = 0;
	while(i<strlen(yytext)) 
	{
		//printf("esc = %d\n",esc);
		if(yytext[i]=='\\' && esc == false)
			esc = true;
		else if(esc == true)
		{
			str[index] = yytext[i];
			index++;
			esc = false;
		}
		else
		{
			str[index] = yytext[i];
			index++;
		}
		//printf("str= %s\n", str);
		i++;
	}
	str[index] = '\0';
	//printf("str= %s\n", str);
	strcpy(yytext, str);
	free(str);
	yylval.string_val = strdup(yytext);
	return WORD;
*/
	//yylval.cpp_string = new string(str);
	//free(str);
	//return WORD;

	
	//string str = yytext;
	//str = str.substr(1,str.length()-2);
	//yylval.cpp_string = new string(str);
	//return WORD;
	//printf("inside expr");
	yylval.cpp_string = new string(yytext);
  	return WORD;
}

. {
	/* Invalid character in input */
	return NOTOKEN;
}
