#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include "csapp.h"

void newTracker(pid_t pid, pid_t gid);
void deleteTracker(pid_t pid);
void freeAllTracker();
pid_t findGidByPid(pid_t pid);
void deleteNodesWithGid(gid_t gid);

#endif

