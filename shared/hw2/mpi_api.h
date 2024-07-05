#ifndef _MPI_API_H
#define _MPI_API_H  

#include <linux/errno.h>

// Mpi poll struct, contains process' pid and indication of incoming message (246)
struct mpi_poll_entry {
	pid_t pid;
	char incoming;
};


// Wrapper function for the MPI register syscall
int mpi_register(void)
{
    int res;
    __asm__ (
        "pushl %%eax;"           // Save the current value of EAX
        "movl $243, %%eax;"      // Load syscall number 243 (mpi_register) into EAX
        "int $0x80;"             // Trigger the syscall interrupt
        "movl %%eax, %0;"        // Move the result from EAX to the variable 'res'
        "popl %%eax;"            // Restore the original value of EAX
        : "=m" (res)             // Output: res holds the result of the syscall
    );
    
    if (res >= (unsigned long)(-125)) { // Check if there was an error
        errno = -res;              // Set errno to the negative value of the result
        res = -1;                  // Return -1 to indicate an error
    }
    return (int)res;                // Return the result of the syscall
}

// Wrapper function for the MPI send syscall
int mpi_send(pid_t pid, char *message, ssize_t message_size)
{
    int res;
    __asm__ (
        "pushl %%eax;"            // Save the current value of EAX
        "pushl %%ebx;"            // Save the current value of EBX
        "pushl %%ecx;"            // Save the current value of ECX
        "pushl %%edx;"            // Save the current value of EDX
        "movl $244, %%eax;"       // Load syscall number 244 (mpi_send) into EAX
        "movl %1, %%ebx;"         // Load the first argument (pid) into EBX
        "movl %2, %%ecx;"         // Load the second argument (message) into ECX
        "movl %3, %%edx;"         // Load the third argument (message_size) into EDX
        "int $0x80;"              // Trigger the syscall interrupt
        "movl %%eax, %0;"         // Move the result from EAX to the variable 'res'
        "popl %%edx;"             // Restore the original value of EDX
        "popl %%ecx;"             // Restore the original value of ECX
        "popl %%ebx;"             // Restore the original value of EBX
        "popl %%eax;"             // Restore the original value of EAX
        : "=m" (res)              // Output: res holds the result of the syscall
        : "m" (pid), "m" (message), "m"(message_size) // Inputs: pid, message, and message_size
    );
    
    if (res >= (unsigned long)(-125)) { // Check if there was an error
        errno = -res;              // Set errno to the negative value of the result
        res = -1;                  // Return -1 to indicate an error
    }
    return (int)res;                // Return the result of the syscall
}

// Wrapper function for the MPI receive syscall
int mpi_receive(pid_t pid, char* message, ssize_t message_size)
{
    int res;
    __asm__ (
        "pushl %%eax;"            // Save the current value of EAX
        "pushl %%ebx;"            // Save the current value of EBX
        "pushl %%ecx;"            // Save the current value of ECX
        "pushl %%edx;"            // Save the current value of EDX
        "movl $245, %%eax;"       // Load syscall number 245 (mpi_receive) into EAX
        "movl %1, %%ebx;"         // Load the first argument (pid) into EBX
        "movl %2, %%ecx;"         // Load the second argument (message) into ECX
        "movl %3, %%edx;"         // Load the third argument (message_size) into EDX
        "int $0x80;"              // Trigger the syscall interrupt
        "movl %%eax, %0;"         // Move the result from EAX to the variable 'res'
        "popl %%edx;"             // Restore the original value of EDX
        "popl %%ecx;"             // Restore the original value of ECX
        "popl %%ebx;"             // Restore the original value of EBX
        "popl %%eax;"             // Restore the original value of EAX
        : "=m" (res)              // Output: res holds the result of the syscall
        : "m" (pid), "m" (message), "m"(message_size) // Inputs: pid, message, and message_size
    );
    
    if (res >= (unsigned long)(-125)) { // Check if there was an error
        errno = -res;              // Set errno to the negative value of the result
        res = -1;                  // Return -1 to indicate an error
    }
    return (int)res;                // Return the result of the syscall
}


int mpi_poll(struct mpi_poll_entry * poll_pids, int npids, int timeout)
{
    int res;
    __asm__
    (
        "pushl %%eax;"
        "pushl %%ebx;"
        "pushl %%ecx;"
        "pushl %%edx;"
        "movl $246, %%eax;"
        "movl %1, %%ebx;"
        "movl %2, %%ecx;"
        "movl %3, %%edx;"
        "int $0x80;"
        "movl %%eax,%0;"
        "popl %%edx;"
        "popl %%ecx;"
        "popl %%ebx;"
        "popl %%eax;"
        : "=m" (res)
        : "m" (poll_pids) ,"m" (npids) ,"m"(timeout)
    );
  
    if (res >= (unsigned long)(-125))
    {
        errno = -res;
        res = -1;
    }
    return (int)res;    
}

#endif
