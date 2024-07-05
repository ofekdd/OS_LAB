#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mpi.h>

// Register the current process for MPI communication
int sys_mpi_register(void) {
    // Mark the current process as registered for MPI
    current->mpi_registered = 1;
    // Initialize the list head for the message queue
    current->l_queue_by_pid.next = &current->l_queue_by_pid;
    current->l_queue_by_pid.prev = &current->l_queue_by_pid;
    return 0;
}

// Receive a message from a specific sender process
int sys_mpi_receive(pid_t sender_pid, char* user_buffer, ssize_t buffer_length) {
    // Validate input parameters
    if (buffer_length < 1 || user_buffer == NULL) {
        return -EINVAL;
    }
    // Check if the current process is registered for MPI
    if (current->mpi_registered == 0) {
        return -EPERM;
    }
    
    struct pid_queue* sender_queue;
    pid_t sender_pid_check;
    list_t *message_list = NULL;
    int is_found = 0;
    list_t *iterator; // current queue iterator

    // Iterate through the message queues to find the sender's queue
    list_for_each(iterator, &current->l_queue_by_pid) {
        sender_queue = list_entry(iterator, struct pid_queue, l_idx);
        sender_pid_check = sender_queue->sender_pid;
        if (sender_pid_check == sender_pid) {
            is_found = 1;
            message_list = &sender_queue->messages;
            break;
        }
    }
    // If no messages from the sender are found, return an error
    if (is_found == 0 || list_empty(message_list)) {
        return -EAGAIN;
    }
    // Retrieve the first message from the sender's queue
    struct message_node *message_node_ptr = (struct message_node*)list_entry(message_list->next, struct message_node, l_idx);

    // Determine the size to copy
    ssize_t return_length = message_node_ptr->message_size < buffer_length ? message_node_ptr->message_size : buffer_length;
    // Copy the message to the user buffer
    int copy_status = copy_to_user(user_buffer, message_node_ptr->message, return_length);
    if (copy_status) {
        return -EFAULT;
    }
    // Remove the message from the queue and free the memory
    list_del(&message_node_ptr->l_idx);
    kfree(message_node_ptr->message);
    kfree(message_node_ptr);

    // If no messages are left from this sender, remove the sender's queue
    if (list_empty(message_list)) {
        list_del(iterator);
        kfree(sender_queue);
    }

    return return_length;
}

// Send a message to a specific process
int sys_mpi_send(pid_t pid, char *message, ssize_t message_size) {
    printk(KERN_INFO "ERANROI - SEND: Entered sys_mpi_send\n");
    if (message_size < 1 || message == NULL) {
        printk(KERN_ERR "ERANROI - Invalid arguments: message_size = %zd, message = %p\n", message_size, message);
        return -EINVAL;
    }
    task_t *p = find_task_by_pid(pid);
    if (!p) {
        printk(KERN_ERR "ERANROI - ESRCH: Could not find task with pid = %d\n", pid);
        return -ESRCH;
    }
    if (p->mpi_registered == 0 || current->mpi_registered == 0) {
        printk(KERN_ERR "ERANROI - EPERM: Either sender or receiver not registered for MPI\n");
        return -EPERM;
    }
    struct pid_queue *cur_pid_queue;
    pid_t cur_sender_pid;
    list_t *head_messages = NULL;
    int found = 0;
    list_t *q_it; // current queue iterator

    printk(KERN_INFO "ERANROI - Iterating through receiver's l_queue_by_pid\n");
    list_for_each(q_it, &p->l_queue_by_pid) {
        cur_pid_queue = list_entry(q_it, struct pid_queue, l_idx);
        cur_sender_pid = cur_pid_queue->sender_pid;
        if (cur_sender_pid == current->pid) {
            found = 1;
            head_messages = &cur_pid_queue->messages;
            break;
        }
    }
    if (found == 0) {
        printk(KERN_INFO "ERANROI - Creating new pid_queue for sender\n");
        struct pid_queue *pq = kmalloc(sizeof(struct pid_queue), GFP_KERNEL);
        if (!pq) {
            printk(KERN_ERR "ERANROI - ENOMEM: Could not allocate memory for pid_queue\n");
            return -ENOMEM;
        }
        pq->sender_pid = current->pid;

        pq->messages.next = &pq->messages;
        pq->messages.prev = &pq->messages;

        head_messages = &pq->messages;
        list_add(&pq->l_idx, &p->l_queue_by_pid);
    }
    struct message_node *mn = kmalloc(sizeof(struct message_node), GFP_KERNEL);
    if (!mn) {
        printk(KERN_ERR "ERANROI - ENOMEM: Could not allocate memory for message_node\n");
        return -ENOMEM;
    }
    mn->message_size = message_size;
    mn->message = kmalloc(message_size * sizeof(char), GFP_KERNEL);
    if (!mn->message) {
        printk(KERN_ERR "ERANROI - ENOMEM: Could not allocate memory for message\n");
        kfree(mn);
        return -ENOMEM;
    }
    int fail_cp = copy_from_user(mn->message, message, message_size);
    if (fail_cp) {
        printk(KERN_ERR "ERANROI - EFAULT: Failed to copy message from user space\n");
        kfree(mn->message);
        kfree(mn);
        return -EFAULT;
    }
    list_add_tail(&mn->l_idx, head_messages);
    printk(KERN_INFO "ERANROI - Message added to receiver's message list\n");

    printk(KERN_INFO "ERANROI - Checking if receiver has watched_pids\n");
    int i;
    if (p->watched_pids != NULL) {
        for (i = 0; i < p->num_watched_pids; ++i) {
            printk(KERN_INFO "ERANROI - Receiver watched_pids[%d] = %d\n", i, p->watched_pids[i]);
            if (current->pid == p->watched_pids[i]) {
                printk(KERN_INFO "ERANROI - Waking up receiver\n");
                p->sender_pid = current->pid;
                set_task_state(p, TASK_RUNNING);
                wake_up_process(p);
            }
        }
    }

    return 0;
}
