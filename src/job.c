#include <stdbool.h>
#include "job.h"

typedef struct {
	pid_t gid;
    short state;
	int nb_running;
	int nb_exist;
	char *command;
} job_t;

static job_t *job_history = NULL;
static int highest_number = 0;

void init_job_history() {
    job_history = (job_t *)malloc(sizeof(job_t) * (MAX_JOB+1));
    if (job_history == NULL) {
        fprintf(stderr, "job: Failed to initialize job history\n");
        exit(1);
    }
    for (int i = 0; i < MAX_JOB; i++) {
        job_t job;
        job.state = UNDEFINED;
        job.command = NULL;
        job_history[i] = job;
    }
    highest_number = 0;
}

bool is_tracking(int number) {
    if (number < 1 && number > MAX_JOB)
        return false;
    short state = job_history[number].state;
    return state == STOPPED || state == RUNNING;
}

void print_job(int number) {
    short state = job_history[number].state;
    if (state == FINISHED) return;
    printf("\033[1;35m[%d]\033[0m \033[1;33m%d\033[0m ", 
            number, job_history[number].gid);
    switch (state) {
    case STOPPED:
        printf("\033[1;38;5;208mStopped   \033[0m ");
        break;
    case RUNNING:
        printf("\033[1;34mRunning   \033[0m ");
        break;
    case TERMINATED:
        printf("\033[1;31mTerminated\033[0m ");
        break;
    default: // Can not happen
        break;
    }
    printf("%s\n", job_history[number].command);
    fflush(stdout);
}

void print_jobs() {
    for (int number = 1; number <= highest_number; number++)
        if (is_tracking(number))
            print_job(number);
}

int find_job_number(pid_t gid) {
    for (int number = 1; number <= highest_number; number++) {
        job_t job = job_history[number];
        if (is_tracking(number) && job.gid == gid)
            return number;
    }
    return NOT_FOUND;
}

pid_t find_job_pid(int number) {
    if (is_tracking(number))
        return job_history[number].gid;
    else
        return NOT_FOUND;
}

void set_job_state(int number, short state) {
    job_history[number].state = state;
    if (state == RUNNING)
        job_history[number].nb_running = job_history[number].nb_exist;
    print_job(number);
    if (!is_tracking(highest_number)) {
        if (job_history[highest_number].command) {
            free(job_history[highest_number].command);
            job_history[highest_number].command = NULL;
        }
        highest_number--;
    }
}

void new_job(pid_t gid, short state, int nb_command, char *seq_string) {
    if (highest_number == MAX_JOB) {
        fprintf(stderr, "job: Out of memory for more jobs.\n");
        exit(1);
    }
    int number = NOT_FOUND;
    for (int i = 1; i <= highest_number; i++)
        if (!is_tracking(i)) {
            if (job_history[i].command)
                free(job_history[i].command);
            number = i;
            break;
        }
    if (number == NOT_FOUND)
        number = ++highest_number;
    job_t job;
    job.gid = gid;
    job.state = state;
    if (state == RUNNING)
        job.nb_running = nb_command;
    else
        job.nb_running = 0;
    job.nb_exist = nb_command;
    job.command = seq_string;
    job_history[number] = job;
    print_job(number);
}

int job_argument_parser(char *str) {
    if (str == NULL)
        return NOT_FOUND;
    int number;
    char *endptr;
    pid_t gid;
    if (str[0] == '%') {
        number = strtol(str + 1, &endptr, 0);
        if (!is_tracking(number) || *endptr != '\0')
            return NOT_FOUND;
    } else {
        gid = strtol(str, &endptr, 0);
        if (*endptr != '\0')
            return NOT_FOUND;
        number = find_job_number(gid);
    }
    return number;
}

void wait_for_job(int number) {
    while (job_history[number].nb_running > 0);
}

void wait_for_job_termination(int number) {
    while (job_history[number].nb_exist > 0);
}

void kill_all_job() {
    for (int number = 1; number <= highest_number; number++) {
        job_t job = job_history[number];
        if (is_tracking(number))
            Kill(-job.gid, SIGKILL);
        wait_for_job_termination(number);
    }
}

int job_count() {
    return highest_number;
}

void destroy_job_history() {
    for (int number = 1; number <= MAX_JOB; number++) {
        job_t job = job_history[number];
        if (job.command)
            free(job.command);
    }
    free(job_history);
}

void decrement_nb_exist(int number, short state) {
    job_history[number].nb_running--;
    if (--job_history[number].nb_exist == 0)
        set_job_state(number, state);
}

void decrement_nb_running(int number) {
    if (--job_history[number].nb_running == 0)
        set_job_state(number, STOPPED);
}

