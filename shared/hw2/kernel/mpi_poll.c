#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/slab.h>
#include <linux/mpi.h>
#include <asm/uaccess.h>

int sys_mpi_poll(struct mpi_poll_entry *poll_pids, int npids, int timeout)
{
    printk(KERN_INFO "ERANROI - POLL: Entered sys_mpi_poll\n");
    if (npids < 1 || timeout < 0){
        printk(KERN_ERR "ERANROI - Invalid arguments: npids = %d, timeout = %d\n", npids, timeout);
        return -EINVAL;
    }
    if (current->mpi_registered == 0){
        printk(KERN_ERR "ERANROI - Current process not registered for MPI\n");
        return -EPERM;
    }

    int fail_cp;
    struct pid_queue *cur_pid_queue;
    pid_t cur_sender_pid;
    list_t *head_messages;
    int found = 0;
    list_t *q_it; // current queue iterator
    int i;

    printk(KERN_INFO "ERANROI - Initializing incoming fields to 0\n");
    for (i = 0; i < npids; ++i) {
        poll_pids[i].incoming = 0;
    }

    printk(KERN_INFO "ERANROI - Iterating through current->l_queue_by_pid\n");
    list_for_each(q_it, &current->l_queue_by_pid) {
        cur_pid_queue = list_entry(q_it, struct pid_queue, l_idx);
        cur_sender_pid = cur_pid_queue->sender_pid;
        printk(KERN_INFO "ERANROI - Checking sender_pid = %d\n", cur_sender_pid);
        for (i = 0; i < npids; ++i)
        {
            if (cur_sender_pid == poll_pids[i].pid){
                found++;
                poll_pids[i].incoming = 1;
                printk(KERN_INFO "ERANROI - Message found from pid = %d\n", cur_sender_pid);
            }
        }
    }

    if (found > 0) {
        printk(KERN_INFO "ERANROI - Found %d messages\n", found);
        return found;
    }

    printk(KERN_INFO "ERANROI - No messages found, going to sleep\n");
    printk(KERN_INFO "ERANROI - Allocating memory for watched_pids\n");
    current->watched_pids = kmalloc(sizeof(pid_t) * npids, GFP_KERNEL);
    if (!current->watched_pids){
        printk(KERN_ERR "ERANROI - ENOMEM: Could not allocate memory for watched_pids\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "ERANROI - Copying pids from user space\n");
    for (i = 0; i < npids; ++i){
        fail_cp = copy_from_user(&current->watched_pids[i], &poll_pids[i].pid, sizeof(pid_t));
        printk(KERN_INFO "ERANROI - current->watched_pids[%d] = %d\n", i, current->watched_pids[i]);
        if (fail_cp){
            printk(KERN_ERR "ERANROI - EFAULT: Failed to copy from user space\n");
            kfree(current->watched_pids);
            return -EFAULT;
        }
    }

    current->num_watched_pids = npids;
    printk(KERN_INFO "ERANROI - current->num_watched_pids = %d\n", current->num_watched_pids);
    printk(KERN_INFO "ERANROI - Setting current process state to TASK_UNINTERRUPTIBLE\n");
    set_current_state(TASK_UNINTERRUPTIBLE);

    printk(KERN_INFO "ERANROI - Going to schedule with timeout = %d\n", timeout);
    int sch_time = schedule_timeout(timeout * HZ);
    printk(KERN_INFO "ERANROI - schedule_timeout returned = %d\n", sch_time);

    printk(KERN_INFO "ERANROI - Freeing watched_pids\n");
    kfree(current->watched_pids);
    current->watched_pids = NULL;

    if (!sch_time) {
        printk(KERN_ERR "ERANROI - ETIMEDOUT: No message arrived before timeout\n");
        return -ETIMEDOUT;
    }

    printk(KERN_INFO "ERANROI - Woken up by message from sender_pid = %d\n", current->sender_pid);
    for (i = 0; i < npids; ++i) {
        if (current->sender_pid == poll_pids[i].pid){
            poll_pids[i].incoming = 1;
            printk(KERN_INFO "ERANROI - Incoming message from pid = %d\n", current->sender_pid);
            return 1;
        }
    }

    printk(KERN_WARNING "ERANROI - Should not reach here\n");
    return found;
}