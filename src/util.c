#include "util.h"
#include "gid_tracker.h"
#include "job.h"
#include "readcmd.h"

static pid_t shell_gid;
static sigset_t shell_background_ignore_set;
static struct timespec next_line_delay;

bool strings_equal(char *s1, char *s2) {
	return strcmp(s1, s2) == 0;
}

bool integrated_command(char *str) {
	return strings_equal(str, "fg") || strings_equal(str, "bg") || strings_equal(str, "stop") || 
			strings_equal(str, "jobs") || strings_equal(str, "quit");
}

void init_util(pid_t gid) {
    shell_gid = gid;
    next_line_delay.tv_sec = 0;
    next_line_delay.tv_nsec = 10000000;
    Sigemptyset(&shell_background_ignore_set);
    Sigaddset(&shell_background_ignore_set, SIGTTOU);
    Sigaddset(&shell_background_ignore_set, SIGTTIN);
}

void delay_new_line() {
    nanosleep(&next_line_delay, NULL);
}

void print_welcome(bool newLine) {
	if (newLine) printf("\n");
	printf("\033[1;32mshell>\033[0m ");
	fflush(stdout);
}

void shell_give_control(pid_t gid) {
	Sigprocmask(SIG_BLOCK, &shell_background_ignore_set, NULL);
	tcsetpgrp(STDIN_FILENO,  gid);
	tcsetpgrp(STDOUT_FILENO, gid);
	tcsetpgrp(STDERR_FILENO, gid);
}

void shell_regain_control() {
	tcsetpgrp(STDIN_FILENO, shell_gid);
	tcsetpgrp(STDOUT_FILENO, shell_gid);
	tcsetpgrp(STDERR_FILENO, shell_gid);
	Sigprocmask(SIG_UNBLOCK, &shell_background_ignore_set, NULL);
}

void error_hander(char *pathname, bool isFile) {
	switch (errno) {
	case EACCES:
		fprintf(stderr, "%s: Permission denied\n", pathname);
		break;
	case ENOENT:
		if (isFile)
			fprintf(stderr, "%s: No such file in directory\n", pathname);
		else
			fprintf(stderr, "%s: Command not found\n", pathname);
		break;
	default:
		perror(pathname);
		break;
	}
}

void execute(char **cmd) {
	if (integrated_command(cmd[0])) {
		fprintf(stderr, "%s: Integrated command not defined in pipe\n", cmd[0]);
		exit(1);
	} else {
		if (execvp(cmd[0], cmd) == -1) {
			error_hander(cmd[0], COMMAND);
			exit(1);
		}
		exit(0);
	}
}

void end_session(struct cmdline **l) {
	kill_all_job();
	free_all_tracker();
	destroy_job_history();
	if (*l) {
		if ((*l)->in) free((*l)->in);
		if ((*l)->out) free((*l)->out);
		if ((*l)->seq) {
			for (int i = 0; (*l)->seq[i]!=0; i++) {
				char **cmd = (*l)->seq[i];
				for (int j = 0; cmd[j]!=0; j++) 
					free(cmd[j]);
				free(cmd);
			}
			free((*l)->seq);
		}
	}
	free(*l);
	exit(0);
}