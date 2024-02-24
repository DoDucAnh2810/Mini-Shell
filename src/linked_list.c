#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    pid_t pid;
    pid_t gid;
    struct Node* next;
} Node;

Node *head = NULL;

Node* createNode(pid_t pid, gid_t gid) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode != NULL) {
        newNode->pid = pid;
        newNode->gid = gid;
        newNode->next = NULL;
    }
    return newNode;
}

void newTracker(pid_t pid, pid_t gid) {
    Node* newNode = createNode(pid, gid);
    if (newNode != NULL) {
        newNode->next = head;
        head = newNode;
    }
}

void deleteTracker(pid_t pid) {
    if (head == NULL) {
        return;
    }

    if (head->pid == pid) {
        Node* temp = head;
        head = head->next;
        free(temp);
        return;
    }

    Node* current = head;
    while (current->next != NULL && current->next->pid != pid) {
        current = current->next;
    }

    if (current->next != NULL) {
        Node* temp = current->next;
        current->next = current->next->next;
        free(temp);
    }
}

void freeAllTracker() {
    while (head != NULL) {
        Node* temp = head;
        head = head->next;
        free(temp);
    }
}

pid_t findGidByPid(pid_t pid) {
    Node* current = head;
    while (current != NULL) {
        if (current->pid == pid)
            return current->gid;
        current = current->next;
    }
    // Return a value to indicate that the pid was not found (you can choose an appropriate value)
    return (pid_t)-1;
}

void deleteNodesWithGid(gid_t gid) {
    if (head == NULL)
        return; // Empty list, nothing to delete

    // Handle the case where the node to be deleted is at the beginning of the list
    while (head != NULL && head->gid == gid) {
        Node* temp = head;
        head = head->next;
        free(temp);
    }

    // Traverse the list and delete nodes with the given gid
    Node* current = head;
    while (current != NULL && current->next != NULL) {
        if (current->next->gid == gid) {
            Node* temp = current->next;
            current->next = current->next->next;
            free(temp);
        } else {
            current = current->next;
        }
    }
}

