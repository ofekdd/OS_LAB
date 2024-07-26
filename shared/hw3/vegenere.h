#ifndef _VEGENERE_H_
#define _VEGENERE_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define MINORS_NUM 256  // Defines the number of minor devices

// Structure for private data
typedef struct private_struct {		
	char *device_buffer;  // Buffer to hold data
    ssize_t buf_size;  // Size of the buffer
    int* encryption_key;  // Pointer to the encryption key array
    int key_length;  // Length of the encryption key
    int is_debug;  // Debug mode flag
    
}private_data;

//
// Function prototypes for device operations
//
int my_open(struct inode *, struct file *);
int my_release(struct inode *, struct file *);
ssize_t my_write(struct file *, const char *, size_t, loff_t *);
ssize_t my_read(struct file *, char *, size_t, loff_t *);
int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
loff_t my_llseek(struct file *, loff_t, int);

// IOCTL definitions
#define MY_MAGIC 'r'  // Magic number for IOCTL
#define SET_KEY  _IOW(MY_MAGIC, 0, char*)  // IOCTL to set the encryption key
#define RESET  _IO(MY_MAGIC, 1)  // IOCTL to reset the device
#define DEBUG  _IOW(MY_MAGIC, 2, int)  // IOCTL to set/clear debug mode

#endif
