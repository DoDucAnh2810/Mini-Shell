/*
 * Copyright (C) 2002, Simon Nieuviarts
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "readcmd.h"
#include "csapp.h"
#include "job.h"

static int nb_reaped;
static struct cmdline *l;

void handlerSIGCHLD() {
	int number, status;
	pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
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
				new_job(pid, STOPPED, l->seq);
			else
				set_job_state(number, STOPPED);
		} else if (WIFCONTINUED(status))
			set_job_state(number, RUNNING);
		nb_reaped++;
	}
}

void handlerPrintNewLine() {
	printf("\n\033[1;32mshell>\033[0m ");
	fflush(stdout);
}

void execute(char **cmd) {
	if (strcmp(cmd[0], "fg") == 0 || strcmp(cmd[0], "bg") == 0 ||
		strcmp(cmd[0], "stop") == 0 || strcmp(cmd[0], "jobs") == 0 ||
		strcmp(cmd[0], "quit") == 0) {
		fprintf(stderr, "Integrated command not defined in pipe\n");
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
	tcsetpgrp(STDERR_FILENO, pid);
	while (nb_reaped < 1);
	sigset_t set;
	Sigemptyset(&set);
	Sigaddset(&set, SIGTTOU);
	Sigprocmask(SIG_BLOCK, &set, NULL);
	tcsetpgrp(STDERR_FILENO, Getpgrp());
	Sigprocmask(SIG_UNBLOCK, &set, NULL);
}

int main(int argc, char **argv) {
	Signal(SIGCHLD, handlerSIGCHLD);
	Signal(SIGINT, handlerPrintNewLine);
	Signal(SIGTSTP, handlerPrintNewLine);
	init_job_history();
	struct timespec next_line_delay;
	next_line_delay.tv_sec = 0;
	next_line_delay.tv_nsec = 10000000;

	while (1) {
		int i, j, file_in, file_out, tube[2], seq_len, start;
		char **cmd;
		pid_t pid, pid_seq, pid_next_comm, pid_exec;
		bool foregrounded;
		nb_reaped = 0;

		/* Read command line */
		nanosleep(&next_line_delay, NULL);
		printf("\033[1;32mshell>\033[0m ");
		l = readcmd();

		/* Treat command line */
		if (!l) {
			printf("\nexit\n");
			exit(0);
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

		/* Compute sequence length */
		seq_len = 0;
		while (l->seq[seq_len] != NULL)
			seq_len++;
		if (seq_len == 0)
			continue;
		
		/* Check for integrated command at top level */
		start = 0;
		cmd = l->seq[start];
		if (strcmp(cmd[0], "quit") == 0)
			exit(0);
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
				Kill(-pid, SIGCONT);
				if (strcmp(cmd[0], "fg") == 0)
					foreground(pid);
			}
			start++;
		}
		if (start == seq_len)
			continue;

		/* Check for background character '&' */
		foregrounded = true;
		for (j = start; l->seq[seq_len-1][j] != NULL; j++)
			if (strcmp(l->seq[seq_len-1][j], "&") == 0) {
				foregrounded = false;
				l->seq[seq_len-1][j] = NULL;
			}

		/* Sequence of non-integrated command */
		if ((pid_seq = Fork())) { // shell
			Setpgid(pid_seq, pid_seq);
			if (foregrounded)
				foreground(pid_seq);
			else
				new_job(pid_seq, RUNNING, l->seq);
		} else { // sequence execution
			Dup2(file_in, 0);
			for (i = start; i < seq_len; i++) {
				cmd = l->seq[i];
				if (i == seq_len - 1) {
					if (l->out)
						Dup2(file_out, 1);
					execute(cmd);
				} else {
					pipe(tube);
					if ((pid_next_comm = Fork())) { // command at the head
						Close(tube[0]);
						if ((pid_exec = Fork())) { // just wait
							Close(tube[1]);
							while (nb_reaped < 2);
							exit(0);
						} else { // excute the commmand
							Dup2(tube[1], 1);
							execute(cmd);
						}
					} else { // the rest of the sequence
						Close(tube[1]);
						Dup2(tube[0], 0);
					}
				}
			}
		}
	}
}