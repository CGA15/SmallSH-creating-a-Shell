# SmallSH-creating-a-Shell-of-sorts

Project Goal:

Create our own shell written in C that does the following:

    -Provide a prompt for running commands

    -Handle blank lines and comments, which are lines beginning with the # character

    -Provide expansion for the variable $$

    -Execute 3 commands exit, cd, and status via code built into the shell

    -Execute other commands by creating new processes using a function from the exec family of functions

    -Support input and output redirection

    -Support running commands in foreground and background processes

    -Implement custom handlers for 2 signals, SIGINT and SIGTSTP


Compile using gcc -o smallsh main.c -lreadline
In a linux terminal
