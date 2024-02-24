#ifndef GID_TRACKER_H
#define GID_TRACKER_H

#include "csapp.h"

void new_tracker(pid_t pid, pid_t gid);
void delete_tracker(pid_t pid);
void free_all_tracker();
pid_t find_gid(pid_t pid);
void delete_nodes_gid(gid_t gid);

#endif

