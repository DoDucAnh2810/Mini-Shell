/* job - Implementation */
#ifndef __JOB_H__
#define __JOB_H__

#include "csapp.h"

#define MAX_JOB 16383
#define MAX_COMMAND 127
#define NOT_FOUND -1
#define UNDEFINED 0
#define STOPPED 1
#define RUNNING 2
#define TERMINATED 3
#define FINISHED 4

/* Intialize the table of jobs */
void init_job_history();

/* Return the job number corresponding to gid if exists, NOT_FOUND otherwise */
int find_job_number(pid_t gid);

/* Return the job's gid corresponding to number */
pid_t find_job_gid(int number);

/* Set a job's state to be state */
void set_job_state(int number, short state);

/* Print a job's information */
void print_job(int number);

/* Print all job's information  */
void print_jobs();

/* Add a new job to the record */
void new_job(pid_t gid, short state, int nb_command, char *seq_string);

/* Return the job number from a string if exists, NOT_FOUND otherwise */
int job_argument_parser(char *str);

/* Send SIGKILL to all jobs */
void kill_all_job();

/* Return the current number of jobs*/
int job_count();

/* Free the table of jobs*/
void destroy_job_history();

/* Decrement the number of process existing within a job
   If number of process existing hits 0, set job state to be state. */
void decrement_nb_exist(int number, short state);

/* Decrement the number of process running within a job */
void decrement_nb_running(int number);

/* Wait for the number of process running within a job to hit 0 */
void wait_for_job(int number);

#endif