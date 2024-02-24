#ifndef UTIL_H
#define UTIL_H

#include <time.h>
#include <stdbool.h>
#include "readcmd.h"
#include "csapp.h"

#define NEW_LINE 1
#define SAME_LINE 0

void init_util(pid_t gid);
void delay_new_line();
void printWelcome(bool newLine);
void shell_give_control(pid_t gid);
void shell_regain_control();
void execute(char **cmd);
void end_session(struct cmdline **l);

#endif