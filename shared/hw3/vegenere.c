/* vegenere.c: Example char device module.
 *
 * This module implements a character device that encrypts and decrypts data 
 * using the Vigenère cipher. It includes read, write, open, release, 
 * llseek, and ioctl operations.
 */

/* Kernel Programming */
#define MODULE
#define LINUX
#define __KERNEL__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <asm/segment.h>
#include <asm/current.h>
#include <linux/slab.h>

#include "vegenere.h"

#define MY_DEVICE "vegenere"

MODULE_AUTHOR("Anonymous");
MODULE_LICENSE("GPL");

/* Structure to hold device information */
struct device_info {
    char* data;         // Data buffer
    int* key;           // Encryption key
    int key_l;          // Length of the encryption key
    int i_data_end;     // End of data in the buffer
    int i_mem_end;      // End of allocated memory
    int is_debug;       // Debug mode flag
};

/* Structure to associate minor number with device info */
struct minor_di {
    unsigned int minor;         // Minor number
    struct device_info* di;     // Pointer to device information
    list_t l_idx;               // List node
};

/* Globals */
int my_major = 0; /* Major number of the device driver */
list_t inode_list = LIST_HEAD_INIT(inode_list); /* List of inode structures */

/* Helper function to convert a character to its index in the dictionary */
int char_to_index(char c){
    char dict[62]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int i;
    for (i = 0; i < 62; ++i)
        if (dict[i]==c)
            return i;
    return -1;
}

/* Helper function to convert an index to a character in the dictionary */
int index_to_char(int i){
    char dict[62]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    return dict[i%62];
}

/* Function to encrypt a character */
char get_encrypted(char c, int i, struct device_info* di){
    if (di->is_debug)
        return c;
    int c_i = char_to_index(c);
    if (c_i==-1)
        return c;
    return index_to_char(c_i + di->key[i % di->key_l]);
}

/* Function to decrypt a character */
char get_decrypted(char c, int i, struct device_info* di){
    if (di->is_debug)
        return c;
    int c_i = char_to_index(c);
    if (c_i == -1)
        return c;
    c_i -= di->key[i % di->key_l];
    if (c_i < 0)
        c_i += 62;
    return index_to_char(c_i);
}

/* File operations structure */
struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .llseek = my_llseek,
    .ioctl = my_ioctl, // Updated to use `ioctl`
};

/* Module initialization function */
int init_module(void)
{
    printk(KERN_INFO "vegenere: Initializing the vegenere module\n");

    // This function is called when inserting the module using insmod
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);
    if (my_major < 0)
    {
        printk(KERN_WARNING "vegenere: can't get dynamic major\n");
        return my_major;
    }

    inode_list.next = &inode_list;
    inode_list.prev = &inode_list;

    printk(KERN_INFO "vegenere: Module loaded with major number %d\n", my_major);
    return 0;
}

/* Module cleanup function */
void cleanup_module(void)
{
    printk(KERN_INFO "vegenere: Cleaning up module\n");

    // Unregister the character device
    unregister_chrdev(my_major, MY_DEVICE);

    // Release inode_list dynamically allocated memory
    list_t *idi_it, *idi_it_n;
    struct minor_di* cur_minor_di;

    list_for_each_safe(idi_it, idi_it_n, &inode_list) {
        cur_minor_di = list_entry(idi_it, struct minor_di, l_idx);

        if (cur_minor_di->di) {
            printk(KERN_INFO "vegenere: Freeing device info for minor %u\n", cur_minor_di->minor);

            if (cur_minor_di->di->data) {
                printk(KERN_INFO "vegenere: Freeing data buffer for minor %u\n", cur_minor_di->minor);
                kfree(cur_minor_di->di->data);
                cur_minor_di->di->data = NULL;
            }

            if (cur_minor_di->di->key) {
                printk(KERN_INFO "vegenere: Freeing key buffer for minor %u\n", cur_minor_di->minor);
                kfree(cur_minor_di->di->key);
                cur_minor_di->di->key = NULL;
            }

            printk(KERN_INFO "vegenere: Freeing device_info struct for minor %u\n", cur_minor_di->minor);
            kfree(cur_minor_di->di);
            cur_minor_di->di = NULL;
        }

        printk(KERN_INFO "vegenere: Freeing minor_di structure for minor %u\n", cur_minor_di->minor);
        list_del(&cur_minor_di->l_idx);
        kfree(cur_minor_di);
        cur_minor_di = NULL;
    }

    printk(KERN_INFO "vegenere: Module cleanup complete\n");
}


