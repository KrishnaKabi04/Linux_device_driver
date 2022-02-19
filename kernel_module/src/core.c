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
*/
int driver_open(struct inode *device_file, struct file *instance) {
	printk("ioctl_example - blockmma file open was called!\n");
	return 0;
}

int driver_close(struct inode *device_file, struct file *instance) {
	printk("ioctl_example - blockmma file close was called!\n");
	return 0;
}

// end : Krishna

//queue has to be global
int  *mat_a_mem, *mat_b_mem, *mat_c_mem;
static struct blockmma_cmd kernel_cmd;
long blockmma_send_task(struct blockmma_cmd __user *user_cmd) //write data from user - to driver
{
	//allocate memory for a acclerator
	//send a signal to get_task queue or wake up the process waiting for completion of send task 
	// target memory address, size of I/O transfer etc.. 
	// probably wake up by interrupt ? 

	struct task_struct *p = current;
	int res, task_state;

	bool debug=True;

	int st= p->state; /* 0 runnable, 1 interruptible, 2 un-interruptible, 4 stopped, 64 dead, 256 waking */
	printk("Process accessing: %s and PID is %i \n", current->comm, current->pid);
	printk("state is: %d \n", st);
	printk("PID in send task is: %d, tgid: %d \n",p->pid, p->tgid);
	
	//user copy_from_user to copy the user_cmd to kernel space
	if(res= copy_from_user(&kernel_cmd, user_cmd, sizeof(kernel_cmd) !=0){
		if (debug)printk("Copy from user to kernel for task id: %d failed", p->tgid);
		return -1;
	} 
	
	kernel_cmd.tid= current->pid;
	if (debug)printk("cmd address of a : %lld, address of b: %lld, tile: %lld \n", kernel_cmd.a, kernel_cmd.b, kernel_cmd.tile);

	//access_ok(arg, cmd)
	//Creating Physical memory using kmalloc() -- for matrix C
	size_t mat_size = ( kernel_cmd.tile * kernel_cmd.tile)*sizeof(int); //allocate 64KB for matrix A
	mat_a_mem = kmalloc(mat_size , GFP_KERNEL);

	if(mat_a_mem == 0){
            printk(KERN_INFO "Cannot allocate memory in kernel\n");
            return -1;
    }

	printk("I got: %zu bytes of memory\n", ksize(mat_a_mem));

	res= copy_from_user(mat_a_mem, (void *)kernel_cmd.a, mat_size); //handle error
	printk("res of copy: %d", res);

	printk("size of mat_a_mem: %zu\n", sizeof(mat_a_mem));
	printk("Data copied for matrix a");

	printk("value of first value of matix A: %d, %d, %d, %d", *mat_a_mem, *(mat_a_mem+1), *(mat_a_mem+2), *(mat_a_mem+127));

	printk("______________________________________________\n");
	printk("SDFsdfsdfsdf");

	return 0;
}

/**
 * Return until all outstanding tasks for the calling process are done
 */
int blockmma_sync(struct blockmma_cmd __user *user_cmd)
{

	//copy_to_user  free memory
	printk("Entering sync.. ");

	kfree(mat_a_mem); 
	printk("Memory released! \n");

    return 0;
}

/**
 * Return the task id of a task to the caller/accelerator to perform computation.
 */
int blockmma_get_task(struct blockmma_hardware_cmd __user *user_cmd) //accelerator gets data from driver : write data from driver - device
{

	//copy to particular accelerator memory ?
	struct task_struct *p= current; //task_struct local to CPU

	printk("inside get task!");
	printk("cmd address of a : %lld, address of b: %lld, tile: %lld \n", kernel_cmd.a, kernel_cmd.b, kernel_cmd.tile);
	
	//copy_to_user data from kernel to accelerator
	printk("Process accessing: %d", current->pid);

	//struct blockmma_hardware_cmd kernel_cmd;
	//copy_to_user data from kernel to accelerator
	//get the stsrtaing address of a,b and c -> then copy the contents from kernel to these addresses.

	static struct blockmma_hardware_cmd hw_cmd;
	int res= copy_from_user(&hw_cmd, user_cmd, sizeof(hw_cmd)); //handle error
	
	if (res==0)
	{
		printk("Copy from hardware success! \n");
	}
	
	//kernel_cmd.tid= current->pid;
	size_t mat_size = ( kernel_cmd.tile * kernel_cmd.tile)*sizeof(float);
	res= copy_to_user((void *)hw_cmd.a, (void *)mat_a_mem, mat_size);

	if (res==0)
	{
		printk("Copy to hardware success! \n");
	}
    
	return 0; 

	//return taskid : who copied data ?
}


/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{
	//read c
	//get value of matrix C to kernel
	struct blockmma_hardware_cmd kernel_cmd;
	int res;
	res= copy_from_user(&kernel_cmd, user_cmd, sizeof(kernel_cmd));
	if (res!=0){
		printk("Copy failed!");
	}
	
	printk("copy success!");


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

