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
#include <linux/kthread.h>
#include "core.h"

extern struct miscdevice blockmma_dev; //declared in interface.c
/**
 * Enqueue a task for the caller/accelerator to perform computation.
 */


// code : Krishna
/*
int driver_open(struct inode *device_file, struct file *instance) {
	printk("ioctl_example - blockmma file open was called!\n");
	return 0;
}

int driver_close(struct inode *device_file, struct file *instance) {
	printk("ioctl_example - blockmma file close was called!\n");
	return 0;
}
*/
// end : Krishna

static DECLARE_WAIT_QUEUE_HEAD(send_wq);
static int send_flag=0; //use mutex to modify -> this flag used by get_task 
static int buffer_size;
static DECLARE_WAIT_QUEUE_HEAD(get_wq);

struct mutex shared_lock;
	
DEFINE_MUTEX(shared_lock); 

struct task_struct *proc = current;
int ctr_thread=1

int send_thread_func(struct blockmma_cmd __user *user_cmd)
{
	struct task_struct *tid=current;
	pid_t tid_d= current->pid;

	printk("PID in send task is: %d, tgid: %d \n",tid->pid, tid->tgid);
	printk("Task id in thread function: %i", tid_d);

	//grab lock for kernel_cmd and copy
	static struct blockmma_cmd kernel_cmd;
	int  *mat_a_mem, *mat_b_mem, *mat_c_mem;
	int resa, resb, resc, task_state;

	//user copy_from_user to copy the user_cmd to kernel space
	resa= copy_from_user(&kernel_cmd, user_cmd, sizeof(kernel_cmd)); //handle error
	printk("res: %d \n", resa);
	kernel_cmd.tid= tid_d;
	printk("cmd address of a : %lld, address of b: %lld, tile: %lld \n", kernel_cmd.a, kernel_cmd.b, kernel_cmd.tile);

	//grab lock for mat a,b and c and copy
	//Creating Physical memory using kmalloc() -- for matrix A,B and C
	size_t mat_size = ( kernel_cmd.tile * kernel_cmd.tile)*sizeof(int); //allocate 64KB for matrix A
	mat_a_mem = kmalloc(mat_size , GFP_KERNEL);
	if(mat_a_mem == 0){
            printk(KERN_INFO "Cannot allocate memory for matrix A in kernel\n");
            return -1;
    }

	mat_b_mem = kmalloc(mat_size , GFP_KERNEL);	
	if(mat_b_mem == 0){
            printk(KERN_INFO "Cannot allocate memory for matrix B in kernel\n");
            return -1;
    }

	mat_c_mem = kmalloc(mat_size , GFP_KERNEL);
	if(mat_c_mem == 0){
            printk(KERN_INFO "Cannot allocate memory for matrix C in kernel\n");
            return -1;
    }
	printk("I got: %zu bytes of memory\n", ksize(mat_c_mem));
	
	resa= copy_from_user(mat_a_mem, (void *)kernel_cmd.a, mat_size); //handle error
	resb= copy_from_user(mat_b_mem, (void *)kernel_cmd.b, mat_size); //handle error
	resc= copy_from_user(mat_c_mem, (void *)kernel_cmd.c, mat_size); //handle error
	printk("res of copy: %d %d, %d", resa, resb, resc);

	printk("size of mat_a_mem: %zu \n", sizeof(mat_a_mem));
	printk("Data copied for matrix a");

	printk("value of first value of matix A: %d, %d, %d, %d", *mat_a_mem, *(mat_a_mem+1), *(mat_a_mem+2), *(mat_a_mem+127));
	printk("value of first value of matix B: %d, %d, %d, %d", *mat_b_mem, *(mat_b_mem+1), *(mat_b_mem+2), *(mat_b_mem+127));

	printk("______________________________________________\n");

	//write to input_buffer -> if buffer full -> wait to write in buffer in a different queue

}

long blockmma_send_task(struct blockmma_cmd __user *user_cmd) //write data from user - to driver
{
	
	int resa, resb, resc, task_state;
	bool debug=true;
	
	//int st= p->state; /* 0 runnable, 1 interruptible, 2 un-interruptible, 4 stopped, 64 dead, 256 waking */
	printk("Process accessing: %s and PID is %i \n", current->comm, current->pid);
	printk("PID in send task is: %d, tgid: %d \n",p->pid, p->tgid);

	struct task_struct *send_thread;
	send_thread= kthread_run(send_thread_func, &user_cmd, "Send_Thread_"+str(ctr_thread)) ; // create thread and copy from user
	if (send_thread)
	{
		printk("send thread created for %i and %i", send_thread->pid, current->pid);
		ctr_thread=ctr_thread+1;
	}
	else 
	{
 		printk(KERN_ERR "Cannot create kthread for process id %i \n", current->pid); 
	}
	
	printk("Current process task id: %i and %i", current->pid, send_thread->pid);

	//Signal to get_task queue: put in a input buffer? -> if buffer full -> wait to write in buffer in a different queue

	//
	

	return 0;
}

