#ifndef _MPI_H
#define _MPI_H  

#include <linux/slab.h>
#include <linux/list.h>



struct pid_queue{
    list_t messages;
    pid_t sender_pid;
    list_t l_idx;
};

struct message_node{
    char *message;
    ssize_t message_size;
    list_t l_idx;
};

struct mpi_poll_entry {
    pid_t pid;
    char incoming;
};

#endif
