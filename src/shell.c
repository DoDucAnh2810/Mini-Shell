/*
 * Copyright (C) 2002, Simon Nieuviarts
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include "readcmd.h"
#include "csapp.h"
#include "job.h"
#include "gid_tracker.h"
#include "util.h"

static int nb_reaped;
static struct cmdline *l;

void handlerSIGCHLD() {
	int number, status;
	pid_t pid, gid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		gid = findGidByPid(pid);
		number = find_job_number(gid);
		if (WIFEXITED(status)) {
			if (number != NOT_FOUND)
				decrement_nb_exist(number, FINISHED);
			else
				deleteTracker(pid);
		} else if(WIFSIGNALED(status)) {
			if (WTERMSIG(status) == SIGINT)  { printf("\n"); fflush(stdout);}
			if (number != NOT_FOUND)
				decrement_nb_exist(number, TERMINATED);
			else
				deleteTracker(pid);
		} else if (WIFSTOPPED(status)) {
			if (WSTOPSIG(status) == SIGTSTP) { printf("\n"); fflush(stdout);}
			if (number == NOT_FOUND)
				new_job(pid, STOPPED, l->seq_len - nb_reaped, l->seq_string);
			else
				decrement_nb_running(number);
		}
		nb_reaped++;
	}
}

void handlerPrintNewLine() {
	printWelcome(NEW_LINE);
}

int main(int argc, char **argv) {
	Signal(SIGCHLD, handlerSIGCHLD);
	Signal(SIGINT, handlerPrintNewLine);
	Signal(SIGTSTP, handlerPrintNewLine);
	init_job_history();
	init_util(Getpgrp());

	while (1) {
		pid_t pid, seq_gid, gid;
		int i, file_in, file_out, tube_old[2], tube_new[2];
		char **cmd;
		bool integrated;

		/* Read command line */
		delay_new_line();
		printWelcome(SAME_LINE);
		l = readcmd();

		/* Treat command line */
		if (!l) {
			printf("exit\n");
			end_session(&l);
		}
		if (l->err) {
			printf("error: %s\n", l->err);
			continue;
		}
		if (l->in) {
			file_in = Open(l->in, O_RDONLY, 777);
			if (file_in == -1) { 
				switch (errno) {
				case EACCES:
					fprintf(stderr, "%s: Permission denied\n", l->in);
					break;
				case ENOENT:
					fprintf(stderr, "%s: No such file in directory\n", l->in);
					break;
				default:
					perror(l->in);
					break;
				}
				continue;
			}
		}
		if (l->out) {
			file_out = Open(l->out, O_CREAT | O_WRONLY, 777);
			if (file_out == -1) { 
				switch (errno) {
				case EACCES:
					fprintf(stderr, "%s: Permission denied\n", l->out);
					break;
				case ENOENT:
					fprintf(stderr, "%s: No such file in directory\n", l->out);
					break;
				default:
					perror(l->out);
					break;
				}
				continue;
			}
		}
		if (l->seq_len == 0)
			continue;
		
		nb_reaped = 0;

		/* Check for integrated command at top level */
		integrated = false;
		cmd = l->seq[0];
		if (strcmp(cmd[0], "quit") == 0) {
			end_session(&l);
			integrated = true;
		} else if (strcmp(cmd[0], "jobs") == 0) {
			print_jobs();
			integrated = true;
		} else if (strcmp(cmd[0], "fg") == 0 || strcmp(cmd[0], "bg") == 0 ||
				   strcmp(cmd[0], "stop") == 0) {
			int number = job_argument_parser(cmd[1]);
			if (number == NOT_FOUND) {
				fprintf(stderr, "%s: Missing or invalid argument\n", cmd[0]);
				continue;
			}
			gid = find_job_pid(number);
			if (strcmp(cmd[0], "stop") == 0)
				Kill(-gid, SIGSTOP);
			else {
				set_job_state(number, RUNNING);
				Kill(-gid, SIGCONT);
				if (strcmp(cmd[0], "fg") == 0) {
					shell_give_control(gid);
					wait_for_job(number);
					shell_regain_control();
				}	
			}
			integrated = true;
		}
		if (integrated) {
			if (l->seq_len != 1)
				fprintf(stderr, "warning: only unique integrated command supported\n");
			continue;
		}
	
		/* Sequence of non-integrated command */
		for (i = 0; i < l->seq_len; i++) {
			if (i > 0) {
				if (i > 1) {
					Close(tube_old[0]);
					Close(tube_old[1]);
				}
				tube_old[0] = tube_new[0];
				tube_old[1] = tube_new[1];
			}
			pipe(tube_new);

			if ((pid = Fork())) {
				if (i == 0) {
					seq_gid = pid;
					if (l->foregrounded)
						shell_give_control(seq_gid);
					else
						new_job(seq_gid, RUNNING, l->seq_len, l->seq_string);
				}
				Setpgid(pid, seq_gid);
				newTracker(pid, seq_gid);
			} else {
				if (i == 0) {
					if (l->in) {
						Dup2(file_in, 0);
						Close(file_in);
					}
				} else // i > 0
					Dup2(tube_old[0], 0);
				if (i == l->seq_len - 1) {
					if (l->out) {
						Dup2(file_out, 1);
						Close(file_out);
					}
				} else
					Dup2(tube_new[1], 1);
				if (i > 0){
					Close(tube_old[0]);
					Close(tube_old[1]);
				}
				Close(tube_new[0]);
				Close(tube_new[1]);
				cmd = l->seq[i];
				execute(cmd);
			}
		}

		if (l->seq_len > 1) {
			Close(tube_old[0]);
			Close(tube_old[1]);
		}
		Close(tube_new[0]);
		Close(tube_new[1]);

		if (l->foregrounded) {
			while (nb_reaped < l->seq_len);
			shell_regain_control();
		}
	}
}