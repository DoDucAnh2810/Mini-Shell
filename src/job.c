#include "job.h"

static job_t *job_history = NULL;
static int lastest_number = 0;

void init_job_history() {
    job_history = (job_t *)malloc(sizeof(job_t) * (MAX_JOB+1));
    for (int i = 0; i < MAX_JOB; i++) {
        job_t job;
        job.state = UNDEFINED;
        job_history[i] = job;
    }
    lastest_number = 0;
}

bool is_tracking(int number) {
    if (number < 1 && number > MAX_JOB)
        return false;
    switch (job_history[number].state) {
    case UNDEFINED:
        return false;
        break;
    case STOPPED:
        return true;
        break;
    case RUNNING:
        if (kill(job_history[number].pid, 0) == 0)
            return true;
        else {
            job_history[number].state = FINISHED;
            return false;
        }
        break;
    default: // FINISHED
        return false;
        break;
    }
}

void print_job(int number) {
    if (is_tracking(number)) {
        printf("\033[1;35m[%d]\033[0m \033[1;33m%d\033[0m ", number, job_history[number].pid);
        if (job_history[number].state == RUNNING)
            printf("\033[1;34mRunning\033[0m  ");
        else
            printf("\033[1;31mStopped\033[0m  ");
        printf("%s\n", job_history[number].command);
        fflush(stdout);
    }
}

void print_jobs() {
    for (int number = 1; number <= lastest_number; number++)
        print_job(number);
}

int find_job_number(pid_t pid) {
    for (int number = 1; number <= lastest_number; number++) {
        job_t job = job_history[number];
        if (is_tracking(number) && job.pid == pid)
            return number;
    }
    return NOT_FOUND;
}

pid_t find_job_pid(int number) {
    if (1 <= number && number <= MAX_JOB && is_tracking(number))
        return job_history[number].pid;
    else
        return NOT_FOUND;
}

void set_job_state(int number, short state) {
    job_history[number].state = state;
    print_job(number);
}


void new_job(pid_t pid, short state, char ***seq) {
    if (lastest_number == MAX_JOB) {
        fprintf(stderr, "job - new_job(): Out of memory for more jobs.\n");
        exit(1);
    }
    lastest_number++;
    job_t job;
    job.pid = pid;
    job.state = state;
    job.command = (char *)malloc(MAX_COMMAND+1);
    int increase, offset = 0;
    for (int i = 0; seq[i] != NULL; i++) {
        for (int j = 0; seq[i][j] != NULL; j++) {
            increase = strlen(seq[i][j]) + 1;
            if (offset + increase > MAX_COMMAND) {
                job_history[lastest_number] = job;
                print_job(lastest_number);
                return;
            }
            sprintf(job.command + offset, "%s ", seq[i][j]);
            offset += increase;
        }
        if (seq[i+1] != NULL) {
            increase = 2;
            if (offset + increase > MAX_COMMAND) {
                job_history[lastest_number] = job;
                print_job(lastest_number);
                return;
            }
            sprintf(job.command + offset, "| ");
            offset += increase;
        }
    }
    job_history[lastest_number] = job;
    print_job(lastest_number);
}

int job_argument_parser(char *str) {
    if (str == NULL)
        return NOT_FOUND;
    int number;
    char *endptr;
    pid_t pid;
    if (str[0] == '%') {
        number = strtol(str + 1, &endptr, 0);
        if (*endptr != '\0')
            return NOT_FOUND;
    } else {
        pid = strtol(str, &endptr, 0);
        if (*endptr != '\0')
            return NOT_FOUND;
        number = find_job_number(pid);
    }
    return number;
}

