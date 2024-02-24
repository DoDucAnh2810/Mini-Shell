/* util - Tools for the shell */
#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include "readcmd.h"
#include "csapp.h"

#define NEW_LINE 1
#define SAME_LINE 0
#define FILE 1
#define COMMAND 0

/* Return true if s1 is exactly s2 */
bool strings_equal(char *s1, char *s2);

/* Return true if str corresponding to an integrated command */
bool integrated_command(char *str);

/* Intialize the util object.
   MUST CALL this function BEFORE any function here */
void init_util(pid_t gid);

/* Wait for a split second */
void delay_new_line();

/* Print the "shell>" messange */
void print_welcome(bool newLine);

/* Give terminal control to a group */
void shell_give_control(pid_t gid);

/* Give terminal control to shell */
void shell_regain_control();

/* Handle error according to errno */
void error_hander(char *pathname, bool isFile);

/* Execute a command */
void execute(char **cmd);

/* Free memory and end shell */
void end_session(struct cmdline **l);

/* Close the file descriptors of a pipe */
void close_pipe(int *pipe);

#endif