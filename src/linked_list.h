#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include "csapp.h"

typedef struct Node {
    pid_t pid;
    pid_t gid;
    struct Node* next;
} Node;

Node* createNode(pid_t pid, gid_t gid);
void insertAtBeginning(Node** head, pid_t pid, pid_t gid);
void insertAtEnd(Node** head, pid_t pid, pid_t gid);
void deleteNode(Node** head, pid_t pid);
void printList(Node* head);
void freeList(Node** head);
Node* initializeList();
pid_t findGidByPid(Node* head, pid_t pid);
void deleteNodesWithGid(Node** head, gid_t gid);

#endif

