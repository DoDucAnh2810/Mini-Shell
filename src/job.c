#include "job.h"
#include "assert.h"

static job_t *job_history = NULL;
static int lastest_number = 0;

void init_job_history() {
    job_history = (job_t *)malloc(sizeof(job_t) * (MAX_JOB+1));
    if (job_history == NULL) {
        fprintf(stderr, "job: Failed to initialize job history\n");
        exit(1);
    }
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
    short state = job_history[number].state;
    return state == STOPPED || state == RUNNING;
}

void print_job(int number) {
    short state = job_history[number].state;
    assert(state != UNDEFINED);
    if (state == FINISHED) return;
    printf("\033[1;35m[%d]\033[0m \033[1;33m%d\033[0m ", 
            number, job_history[number].pid);
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
    for (int number = 1; number <= lastest_number; number++)
        if (is_tracking(number))
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
    if (is_tracking(number))
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
        fprintf(stderr, "job: Out of memory for more jobs.\n");
        exit(1);
    }
    lastest_number++;
    job_t job;
    job.pid = pid;
    job.state = state;
    job.command = (char *)malloc(MAX_COMMAND+1);
    if (job.command == 0) {
        fprintf(stderr, "job: Out of memory for more jobs.\n");
        exit(1);
    }
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