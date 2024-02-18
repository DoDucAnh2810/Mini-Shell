/*
 * Copyright (C) 2002, Simon Nieuviarts
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "readcmd.h"
#include "csapp.h"
#include "job.h"

static pid_t shell_pid;
static pid_t foreground_pid;
static pid_t waiting_pid;
static bool waiting_SIGCHLD;
static bool executing_handlerSIGCHLD;
static struct cmdline *l;

void handlerSIGCHLD() {
	executing_handlerSIGCHLD = true;
	int number, status;
	pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		if (pid == waiting_pid)
			waiting_SIGCHLD = false;
		number = find_job_number(pid);
		if (WIFEXITED(status)) {
			if (number != NOT_FOUND)
				set_job_state(number, FINISHED);
		} else if(WIFSIGNALED(status)) {
			if (number != NOT_FOUND)
				set_job_state(number, TERMINATED);
		} else if (WIFSTOPPED(status)) {
			if (number == NOT_FOUND)
				new_job(pid, STOPPED, l->seq);
			else
				set_job_state(number, STOPPED);
		} else if (WIFCONTINUED(status))
			set_job_state(number, RUNNING);		
	}
	executing_handlerSIGCHLD = false;
}

void handlerSIGQUIT() {
	exit(0);
}

void handlerSIGINT() {
	fprintf(stderr, "\n"); fflush(stderr);
	if (foreground_pid == shell_pid) {
		fprintf(stderr, "\033[1;32mshell>\033[0m "); 
		fflush(stderr);
	} else {
		Kill(-foreground_pid, SIGINT);
		foreground_pid = shell_pid;
		while(executing_handlerSIGCHLD);
	}
}

void handlerSIGTSTP() {
	fprintf(stderr, "\n"); fflush(stderr);
	if (foreground_pid == shell_pid) {
		fprintf(stderr, "\033[1;32mshell>\033[0m ");
		fflush(stderr);
	} else {
		Kill(-foreground_pid, SIGTSTP);
		foreground_pid = shell_pid;
		while(executing_handlerSIGCHLD);
	}
}

void execute(char **cmd) {
	if (strcmp(cmd[0], "fg") == 0 ||
		strcmp(cmd[0], "bg") == 0 ||
		strcmp(cmd[0], "stop") == 0 ||
		strcmp(cmd[0], "jobs") == 0 ||
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

void wait_for(pid_t pid) {
	waiting_pid = pid;
	waiting_SIGCHLD = true;
	while (waiting_SIGCHLD);
}

void foreground(pid_t pid) {
	foreground_pid = pid;
	wait_for(pid);
	foreground_pid = shell_pid;
}

int main(int argc, char **argv) {
	Signal(SIGCHLD, handlerSIGCHLD);
	Signal(SIGQUIT, handlerSIGQUIT);
	Signal(SIGINT, handlerSIGINT);
	Signal(SIGTSTP, handlerSIGTSTP);
	
	init_job_history();
	shell_pid = foreground_pid = getpid();

	while (1) {
		int i, j, file_in, file_out, tube[2], seq_len, start;
		char **cmd;
		pid_t pid, pid_seq, pid_next_comm, pid_exec;
		bool foregrounded;

		/* Read command line */
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
		if (l->in){
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
		
		/* Check for integrated job command at top level */
		start = 0;
		cmd = l->seq[start];
		if (strcmp(cmd[0], "quit") == 0)
			exit(0);
		else if (strcmp(cmd[0], "jobs") == 0) {
			print_jobs();
			start++;
		} else if (strcmp(cmd[0], "fg") == 0 ||
				   strcmp(cmd[0], "bg") == 0 ||
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
			dup2(file_in, 0);
			for (i = start; i < seq_len; i++) {
				cmd = l->seq[i];
				if (i == seq_len - 1) {
					if (l->out) 
						dup2(file_out, 1);
					execute(cmd);
				} else {
					pipe(tube);
					if ((pid_next_comm = Fork())) { // command at the head
						Close(tube[0]);
						if ((pid_exec = Fork())) { // just wait
							Close(tube[1]);
							wait_for(pid_exec);
							wait_for(pid_next_comm);
							exit(0);
						} else { // excute the commmand
							dup2(tube[1], 1);
							execute(cmd);
						}
					} else { // the rest of the sequence
						Close(tube[1]);
						dup2(tube[0], 0);
					}
				}
			}
		}
	}
}