/* Open function */
int my_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "vegenere: Opening device\n");

    int found = 0;
    struct device_info* di = NULL;
    struct minor_di* cur_minor_di = NULL;
    list_t *q_it; // current queue iterator

    list_for_each(q_it, &inode_list) {
        cur_minor_di = list_entry(q_it, struct minor_di, l_idx);
        if (MINOR(inode->i_rdev) == cur_minor_di->minor) {
            found = 1;
            filp->private_data = cur_minor_di->di;
            di = cur_minor_di->di;
            printk(KERN_INFO "vegenere: Found existing device_info for minor %u\n", cur_minor_di->minor);
            break;
        }
    }

    if (!found) {
        printk(KERN_INFO "vegenere: Creating new device_info\n");
        di = (struct device_info*)kmalloc(sizeof(struct device_info), GFP_KERNEL);
        if (!di) {
            printk(KERN_ERR "vegenere: Failed to allocate memory for device_info\n");
            return -ENOMEM;
        }
        filp->private_data = di;
        di->data = NULL;
        di->key = NULL;
        di->is_debug = 0;

        struct minor_di *new_minor_di = (struct minor_di*)kmalloc(sizeof(struct minor_di), GFP_KERNEL);
        if (!new_minor_di) {
            printk(KERN_ERR "vegenere: Failed to allocate memory for minor_di\n");
            kfree(di);
            return -ENOMEM;
        }
        new_minor_di->minor = MINOR(inode->i_rdev);
        new_minor_di->di = di;

        list_add_tail(&new_minor_di->l_idx, &inode_list);
    }

    if (!di->data) {
        printk(KERN_INFO "vegenere: Allocating data buffer\n");
        di->data = (char*)kmalloc(32 * sizeof(char), GFP_KERNEL);
        if (!di->data) {
            printk(KERN_ERR "vegenere: Failed to allocate memory for data buffer\n");
            return -ENOMEM;
        }
        di->i_mem_end = 32;
        di->i_data_end = 0;
    }

    printk(KERN_INFO "vegenere: Device opened successfully\n");
    return 0;
}


/* Release function */
int my_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "vegenere: Releasing device\n");

    struct device_info *di = (struct device_info *)filp->private_data;

    if (di) {
        printk(KERN_INFO "vegenere: Valid device_info structure\n");

        if (di->data) {
            printk(KERN_INFO "vegenere: Freeing data buffer\n");
            kfree(di->data);
            di->data = NULL;
        } else {
            printk(KERN_INFO "vegenere: Data buffer already NULL\n");
        }

        if (di->key) {
            printk(KERN_INFO "vegenere: Freeing key buffer\n");
            kfree(di->key);
            di->key = NULL;
        } else {
            printk(KERN_INFO "vegenere: Key buffer already NULL\n");
        }

        printk(KERN_INFO "vegenere: Freeing device_info structure\n");
        kfree(di);
        filp->private_data = NULL;
    } else {
        printk(KERN_INFO "vegenere: device_info structure is NULL\n");
    }

    printk(KERN_INFO "vegenere: Device released\n");
    return 0;
}


