#ifndef UTIL_H
#define UTIL_H

#include <time.h>
#include <stdbool.h>
#include "readcmd.h"
#include "csapp.h"

#define NEW_LINE 1
#define SAME_LINE 0
#define FILE 1
#define COMMAND 0

bool strings_equal(char *s1, char *s2);
void init_util(pid_t gid);
void delay_new_line();
void printWelcome(bool newLine);
void shell_give_control(pid_t gid);
void shell_regain_control();
void error_hander(char *pathname, bool isFile);
void execute(char **cmd);
void end_session(struct cmdline **l);

#endif