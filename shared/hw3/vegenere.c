/* vegenere.c: Example char device module for Vigenere cipher operations.
 *
 * This module demonstrates the implementation of a character device that
 * supports reading, writing, and IOCTL operations to manipulate data using
 * a Vigenere cipher. The cipher uses a provided key to encrypt or decrypt data.
 */

/* Kernel Programming */
#define MODULE
#define LINUX
#define __KERNEL__

#include <linux/kernel.h>  
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/errno.h>  
#include <asm/segment.h>
#include <asm/current.h>
#include "vegenere.h"

#define MY_DEVICE "vegenere"

MODULE_AUTHOR("Anonymous");
MODULE_LICENSE("GPL");

/* globals */
int my_major = 0; // Major number for the device
char alphabet[62] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','0','1','2','3','4','5','6','7','8','9'};
static private_data minors_pd[MINORS_NUM] = {}; // Array to hold private data for each minor

static struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .ioctl = my_ioctl,
    .write = my_write,
    .llseek = my_llseek,
};

// Function to get index of character in the alphabet
int getAlphabetLocation(unsigned char c){
    int i;
    for(i=0; i<62; i++){
        if(alphabet[i] == c){
            return i;
        }
    }
    return -1;
}

// Function to encrypt a single character using the Vigenere cipher
char encryptChar(unsigned char msg, int i, int encryption_key_size, int* encryption_key){
    int loc = getAlphabetLocation(msg);
    i = i % (encryption_key_size);
    
    if(loc != -1){
       int index = loc + encryption_key[i];
        if(index >= 62) { 
            index = index % 62;
        }
        return alphabet[index]; // Return encrypted character
    }
    return msg; // Return original character if not in alphabet
}

// Function to decrypt a single character using the Vigenere cipher
char decryptChar(unsigned char msg, int i,int encryption_key_size, int* encryption_key){
    int loc = getAlphabetLocation(msg);
    i = i % (encryption_key_size);
    if(loc != -1){
       int index = loc - encryption_key[i];
        if(index < 0) { 
            index = loc - encryption_key[i] + 62;  // Wrap around if index is negative
        }
        return alphabet[index]; // Return decrypted character
    }
    return msg; // Return original character if not in alphabet
}

// Function to encrypt a buffer of data
void encryptBuffer(unsigned char* msg, int length,int encryption_key_size, int* encryption_key, int offset){
    int i;
    for(i=0; i<length; i++){
        msg[i] = encryptChar(msg[i], i + offset,encryption_key_size,encryption_key);
    }
}

// Function to decrypt a buffer of data
void decryptBuffer(unsigned char* msg, int length,int encryption_key_size, int* encryption_key, loff_t offset){
    int i;
    for( i=0; i<length; i++){
        msg[i] = decryptChar(msg[i], i + offset,encryption_key_size,encryption_key);
    }
}

// Module initialization function
int init_module(void)
{
    // This function is called when inserting the module using insmod

    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
	printk(KERN_WARNING "can't get dynamic major\n");
	return my_major;
    }

    int i;
    for(i=0; i<MINORS_NUM; i++){
        minors_pd[i].device_buffer = NULL;
        minors_pd[i].buf_size = 0;
        minors_pd[i].encryption_key = NULL;
        minors_pd[i].key_length = 0;
        minors_pd[i].is_debug = 0;
    }
    
    return 0;
}

// Module cleanup function
void cleanup_module(void)
{
    // This function is called when removing the module using rmmod

    unregister_chrdev(my_major, MY_DEVICE);

    return;
}

// Function to handle file opening
int my_open(struct inode *inode, struct file *filp)
{
    if ((inode == NULL) || (filp == NULL)){
        return -EFAULT;
    }
    int minor_number = MINOR(inode->i_rdev);
    filp->private_data = &(minors_pd[minor_number]);

    return 0;
}

// Function to handle file release
int my_release(struct inode *inode, struct file *filp)
{
    // handle file closing

    return 0;
}

// Function to handle reading from the device
ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    int i;
    private_data *pd = (private_data*)filp->private_data;
    if(buf == NULL || count < 0 || f_pos == NULL || filp == NULL)
    {
        return -EFAULT;
    }
    if ((pd->encryption_key == NULL) && (pd->is_debug == 0) ){
         return -EINVAL;
    }
    if(count == 0){
        return 0;
    }
    char* bytes_that_were_read = kmalloc(sizeof(char)*(count), GFP_KERNEL);
    if(bytes_that_were_read == NULL){
        return -ENOMEM;
    }
    int start_pos = *f_pos;
    if(start_pos == -1){
        start_pos = 0;
    }
    int num_of_available_bytes_to_read = (pd->buf_size) - (start_pos);

    if (num_of_available_bytes_to_read <= count){
        count = num_of_available_bytes_to_read;
    }

    char* data_for_user = (char*)kmalloc(sizeof(char)*count, GFP_KERNEL);
    for(i=0; i<count; i++){
        data_for_user[i]= pd->device_buffer[start_pos+i];
    }
    
    loff_t offset = *f_pos;
    if(pd->is_debug == 0){
        decryptBuffer(data_for_user,count,pd->key_length,pd->encryption_key, offset);
    }
    if(copy_to_user(buf, data_for_user, sizeof(char)*count) != 0){
        kfree(data_for_user);
        return -EBADF;
    }
    if (data_for_user != NULL){
        kfree(data_for_user);
    }
    
    *f_pos = *f_pos + count;
    return count;
}