/* Read function */
ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    struct device_info* di = (struct device_info*)filp->private_data;
    if (!buf){
        return -EFAULT;
    }
    if (!di->key){
        return -EINVAL;
    }
    int i;
    if (*f_pos + count >= di->i_data_end)
        count = di->i_data_end - *f_pos;

    for (i = 0; i < count; ++i){
        char decrypted_char = get_decrypted(di->data[*f_pos + i], *f_pos + i, di);
        if (copy_to_user(&buf[i], &decrypted_char, 1))
            return -EFAULT;
    }
    *f_pos += count;

    return count;  
}

/* Write function */
ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    struct device_info* di = (struct device_info*)filp->private_data;
    if (!buf){
        return -EFAULT;
    }
    if (!di->data){
        return -EINVAL;
    }
    if (!di->key){
        return -EINVAL;
    }
    int i;
    if (di->i_data_end + count >= di->i_mem_end){
        int new_size = di->i_mem_end * 2;
        while (di->i_data_end + count >= new_size)
            new_size *= 2;
        char* new_data = (char*)kmalloc(new_size * sizeof(char), GFP_KERNEL);
        if (!new_data){
            return -ENOMEM;
        }
        di->i_mem_end = new_size;
        memcpy(new_data, di->data, di->i_data_end);
        kfree(di->data);
        di->data = new_data;
    }

    for (i = 0; i < count; ++i) {
        char c;
        if (copy_from_user(&c, &buf[i], 1))
            return -EFAULT;
        di->data[di->i_data_end + i] = get_encrypted(c, di->i_data_end + i, di);
    }
    di->i_data_end += count;

    return count; 
}

/* LLSeek function */
loff_t my_llseek(struct file *filp, loff_t offset, int type)
{
    struct device_info* di = (struct device_info*)filp->private_data;
    if (filp->f_pos + offset >= di->i_data_end){
        filp->f_pos = di->i_data_end;
        if (filp->f_pos < 0)
            filp->f_pos = 0;
    }
    else if (filp->f_pos + offset < 0){
        filp->f_pos = 0;
    }
    else
        filp->f_pos += offset;
    return filp->f_pos; 
}

/* IOCTL function */
int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct device_info* di = (struct device_info*)filp->private_data;

    char* r_key;
    int i;
    printk(KERN_INFO "vegenere: IOCTL command received\n");

    switch(cmd)
    {
    case SET_KEY:
        printk(KERN_INFO "vegenere: SET_KEY command\n");
        r_key = (char*)arg;
        if (!r_key){
            printk(KERN_ERR "vegenere: Invalid key pointer\n");
            return -EINVAL;
        }

        di->key_l = strnlen_user(r_key, 256) - 1; // 256 is an arbitrary limit
        printk(KERN_INFO "vegenere: Key length is %d\n", di->key_l);
        if (di->key_l < 1){
            printk(KERN_ERR "vegenere: Key length is invalid\n");
            return -EINVAL;
        }
        di->key = (int*)kmalloc(di->key_l * sizeof(int), GFP_KERNEL);
        if (!di->key){
            printk(KERN_ERR "vegenere: Memory allocation for key failed\n");
            return -ENOMEM;
        }

        for (i = 0; i < di->key_l; ++i) {
            di->key[i] = char_to_index(r_key[i]) + 1;
            printk(KERN_INFO "vegenere: Key[%d] = %d\n", i, di->key[i]);
        }

        return 0;
    case RESET:
        printk(KERN_INFO "vegenere: RESET command\n");
        if (di->data){
            kfree(di->data);
        }
        di->data = NULL;
        if (di->key){
            kfree(di->key);
        }
        di->key = NULL;
        break;
    case DEBUG:
        printk(KERN_INFO "vegenere: DEBUG command\n");
        i = (int)arg;
        di->is_debug = i;
        if (!(i == 0 || i == 1)) {
            printk(KERN_ERR "vegenere: Invalid debug value\n");
            return -EINVAL;
        }
        return 0;
    default:
        printk(KERN_ERR "vegenere: Unknown IOCTL command\n");
        return -ENOTTY;
    }

    printk(KERN_INFO "vegenere: IOCTL command handled\n");
    return 0;
}
