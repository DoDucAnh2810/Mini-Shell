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

static int nb_reaped;
static struct cmdline *l;
static sigset_t set;
static pid_t foreground_gid;
static pid_t shell_gid;


void printWelcome(bool newLine) {
	if (newLine)
		printf("\n");
	printf("\033[1;32mshell>\033[0m ");
	fflush(stdout);
}

void shell_give_control(pid_t gid) {
	Sigprocmask(SIG_BLOCK, &set, NULL);
	tcsetpgrp(STDIN_FILENO,  gid);
	tcsetpgrp(STDOUT_FILENO, gid);
	tcsetpgrp(STDERR_FILENO, gid);
	foreground_gid = gid;
}

void shell_regain_control() {
	tcsetpgrp(STDIN_FILENO, Getpgrp());
	tcsetpgrp(STDOUT_FILENO, Getpgrp());
	tcsetpgrp(STDERR_FILENO, Getpgrp());
	Sigprocmask(SIG_UNBLOCK, &set, NULL);
	foreground_gid = shell_gid;
}

void handlerSIGCHLD() {
	int number, status;
	pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		number = find_job_number(pid);
		if (WIFEXITED(status)) {
			if (number != NOT_FOUND)
				set_job_state(number, FINISHED);
		} else if(WIFSIGNALED(status)) {
			if (WTERMSIG(status) == SIGINT) {
				printf("\n"); fflush(stdout);
			}
			if (number != NOT_FOUND)
				set_job_state(number, TERMINATED);
		} else if (WIFSTOPPED(status)) {
			if (WSTOPSIG(status) == SIGTSTP) {
				printf("\n"); fflush(stdout);
			}
			if (number == NOT_FOUND)
				new_job(pid, STOPPED, l->seq_string);
			else
				set_job_state(number, STOPPED);
		}
		nb_reaped++;
	}
}

void handlerPrintNewLine() {
	printWelcome(true);
}

void execute(char **cmd) {
	if (strcmp(cmd[0], "fg") == 0 || strcmp(cmd[0], "bg") == 0 ||
		strcmp(cmd[0], "stop") == 0 || strcmp(cmd[0], "jobs") == 0 ||
		strcmp(cmd[0], "quit") == 0) {
		fprintf(stderr, "%s: Integrated command not defined in pipe\n", cmd[0]);
		exit(1);
	} else {
		if (execvp(cmd[0], cmd) == -1) {
			switch (errno) {
			case EACCES:
				fprintf(stderr, "%s: Permission denied\n", cmd[0]);
				break;
			case ENOENT:
				fprintf(stderr, "%s: Command not found\n", cmd[0]);
				break;
			default:
				perror(cmd[0]);
				break;
			}
			exit(1);
		}
		exit(0);
	}
}

void foreground(pid_t pid) {
	sigset_t set;
	Sigemptyset(&set);
	Sigaddset(&set, SIGTTOU);
	Sigprocmask(SIG_BLOCK, &set, NULL);
	tcsetpgrp(STDIN_FILENO, pid);
	tcsetpgrp(STDOUT_FILENO, pid);
	tcsetpgrp(STDERR_FILENO, pid);
	Kill(-pid, SIGCONT);
	while (nb_reaped < 1);
	tcsetpgrp(STDIN_FILENO, Getpgrp());
	tcsetpgrp(STDOUT_FILENO, Getpgrp());
	tcsetpgrp(STDERR_FILENO, Getpgrp());
	Sigprocmask(SIG_UNBLOCK, &set, NULL);
}

void end_session() {
	kill_all_job();
	while (nb_reaped < job_count());
	destroy_job_history();
	if (l) {
		if (l->in) free(l->in);
		if (l->out) free(l->out);
		if (l->seq) {
			for (int i = 0; l->seq[i]!=0; i++) {
				char **cmd = l->seq[i];
				for (int j = 0; cmd[j]!=0; j++) 
					free(cmd[j]);
				free(cmd);
			}
			free(l->seq);
		}
	}
	free(l);
	exit(0);
}

int main(int argc, char **argv) {
	Signal(SIGCHLD, handlerSIGCHLD);
	Signal(SIGINT, handlerPrintNewLine);
	Signal(SIGTSTP, handlerPrintNewLine);
	init_job_history();
	struct timespec next_line_delay;
	next_line_delay.tv_sec = 0;
	next_line_delay.tv_nsec = 10000000;
	foreground_gid = shell_gid = Getpgrp();
	Sigemptyset(&set);
	Sigaddset(&set, SIGTTOU);
	Sigaddset(&set, SIGTTIN);

	while (1) {
		pid_t pid, seq_gid;
		int i, file_in, file_out, tube_old[2], tube_new[2], start;
		char **cmd;

		/* Read command line */
		nanosleep(&next_line_delay, NULL);
		printWelcome(false);
		l = readcmd();

		/* Treat command line */
		if (!l) {
			printf("exit\n");
			end_session();
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
		
		/* Check for integrated command at top level */
		nb_reaped = 0;
		start = 0;
		cmd = l->seq[start];
		if (strcmp(cmd[0], "quit") == 0)
			end_session();
		else if (strcmp(cmd[0], "jobs") == 0) {
			print_jobs();
			start++;
		} else if (strcmp(cmd[0], "fg") == 0 || strcmp(cmd[0], "bg") == 0 ||
				   strcmp(cmd[0], "stop") == 0) {
			int number = job_argument_parser(cmd[1]);
			if (number == NOT_FOUND) {
				fprintf(stderr, "%s: Missing or invalid argument\n", cmd[0]);
				continue;
			}
			pid = find_job_pid(number);
			if (strcmp(cmd[0], "stop") == 0)
				Kill(-pid, SIGSTOP);
			else {
				set_job_state(number, RUNNING);
				if (strcmp(cmd[0], "fg") == 0)
					foreground(pid);
				else
					Kill(-pid, SIGCONT);
			}

			start++;
		}
		if (start == l->seq_len)
			continue;
	
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
						new_job(seq_gid, RUNNING, l->seq_string);
				}
				Setpgid(pid, seq_gid);
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