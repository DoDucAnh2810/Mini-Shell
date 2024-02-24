/* gid_tracker - Tracking group id of processes */
#ifndef GID_TRACKER_H
#define GID_TRACKER_H

#include "csapp.h"

/* Add a new gid tracker to the record */
void new_tracker(pid_t pid, pid_t gid);

/* Delete the tracker of pid */
void delete_tracker(pid_t pid);

/* Free the record of all tracker */
void free_all_tracker();

/* Return the group id of pid */
pid_t find_gid(pid_t pid);

/* Free all nodes corresponding to gid */
void delete_nodes_gid(gid_t gid);

#endif