/**
 * Return until all outstanding tasks for the calling process are done
 */
int blockmma_sync(struct blockmma_cmd __user *user_cmd)
{
	//all threads Poll for COMP: For this process -> iterate through all tasks and free memory -> stop threads


	//copy_to_user  free memory
	printk("Entering sync.. \n");
	size_t mat_size = ( kernel_cmd.tile * kernel_cmd.tile)*sizeof(int); 

	//write only to c matrix
	int res;
	res= copy_to_user((void *)kernel_cmd.c, (void *)mat_c_mem, mat_size);

	if (res!=0){
		printk("Blockmma sync copy of matrix C to user failed!! ");
	}
	kfree(mat_a_mem);
	kfree(mat_b_mem);
	kfree(mat_c_mem); 
	printk("Memory released! \n");

	kthread_stop(etx_thread);
	ctr_thread=0;
    return 0;
}

/**
 * Return the task id of a task to the caller/accelerator to perform computation.
 */


static struct blockmma_hardware_cmd hw_cmd;
int blockmma_get_task(struct blockmma_hardware_cmd __user *user_cmd) //accelerator gets data from driver : write data from driver - device
{

	wait_event_interruptible(); // (poll / interupt) 

	//copy to particular accelerator memory ?
	struct task_struct *p= current; //task_struct local to CPU

	printk("inside get task! \n");
	printk("cmd address of a : %lld, address of b: %lld, tile: %lld \n", kernel_cmd.a, kernel_cmd.b, kernel_cmd.tile);
	
	//copy_to_user data from kernel to accelerator
	printk("Process accessing: %d", current->pid);

	//struct blockmma_hardware_cmd kernel_cmd;
	//copy_to_user data from kernel to accelerator
	//get the stsrtaing address of a,b and c -> then copy the contents from kernel to these addresses.

	int resa, resb, resc;
	resa= copy_from_user(&hw_cmd, user_cmd, sizeof(hw_cmd)); //handle error
	
	printk("hardware cmd address of a : %lld, address of b: %lld, tile: %lld \n", hw_cmd.a, hw_cmd.b, hw_cmd.c);

	if (resa==0)
	{
		printk("Copy from hardware success! \n");
	}
	
	//kernel_cmd.tid= current->pid;
	size_t mat_size = ( kernel_cmd.tile * kernel_cmd.tile)*sizeof(int);
	resa= copy_to_user((void *)hw_cmd.a, (void *)mat_a_mem, mat_size);
	resb= copy_to_user((void *)hw_cmd.b, (void *)mat_b_mem, mat_size);
	resc= copy_to_user((void *)hw_cmd.c, (void *)mat_c_mem, mat_size);

	if ((resa==0) & (resb==0) & (resc==0))
	{
		printk("Copy to hardware success! \n");
	}
    
	return 0; 

	
}


/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{

	

	//read c
	//get value of only matrix C to kernel
	printk("Entering computation.. \n");
	int res;
	size_t mat_size = ( kernel_cmd.tile * kernel_cmd.tile)*sizeof(int);
	printk("hardware cmd address of a : %lld, address of b: %lld, tile: %lld \n", hw_cmd.a, hw_cmd.b, hw_cmd.c);

	res= copy_from_user(mat_c_mem, (void *)hw_cmd.c, mat_size);
	if (res!=0){
		printk("Copy failed!");
	}

	printk("copy for matrix C from hardware success!");

    return 0;
}

/*
 * Tell us who wrote the module
 */
int blockmma_author(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct blockmma_hardware_cmd cmd;
    char authors[] = "Krishna Kabi(kkabi004), (SID: 862255132) ";// Yu-Chia Liu (yliu719), 987654321 and Hung-Wei Tseng (htseng), 123456789";
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd))) //copying data from user
    {
        return -1;
    }

    copy_to_user((void *)cmd.a, (void *)authors, sizeof(authors));

	printk("author_example - test_acc= %lld ,  cmd.tid= %lld \n", cmd.op, cmd.tid);
	printk("Process accessing in blockmma_author: %d", current->pid);
	
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
