/*
 * Copyright (C) 2002, Simon Nieuviarts
 */
#include <stdio.h>
#include "gid_tracker.h"
#include "readcmd.h"
#include "csapp.h"
#include "util.h"
#include "job.h"

static int nb_reaped;
static struct cmdline *l;

void handler_SIGCHLD() {
	int number, status;
	pid_t pid, gid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		gid = find_gid(pid);
		number = find_job_number(gid);
		if (WIFEXITED(status)) {
			if (number != NOT_FOUND)
				decrement_nb_exist(number, FINISHED);
			else
				delete_tracker(pid);
		} else if(WIFSIGNALED(status)) {
			if (WTERMSIG(status) == SIGINT)  { printf("\n"); fflush(stdout);}
			if (number != NOT_FOUND)
				decrement_nb_exist(number, TERMINATED);
			else
				delete_tracker(pid);
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

void handler_new_line() {
	print_welcome(NEW_LINE);
}

int main(int argc, char **argv) {
	int i, file_in, file_out, tube_old[2], tube_new[2];
	pid_t pid, gid;
	char **cmd;

	Signal(SIGCHLD, handler_SIGCHLD);
	Signal(SIGINT, handler_new_line);
	Signal(SIGTSTP, handler_new_line);
	init_job_history();
	init_util(Getpgrp());

	while (1) {
		/* Read command line */
		delay_new_line();
		print_welcome(SAME_LINE);
		l = readcmd();
		nb_reaped = 0;

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
			file_in = Open(l->in, O_RDONLY, 0644);
			if (file_in == -1) {
				error_hander(l->in, FILE);
				continue;
			}
		}
		if (l->out) {
			file_out = Open(l->out, O_CREAT | O_WRONLY, 0644);
			if (file_out == -1) {
				error_hander(l->out, FILE);
				continue;
			}
		}
		if (l->seq_len == 0)
			continue;

		/* Check for integrated command at top level */
		cmd = l->seq[0];
		if (integrated_command(cmd[0])) {
			if (strings_equal(cmd[0], "quit"))
				end_session(&l);
			else if (strings_equal(cmd[0], "jobs"))
				print_jobs();
			else if (strings_equal(cmd[0], "fg") || strings_equal(cmd[0], "bg")||
					strings_equal(cmd[0], "stop")) {
				int number = job_argument_parser(cmd[1]);
				if (number == NOT_FOUND) {
					fprintf(stderr, "%s: Missing or invalid argument\n", cmd[0]);
					continue;
				}
				gid = find_job_gid(number);
				if (strings_equal(cmd[0], "stop"))
					Kill(-gid, SIGSTOP);
				else {
					set_job_state(number, RUNNING);
					Kill(-gid, SIGCONT);
					if (strings_equal(cmd[0], "fg")) {
						shell_give_control(gid);
						wait_for_job(number);
						shell_regain_control();
					}	
				}
			}
			if (l->seq_len != 1)
				fprintf(stderr, "warning: only unique integrated command supported\n");
			continue;
		}
	
		/* Sequence of non-integrated commands */
		for (i = 0; i < l->seq_len; i++) {
			// update tubes
			if (i > 0) {
				if (i > 1) close_pipe(tube_old);
				tube_old[0] = tube_new[0];
				tube_old[1] = tube_new[1];
			}
			pipe(tube_new);

			if ((pid = Fork())) { // shell
				if (i == 0) { // first iteration
					gid = pid;
					if (l->foregrounded)
						shell_give_control(gid);
					else
						new_job(gid, RUNNING, l->seq_len, l->seq_string);
				}
				Setpgid(pid, gid);
				new_tracker(pid, gid);
			} else { // children
				// redirecting input
				if (i == 0) {
					if (l->in) {
						Dup2(file_in, 0);
						Close(file_in);
					}
				} else
					Dup2(tube_old[0], 0);
				// redirecting output
				if (i == l->seq_len - 1) {
					if (l->out) {
						Dup2(file_out, 1);
						Close(file_out);
					}
				} else
					Dup2(tube_new[1], 1);
				// Close all tubes
				if (i > 0) close_pipe(tube_old);
				close_pipe(tube_new);
				// Execution
				cmd = l->seq[i];
				execute(cmd);
			}
		}
		// Close all tubes
		if (l->seq_len > 1) close_pipe(tube_old);
		close_pipe(tube_new);
		// Wait if necessary
		if (l->foregrounded) {
			while (nb_reaped < l->seq_len);
			shell_regain_control();
		}
	}
}