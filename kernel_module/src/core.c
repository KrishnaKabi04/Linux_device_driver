//////////////////////////////////////////////////////////////////////
//                     University of California, Riverside
//
//
//
//                             Copyright 2022
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for CSE202's Resource Container
//
////////////////////////////////////////////////////////////////////////

#include "blockmma.h"
#include <asm/segment.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <asm/current.h>
#include "core.h"

extern struct miscdevice blockmma_dev; //declared in interface.c
/**
 * Enqueue a task for the caller/accelerator to perform computation.
 */


// code : Krishna
/*
extern struct mystruct{
	int repeat;
	char name[64];
};

#define WR_VALUE _IOW('a', 'a', int32_t *)
#define RD_VALUE _IOR('a', 'b', int32_t *)
#define GREETER  _IOW('a', 'c', struct mystruct *)
*/
int driver_open(struct inode *device_file, struct file *instance) {
	printk("ioctl_example - blockmma file open was called!\n");
	return 0;
}

int driver_close(struct inode *device_file, struct file *instance) {
	printk("ioctl_example - blockmma file close was called!\n");
	return 0;
}
/*
int32_t answer = 42;

long my_ioctl(struct file *file, unsigned cmd, unsigned long arg){
	struct mystruct test;

	switch(cmd){
		case WR_VALUE:
			if(copy_from_user(&answer, (int32_t *) arg, sizeof(answer))) 
				printk("ioctl_example - Error copying data from user!\n");
			else
				printk("ioctl_example - Update the answer to %d\n", answer);
			break;
		case RD_VALUE:
			if(copy_to_user((int32_t *) arg, &answer, sizeof(answer))) 
				printk("ioctl_example - Error copying data to user!\n");
			else
				printk("ioctl_example - The answer was copied!\n");
			break;
		case GREETER:
			if(copy_from_user(&test, (struct mystruct *) arg, sizeof(test))) 
				printk("ioctl_example - Error copying data from user!\n");
			else
				printk("ioctl_example - %d greets to %s\n", test.repeat, test.name);
			break;
	}
	return 0;
}
*/
// end : Krishna

static int buffer_a[128];

long blockmma_send_task(struct blockmma_cmd __user *user_cmd) //write data from user - to driver
{
	//allocate memory for a acclerator
	//send a signal to get_task queue or wake up the process waiting for completion of send task 
	// target memory address, size of I/O transfer etc.. 
	// probably wake up by interrupt ? 

	struct task_struct *p = current;	
	int st= p->state; /* -1 unrunnable, 0 runnable, >0 stopped */
	printk("Process accessing: %s and PID is %i", current->comm, current->pid);
	printk("state is: %d", st);
	printk("PID in send task is: %d",p->pid);
	
	//struct mm_struct mm_ex;
	//access fields from user_cmd:

	printk("cmd op: %d", user_cmd->op);
	printk("cmd tid: %d", user_cmd->tid);
	printk("cmd a: %p, b: %p, c: %p", user_cmd->a, user_cmd->b, user_cmd->c);
	printk("cmd M: %d, N: %d, K: %d", user_cmd->m, user_cmd->n, user_cmd->k);
	
	struct page *page;
	//copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    
	return 0;
}

/**
 * Return until all outstanding tasks for the calling process are done
 */
int blockmma_sync(struct blockmma_cmd __user *user_cmd)
{
    return 0;
}

/**
 * Return the task id of a task to the caller/accelerator to perform computation.
 */
int blockmma_get_task(struct blockmma_hardware_cmd __user *user_cmd) //accelerator gets data from driver : write data from driver - device
{

	//copy to particular accelerator memory ?
	struct task_struct *p= current; //task_struct local to CPU

	printk("Entering GET TASK ! \n");
	printk("Process accessing: %d", current->pid);

    return 0;
}


/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{
    return 0;
}

/*
 * Tell us who wrote the module
 */
int blockmma_author(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct blockmma_hardware_cmd cmd;
    char authors[] = "Krishna Kabi(kkabi04), (SID: 862255132) ";// Yu-Chia Liu (yliu719), 987654321 and Hung-Wei Tseng (htseng), 123456789";
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd))) //copying data from user
    {
        return -1;
    }
    copy_to_user((void *)cmd.a, (void *)authors, sizeof(authors));

	printk("author_example - test_acc= %lld ,  cmd.tid= %lld \n", cmd.op, cmd.tid);
	printk("Process accessing in blockmma_author: %d", current->pid);
	//sleep
	/*int i=0
	while i<10000{
		printk(i);
	} */
    return 0;
}

int blockmma_init(void)
{
    int ret =0;
    if ((ret = misc_register(&blockmma_dev))) //register device with kernel
    {
        printk(KERN_ERR "Unable to register \"blockmma\" misc device\n");
        return ret;
    }
    printk("BlockMMA kernel module installed\n");
    return ret;
}

void blockmma_exit(void)
{
    printk("BlockMMA removed\n");
    misc_deregister(&blockmma_dev); // un-register device with kernel
}