// Function to handle writing to the device
ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos){

    private_data *pd = (private_data*)filp->private_data;
    int first_available_byte = 0;
    int i;

    if(buf == NULL || filp == NULL ||f_pos == NULL || count < 0){
        return -EFAULT;
    }
    if ((pd->encryption_key == NULL) && (pd->is_debug == 0) ){
         return -EINVAL;
    }
    if(count == 0){
        return 0;
    }
    char *new_device_buffer = (char*)kmalloc(sizeof(char)*(pd->buf_size+count), GFP_KERNEL);
    if(new_device_buffer == NULL){
        return -ENOMEM;
    }
    for(i = 0; i < pd->buf_size; i++){
        new_device_buffer[i] = pd->device_buffer[i];
    }
    first_available_byte = i;
    char *user_data = (char*)kmalloc(sizeof(char)*count, GFP_KERNEL);
    if(user_data == NULL){
        return -ENOMEM;
    }
    if(copy_from_user(user_data, buf, sizeof(char)*count) != 0){
        return -EBADF;
    }
    if (pd->is_debug == 0){
        encryptBuffer(user_data,count,pd->key_length,pd->encryption_key, first_available_byte);
    }
    for (i = 0; i <count; i++){
        new_device_buffer[first_available_byte+i] = user_data[i]; 
    }
    if(pd->device_buffer != NULL){
        kfree(pd->device_buffer);
    }
    if(user_data != NULL){
        kfree(user_data);
    }
    
    pd->device_buffer = new_device_buffer;
    pd->buf_size +=count;

    return count; 
}

// Function to handle file seek operations
loff_t my_llseek(struct file *filp, loff_t offset, int ignore){

    loff_t new_offset = 0;
    new_offset = filp->f_pos + offset;
    private_data *pd = (private_data*)filp->private_data;
    int buf_size = pd->buf_size;

    if(new_offset < 0){
        filp->f_pos = 0;
        return 0;
    }
    else if(new_offset >= buf_size){
        filp->f_pos = buf_size;
        return buf_size;
    }

    filp->f_pos = new_offset;
    return new_offset;
}

// IOCTL function to handle various control requests from user space
int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    private_data *pd = (private_data*)filp->private_data;
    int i;
    int value;
    int keysize;
    char *encryption_key_string = NULL;
    char *encryption_key_string_user = NULL;
    int* encryption_key;

    switch(cmd)
    {
    case SET_KEY:
        encryption_key_string_user = (char *)arg;
        if((encryption_key_string_user) == NULL){
            return -EINVAL;
        }
        keysize = strlen_user(encryption_key_string_user) - 1;
        if (keysize == 0){
             return -EINVAL;
        }
        encryption_key_string = (char*)kmalloc(sizeof(char)*keysize, GFP_KERNEL);
        encryption_key = (int*)kmalloc(sizeof(int)*keysize, GFP_KERNEL);
        if(encryption_key_string == NULL || encryption_key == NULL){
            return -ENOMEM;
        }
        if(copy_from_user(encryption_key_string, encryption_key_string_user, sizeof(char)*keysize) != 0){
            kfree(encryption_key_string);
            return -EBADF;
        }
        for(i=0; i<keysize; i++){
            encryption_key[i] = getAlphabetLocation(encryption_key_string[i]) + 1;
        }
        if (pd->encryption_key != NULL){
            kfree(pd->encryption_key);
        }
        pd->encryption_key = encryption_key;
        pd->key_length = keysize;
	break;

    case RESET:
        if(pd->device_buffer != NULL){
            kfree(pd->device_buffer);
        }
        pd->device_buffer = NULL;
        pd->buf_size = 0;
        if(pd->encryption_key != NULL){
            kfree(pd->encryption_key);
        }
        pd->encryption_key = NULL;
        pd->key_length = 0;
        pd->is_debug = 0;
        filp->f_pos = 0;
	break;

    case DEBUG:
        value = (int)arg;
        if ((value != 1) && (value !=0)){
            return -EINVAL;
        }
        pd->is_debug = value;
        return 0;

	break;

    default:
	    return -ENOTTY;
    }

    return 0;
}
