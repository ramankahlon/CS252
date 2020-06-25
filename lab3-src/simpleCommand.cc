#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <iostream>

#include "simpleCommand.hh"

SimpleCommand::SimpleCommand() 
{
  _arguments = std::vector<std::string *>();
}

SimpleCommand::~SimpleCommand() {
  // iterate over all the arguments and delete them
  for (auto & arg : _arguments) {
    delete arg;
  }
}

void SimpleCommand::insertArgument( std::string * argument ) {
  // simply add the argument to the vector
	int i=0;
	char * str = strdup(argument -> c_str());
	char * arg = strdup(argument -> c_str());
	//char * str = (char *)malloc(strlen(argument)+1);
	char * text = str;
	bool esc = false;
	if(*text == '~')
	{
		
	}
	else{
	while(*text!='\0')
	{
		if(!esc && *text == '\\')
		{
			esc = true;
		}
		else
		{
			str[i] = *text;
			i++;
			esc = false;
		}
		text++;
	}
	str[i] = '\0';
	}
	
	std::string * pb = new std::string(str);
	_arguments.push_back(pb);
	free(str);
	delete argument;

/*
	//Environment variable expansion
	char * env = expansion(arg);
	if(env)
		arg = strdup(env);

	env = tilde(arg);

	if(env) 
		arg = strdup(env);

	_arguments[ argcount ] = arg;

	// Add NULL argument at the end
	_arguments[ argcount + 1] = NULL;

	argcount++;
*/
}

// Print out the simple command
void SimpleCommand::print() {
  for (auto & arg : _arguments) {
    std::cout << "\"" << *arg << "\" \t";
  }
  // effectively the same as printf("\n\n");
  std::cout << std::endl;
}
