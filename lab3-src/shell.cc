#include <cstdio>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include "shell.hh"
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

void yyrestart(FILE * file);
int yyparse(void);
char * dir;

void Shell::prompt() {
//printf("in prompt");
  if(isatty(0))
	printf("myshell>");
//printf("after prompt");
  fflush(stdout);
}

extern "C" void disp( int sig )
{
	fprintf( stderr, "\nsig:%d	Exit\n", sig);
	Shell::prompt();
}

extern "C" void disp2( int sig )
{
	wait3(0,0,NULL);
	while(waitpid(-1, NULL, WNOHANG) > 0)
	{
	}
}

int main() {
	struct sigaction sa;
	sa.sa_handler = disp;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if(sigaction(SIGINT, &sa, NULL)){
		perror("sigaction");
		exit(2);
	}

	struct sigaction sa2;
	sa2.sa_handler = disp2;
	sigemptyset(&sa2.sa_mask);
	sa2.sa_flags = SA_RESTART;

	if(sigaction(SIGCHLD, &sa2, NULL)){
		perror("sigaction");
		exit(2);
	}

	//Source shellrc
	FILE * fd = fopen(".shellrc", "r");
	if(fd)
	{
		yyrestart(fd);
		yyparse();
		yyrestart(stdin);
		fclose(fd);
	}
	else
		Shell::prompt();

//	printf("before pwd");
	int size = sizeof(char*)*(strlen(getenv("PWD")) +1+strlen(getenv("_")));
	dir = (char *)malloc(size);
	strncpy(dir, getenv("PWD"), size);
	strcat(dir, "/");
	strcat(dir, getenv("_"));
//	printf("after pwd");

	Shell::prompt();
//	printf("before yyparse");
	yyparse();
}

Command Shell::_currentCommand;
