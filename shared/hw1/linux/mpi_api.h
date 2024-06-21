#ifndef MPI_API_H
#define MPI_API_H

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

static inline int mpi_register(void) {
    int res;
    __asm__ (
        "pushl %%eax;\n"
        "movl $243, %%eax;\n"  // System call number for mpi_register
        "int $0x80;\n"
        "popl %%eax;\n"
        "movl %%eax, %0;"
        : "=r" (res)
        :
        : "eax"
    );
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return 0;
}

static inline int mpi_send(pid_t pid, char *message, ssize_t message_size) {
    int res;
    __asm__ (
        "pushl %%eax;\n"
        "pushl %%ebx;\n"
        "pushl %%ecx;\n"
        "pushl %%edx;\n"
        "movl $244, %%eax;\n"  // System call number for mpi_send
        "movl %1, %%ebx;\n"
        "movl %2, %%ecx;\n"
        "movl %3, %%edx;\n"
        "int $0x80;\n"
        "popl %%edx;\n"
        "popl %%ecx;\n"
        "popl %%ebx;\n"
        "popl %%eax;\n"
        "movl %%eax, %0;"
        : "=r" (res)
        : "r" (pid), "r" (message), "r" (message_size)
        : "eax", "ebx", "ecx", "edx"
    );
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return 0;
}

static inline int mpi_receive(pid_t pid, char *message, ssize_t message_size) {
    int res;
    __asm__ (
        "pushl %%eax;\n"
        "pushl %%ebx;\n"
        "pushl %%ecx;\n"
        "pushl %%edx;\n"
        "movl $245, %%eax;\n"  // System call number for mpi_receive
        "movl %1, %%ebx;\n"
        "movl %2, %%ecx;\n"
        "movl %3, %%edx;\n"
        "int $0x80;\n"
        "popl %%edx;\n"
        "popl %%ecx;\n"
        "popl %%ebx;\n"
        "popl %%eax;\n"
        "movl %%eax, %0;"
        : "=r" (res)
        : "r" (pid), "r" (message), "r" (message_size)
        : "eax", "ebx", "ecx", "edx"
    );
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

#endif // MPI_API_H
