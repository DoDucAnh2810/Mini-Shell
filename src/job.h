#ifndef __JOB_H__
#define __JOB_H__

#include <stdbool.h>
#include "csapp.h"

#define MAX_JOB 16383
#define MAX_COMMAND 127
#define NOT_FOUND -1
#define UNDEFINED 0
#define STOPPED 1
#define RUNNING 2
#define TERMINATED 3
#define FINISHED 4

typedef struct {
	pid_t gid;
    short state;
	char *command;
} job_t;

void init_job_history();

int find_job_number(pid_t gid);

pid_t find_job_pid(int number);

void set_job_state(int number, short state);

void print_job(int number);

void print_jobs();

void new_job(pid_t gid, short state, char *seq_string);

int job_argument_parser(char *str);

void kill_all_job();

int job_count();

void destroy_job_history();

#endif