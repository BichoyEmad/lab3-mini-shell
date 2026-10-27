/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

// helper to clean up simple command args on error
void free_simple_command(SimpleCommand *s_cmd)
{
    if (s_cmd)
    {
        for (int j = 0; j < s_cmd->_numberOfArguments; j++)
        {
            free(s_cmd->_arguments[j]);
        }
        free(s_cmd->_arguments);
        delete s_cmd;
    }
}

// handle logging terminated child procs
void sigchld_handler(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
        FILE *log_file = fopen("shell.log", "a");
        if (log_file != NULL)
        {
            time_t now = time(NULL);
            fprintf(log_file, "Child process terminated at: %s", ctime(&now));
            fclose(log_file);
        }
    }
}

void parse(std::vector<Token> &tokens)
{
    Command cur_cmd;
    SimpleCommand *curr_s_cmd = new SimpleCommand();

    if (!tokens.empty() && tokens[0].type == TOKEN_PIPE)
    {
        fprintf(stderr, "error invalid token '|'\n");
        free_simple_command(curr_s_cmd);
        return;
    }

    bool loop = true;
    for (size_t i = 0; i < tokens.size() && loop; i++)
    {
        Token tkn = tokens[i];

        switch (tkn.type)
        {
        case TOKEN_EOF:
          loop = false;
          break;
        case TOKEN_COMMAND:
        case TOKEN_ARGUMENT:
          curr_s_cmd->insertArgument(strdup(tkn.value.c_str()));
          break;
        case TOKEN_PIPE:
          if (curr_s_cmd->_numberOfArguments == 0
                || i + 1 >= tokens.size()
                || tokens[i + 1].type == TOKEN_PIPE
                || tokens[i + 1].type == TOKEN_EOF
            )
            {
                fprintf(stderr, "error unexpected token '|'\n");
                free_simple_command(curr_s_cmd);
                cur_cmd.clear();
                return;
            }
            cur_cmd.insertSimpleCommand(curr_s_cmd);
            curr_s_cmd = new SimpleCommand();
            break;
          // o/p redirect '>'
          case TOKEN_REDIRECT:
            i++;
              if (i >= tokens.size() || (tokens[i].type != TOKEN_ARGUMENT && tokens[i].type != TOKEN_COMMAND))
              {
                  fprintf(stderr, "errorinvalidredirection file\n");
                  free_simple_command(curr_s_cmd);
                  cur_cmd.clear();
                  return;
              }
              cur_cmd._outFile = strdup(tokens[i].value.c_str());
              break;
            // append redirect '>>'
            case TOKEN_APPEND:
              i++;
              if (i >= tokens.size() || (tokens[i].type != TOKEN_ARGUMENT && tokens[i].type != TOKEN_COMMAND))
              {
                  fprintf(stderr, "error invalid append file\n");
                  free_simple_command(curr_s_cmd);
                  cur_cmd.clear();
                  return;
              }
              cur_cmd._outFile = strdup(tokens[i].value.c_str());
              cur_cmd._append = 1;
              break;
            // i/p redirect '<'
            case TOKEN_INPUT:
              i++;
              if (i >= tokens.size() || (tokens[i].type != TOKEN_ARGUMENT && tokens[i].type != TOKEN_COMMAND))
              {
                  fprintf(stderr, "error invalid input file\n");
                  free_simple_command(curr_s_cmd);
                  cur_cmd.clear();
                  return;
              }
              cur_cmd._inputFile = strdup(tokens[i].value.c_str());
              break;
            // err redir '2>'
            case TOKEN_ERROR:
              i++;
              if (i >= tokens.size() || (tokens[i].type != TOKEN_ARGUMENT && tokens[i].type != TOKEN_COMMAND))
              {
                  fprintf(stderr, "error invalid error redirection file\n");
                  free_simple_command(curr_s_cmd);
                  cur_cmd.clear();
                  return;
              }
              cur_cmd._errFile = strdup(tokens[i].value.c_str());
              break;
            // combined out & err '>>&'
            case TOKEN_REDIRECT_AND_ERROR:
              i++;
              if (i >= tokens.size() || (tokens[i].type != TOKEN_ARGUMENT && tokens[i].type != TOKEN_COMMAND))
              {
                  fprintf(stderr, "error invalid out/err file\n");
                  free_simple_command(curr_s_cmd);
                  cur_cmd.clear();
                  return;
              }
              cur_cmd._outFile = strdup(tokens[i].value.c_str());
              cur_cmd._errFile = strdup(tokens[i].value.c_str());
              cur_cmd._out_error = 1;
              break;
            // background '&'
            case TOKEN_BACKGROUND:
              cur_cmd._background = 1;
        }
    }

    if (curr_s_cmd->_numberOfArguments > 0)
    {
        cur_cmd.insertSimpleCommand(curr_s_cmd);
    }
    else
    {
        free_simple_command(curr_s_cmd);
    }

    cur_cmd.execute();
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
    // check if argument has wildcards (* or ?)
    if (strchr(argument, '*') != NULL || strchr(argument, '?') != NULL)
    {
        glob_t glob_res;
        memset(&glob_res, 0, sizeof(glob_res));
        
        int res = glob(argument, 0, NULL, &glob_res);
        if (res == 0)
        {
            free(argument); // free original pattern
            for (size_t i = 0; i < glob_res.gl_pathc; i++)
            {
                if (_numberOfAvailableArguments <= _numberOfArguments + 1)
                {
                    _numberOfAvailableArguments *= 2;
                    _arguments = (char **)realloc(_arguments, _numberOfAvailableArguments * sizeof(char *));
                }
                _arguments[_numberOfArguments] = strdup(glob_res.gl_pathv[i]);
                _arguments[_numberOfArguments + 1] = NULL;
                _numberOfArguments++;
            }
            globfree(&glob_res);
            return;
        }
        else
        {
            globfree(&glob_res);
            // if no match, keep pattern as is
        }
    }

    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _numberOfSimpleCommands = 0;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }
    
    if (_simpleCommands)
    {
        free(_simpleCommands);
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile && !_out_error)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute()
{
    if (_numberOfSimpleCommands == 0)
    {
        clear();
        prompt();
        return;
    }

    // save default std files
    int defaultStdin = dup(0);
    int defaultStdout = dup(1);
    int defaultStderr = dup(2);

    int fd_in = 0;
    int fd_out = 0;

    if (_inputFile)
    {
        fd_in = open(_inputFile, O_RDONLY);
        if (fd_in < 0)
        {
            perror("cant open input file");
            clear();
            prompt();
            return;
        }
    }
    else
    {
        fd_in = dup(defaultStdin);
    }

    pid_t lst_pid = 0;

    // loop in pipeline
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        dup2(fd_in, 0);
        close(fd_in);

        // check if last cmd in pipe
        if (i == _numberOfSimpleCommands - 1)
        {
            if (_outFile)
            {
                int flgs = O_WRONLY | O_CREAT;
                if (_append)
                {
                    flgs |= O_APPEND;
                }
                else
                {
                    flgs |= O_TRUNC;
                }

                fd_out = open(_outFile, flgs, 0666);
                if (fd_out < 0)
                {
                    perror("cant open output");
                    clear();
                    prompt();
                    return;
                }

                if (_out_error)
                {
                    dup2(fd_out, 2);
                }
            }
            else
            {
                fd_out = dup(defaultStdout);
            }

            if (_errFile && !_out_error)
            {
                int err_flgs = O_WRONLY | O_CREAT | O_APPEND;
                int fd_err = open(_errFile, err_flgs, 0666);
                if (fd_err >= 0)
                {
                    dup2(fd_err, 2);
                    close(fd_err);
                }
            }
        }
        else
        {
            int p_fds[2];
            if (pipe(p_fds) < 0)
            {
                perror("pipe err");
                clear();
                prompt();
                return;
            }
            fd_out = p_fds[1]; // write end
            fd_in = p_fds[0];  // read end
        }

        dup2(fd_out, 1);
        close(fd_out);

        // exit
        if (_numberOfSimpleCommands == 1 && strcmp(_simpleCommands[0]->_arguments[0], "exit") == 0)
        {
            printf("Good bye!!\n");
            exit(0);
        }

        // cd
        if (_numberOfSimpleCommands == 1 && strcmp(_simpleCommands[0]->_arguments[0], "cd") == 0)
        {
            char *d = _simpleCommands[0]->_arguments[1];
            if (d == NULL)
            {
                d = getenv("HOME");
            }
            if (chdir(d) < 0)
            {
                perror("cd err");
            }
            dup2(defaultStdin, 0);
            dup2(defaultStdout, 1);
            dup2(defaultStderr, 2);
            close(defaultStdin);
            close(defaultStdout);
            close(defaultStderr);
            clear();
            prompt();
            return;
        }

        // fork proc
        pid_t p_id = fork();
        if (p_id < 0)
        {
            perror("fork err");
            clear();
            prompt();
            return;
        }

        if (p_id == 0)
        {
            // child execution
            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            perror("execvp err");
            _exit(1);
        }
        else
        {
            lst_pid = p_id;
        }
    }

    // restore stds
    dup2(defaultStdin, 0);
    dup2(defaultStdout, 1);
    dup2(defaultStderr, 2);
    close(defaultStdin);
    close(defaultStdout);
    close(defaultStderr);

    if (!_background)
    {
        waitpid(lst_pid, NULL, 0);
    }

    clear();
    prompt();
}

void Command::prompt()
{
    printf("myshell>");
    fflush(stdout);
    std::string input;
    while (true)
    {
        std::getline(std::cin, input); 
        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    // ignore ctrl-c (SIGINT)
    signal(SIGINT, SIG_IGN);

    // setup sigchld handler for logging terminated children
    signal(SIGCHLD, sigchld_handler);

    Command::_currentCommand.prompt();
    return 0;
}
