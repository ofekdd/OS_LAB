#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/errno.h>

// Structure to represent a message in the MPI system
struct mpi_message {
    char *message;             // Pointer to the message data
    ssize_t size;              // Size of the message
    pid_t sender_pid;          // PID of the process that sent the message
    struct list_head list;     // List node for linking messages in the queue
};

// Structure to represent a process registered in the MPI system
struct mpi_process {
    pid_t pid;                 // PID of the registered process
    struct list_head message_queue; // Queue of messages sent to this process
    struct list_head list;     // List node for linking processes in the global list
};

// Global linked list to keep track of all registered MPI processes
LIST_HEAD(mpi_processes);
// Spinlock to protect access to the global linked list
DEFINE_SPINLOCK(mpi_lock);

// Function to find an MPI process in the global list by PID
static struct mpi_process *find_mpi_process(pid_t pid) {
    struct mpi_process *proc;

    // Iterate over the list to find the process with the given PID
    list_for_each_entry(proc, &mpi_processes, list) {
        if (proc->pid == pid) {
            return proc;  // Return the process if found
        }
    }

    return NULL;  // Return NULL if the process is not found
}

// System call to register the current process in the MPI system
asmlinkage int mpi_register(void) {
    struct mpi_process *proc;
    unsigned long flags;

    // Acquire the spinlock to protect access to the global list
    spin_lock_irqsave(&mpi_lock, flags);

    // Check if the process is already registered
    if (find_mpi_process(current->pid)) {
        spin_unlock_irqrestore(&mpi_lock, flags);
        printk(KERN_INFO "mpi_register: Process %d is already registered\n", current->pid);
        return 0; // Return 0 if the process is already registered
    }

    // Allocate memory for a new mpi_process structure
    proc = kmalloc(sizeof(*proc), GFP_KERNEL);
    if (!proc) {
        spin_unlock_irqrestore(&mpi_lock, flags);
        printk(KERN_ERR "mpi_register: Failed to allocate memory for process %d\n", current->pid);
        return -ENOMEM;  // Return -ENOMEM if memory allocation fails
    }

    // Initialize the new mpi_process structure
    proc->pid = current->pid;
    INIT_LIST_HEAD(&proc->message_queue);
    // Add the new process to the global list
    list_add(&proc->list, &mpi_processes);

    // Release the spinlock
    spin_unlock_irqrestore(&mpi_lock, flags);

    printk(KERN_INFO "mpi_register: Process %d registered successfully\n", current->pid);
    return 0;  // Return 0 on success
}

// System call to send a message to a specified process
asmlinkage int mpi_send(pid_t pid, char *message, ssize_t message_size) {
    struct mpi_process *proc;
    struct mpi_message *msg;
    unsigned long flags;

    if (!message || message_size < 1) {
        printk(KERN_ERR "mpi_send: Invalid message or size from process %d\n", current->pid);
        return -EINVAL;  // Return error if message is invalid
    }

    // Acquire the spinlock to protect access to the global list
    spin_lock_irqsave(&mpi_lock, flags);

    // Find the target process by PID
    proc = find_mpi_process(pid);
    if (!proc) {
        spin_unlock_irqrestore(&mpi_lock, flags);
        printk(KERN_ERR "mpi_send: Target process %d not found\n", pid);
        return -ESRCH;  // Return error if the target process is not found
    }

    // Allocate memory for a new mpi_message structure
    msg = kmalloc(sizeof(*msg), GFP_KERNEL);
    if (!msg) {
        spin_unlock_irqrestore(&mpi_lock, flags);
        printk(KERN_ERR "mpi_send: Failed to allocate memory for message\n");
        return -ENOMEM;  // Return -ENOMEM if memory allocation fails
    }

    // Allocate memory for the message data
    msg->message = kmalloc(message_size, GFP_KERNEL);
    if (!msg->message) {
        kfree(msg);
        spin_unlock_irqrestore(&mpi_lock, flags);
        printk(KERN_ERR "mpi_send: Failed to allocate memory for message data\n");
        return -ENOMEM;  // Return -ENOMEM if memory allocation fails
    }

    // Copy the message data from user space to kernel space
    if (copy_from_user(msg->message, message, message_size)) {
        kfree(msg->message);
        kfree(msg);
        spin_unlock_irqrestore(&mpi_lock, flags);
        printk(KERN_ERR "mpi_send: Failed to copy message from user space for process %d\n", current->pid);
        return -EFAULT;  // Return error if copying fails
    }

    // Initialize the new mpi_message structure
    msg->size = message_size;
    msg->sender_pid = current->pid;
    // Add the message to the target process's message queue
    list_add_tail(&msg->list, &proc->message_queue);

    // Release the spinlock
    spin_unlock_irqrestore(&mpi_lock, flags);

    printk(KERN_INFO "mpi_send: Process %d sent a message to process %d\n", current->pid, pid);
    return 0;  // Return 0 on success
}

// System call to receive a message from a specified process
asmlinkage int mpi_receive(pid_t pid, char *message, ssize_t message_size) {
    struct mpi_process *proc;
    struct mpi_message *msg, *tmp;
    ssize_t copied_size = 0;
    unsigned long flags;

    if (!message || message_size < 1) {
        printk(KERN_ERR "mpi_receive: Invalid message buffer or size for process %d\n", current->pid);
        return -EINVAL;  // Return error if message buffer is invalid
    }

    // Acquire the spinlock to protect access to the global list
    spin_lock_irqsave(&mpi_lock, flags);

    // Find the current process in the global list
    proc = find_mpi_process(current->pid);
    if (!proc) {
        spin_unlock_irqrestore(&mpi_lock, flags);
        printk(KERN_ERR "mpi_receive: Process %d is not registered\n", current->pid);
        return -EPERM;  // Return error if the current process is not registered
    }

    // Iterate over the message queue to find a message from the specified sender
    list_for_each_entry_safe(msg, tmp, &proc->message_queue, list) {
        if (msg->sender_pid == pid) {
            // Remove the message from the queue
            list_del(&msg->list);
            copied_size = min(msg->size, message_size);
            // Copy the message data from kernel space to user space
            if (copy_to_user(message, msg->message, copied_size)) {
                spin_unlock_irqrestore(&mpi_lock, flags);
                kfree(msg->message);
                kfree(msg);
                printk(KERN_ERR "mpi_receive: Failed to copy message to user space for process %d\n", current->pid);
                return -EFAULT;  // Return error if copying fails
            }
            // Free the allocated memory for the message
            kfree(msg->message);
            kfree(msg);
            spin_unlock_irqrestore(&mpi_lock, flags);
            printk(KERN_INFO "mpi_receive: Process %d received a message from process %d\n", current->pid, pid);
            return copied_size;  // Return the size of the copied message
        }
    }

    // Release the spinlock
    spin_unlock_irqrestore(&mpi_lock, flags);
    printk(KERN_INFO "mpi_receive: No message from process %d found for process %d\n", pid, current->pid);
    return -EAGAIN;  // Return error if no message from the specified sender is found
}
