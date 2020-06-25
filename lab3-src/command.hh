#ifndef command_h
#define command_h

#include "simpleCommand.hh"

// Command Data Structure

struct Command {
  int _numOfAvailableSimpleCommands;
  int _numOfSimpleCommands;
  std::vector<SimpleCommand *> _simpleCommands;
  std::string * _outFile;
  std::string * _inFile;
  std::string * _errFile;
  int _background;
  int _append;
  int _out_flag;
  int _err_flag;
  int _in_flag;

  void prompt();
  void print();
  void execute();
  void clear();

  Command();
  void insertSimpleCommand( SimpleCommand * simpleCommand );

  //static Command _currentCommand;
  static SimpleCommand *_currentSimpleCommand;
};

#endif
