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
#include <asm/uaccess.h>

#include "core.h"

/*Queue implementation starts*/

int len = 0, temp, i = 0, ret;
char * empty = "Queue Empty    \n";
int emp_len, flag = 1;
long task_id = 0;
int incomplete_task = 0;
float * op_address;
char * msg;

//Linked list node
struct k_list {
    struct list_head queue_list; //struct of linked list
    //struct task_struct *p;
    struct blockmma_cmd * kernel_cmd_d;
    int *mat_a_d;
    int tsk_id;
};

static struct k_list * node, *entry;
struct list_head *task_queue, *ptr; //submit task to this queue ?
INIT_LIST_HEAD(&task_queue);

//LIST_HEAD(Head_node);
//static struct k_list mylist;
//INIT_LIST_HEAD(&mylist -> queue_list);

/*
struct blockmma_hardware_cmd * pop_queue(struct list_head * head) {
    struct k_list * node;
    struct blockmma_hardware_cmd * msg;
    struct list_head * queue_node;
    node = list_first_entry(head, struct k_list, queue_list);
    msg = node -> data;
    queue_node = & node -> queue_list;
    list_del(queue_node);
    len--;
    return msg;
}*/

//add elements to mylist
void push_queue(struct list_head * head, struct blockmma_cmd * k_cmd, int *a) {
    
    //struct k_list *node;
    //node = kmalloc(sizeof(struct k_list), GFP_KERNEL);
    node = kmalloc(sizeof(struct k_list * ), GFP_KERNEL); 
    node -> kernel_cmd_d = k_cmd;
    node -> mat_a_d= a;
    node -> tsk_id= 612;
    //node -> mat_b_d= b;
    //node -> mat_c_d= c;
    
    list_add_tail( & node -> queue_list, &task_queue);
    int count=0;
    //traverse the list
    list_for_each(ptr,&task_queue)
    {
        entry=list_entry(ptr, struct k_list, queue_list); //returns address of list in current sructure
        printk(KERN_INFO "\n task id:  %d   \n ", entry->tsk_id);
        printk(KERN_INFO "\n Hello %lld   \n ", entry->kernel_cmd_d.a);
        count++;
    }   
    printk("Total Nodes = %d\n", count);

    kfree(node);
    printk("Memory assigned to node freed! ");
    len++;

}

/*Queue implementation ends*/


extern struct miscdevice blockmma_dev;
/**
 * Enqueue a task for the caller/accelerator to perform computation.
 */
long blockmma_send_task(struct blockmma_cmd __user *user_cmd)
{

    printk("Process accessing: %s and PID is %i \n", current->comm, current->pid);
	printk("PID in send task is: %d, tgid: %d \n",p->pid, p->tgid);

    struct blockmma_cmd kernel_cmd;
    copy_from_user(&kernel_cmd, user_cmd, sizeof(kernel_cmd)); 

    //int m = user_cmd->m;
    //int n = user_cmd->n;
    //int k = user_cmd->k;
    
    int row = 0;
    int *mat_a; // *mat_b, *mat_c;
    mat_a = (int *)kmalloc(128*128*sizeof(int), GFP_KERNEL);
    //mat_b = (int *)kmalloc(128*128*sizeof(int), GFP_KERNEL);
    //mat_c = (int *)kmalloc(128*128*sizeof(int), GFP_KERNEL);

    //kernel_cmd.a = (__u64)a;
    //kernel_cmd.b = (__u64)b;
    //kernel_cmd.c = (__u64)c;
    //kernel_cmd.tid = (__u64)task_id;

    //op_address = (float *)(user_cmd->a);
    /*
    for(row=0;row<128;row++){
        copy_from_user(&(a[row]), &(((int *)(user_cmd->a))[row*n]), sizeof(int)*128);
        copy_from_user(&(b[row]), &(((int *)(user_cmd->b))[row*n]), sizeof(int)*128);
        copy_from_user(&(c[row]), &(((int *)(user_cmd->c))[row*n]), sizeof(int)*128);
    }
    */
    resa= copy_from_user(mat_a, (void *)kernel_cmd.a, mat_size);
    //resb= copy_from_user(mat_b, (void *)kernel_cmd.b, mat_size);
    //resc= copy_from_user(mat_c, (void *)kernel_cmd.c, mat_size);

    printk(KERN_INFO "\n Probably copy worked %d %d %d\n", kernel_cmd.m, kernel_cmd.n, kernel_cmd.k);
    printk("value of first value of matix A: %d, %d, %d, %d", *mat_a, *(mat_a+1), *(mat_a+2), *(mat_a+127));

    printk("I got: %zu bytes of memory\n", ksize(mat_a));

    //task_queue = queue_init();
    push_queue(task_queue, &kernel_cmd, &mat_a) ;//, &mat_b, &mat_c);


    //deallocating memory
    kfree(mat_a);
	//kfree(mat_b);
	//kfree(mat_c); 
	printk("Memory released! \n");

    //incomplete_task++;
    //printk(KERN_INFO "\n No error push\n");
    //printk("task_id: %d", (int)task_id);
    // pop_queue(sample_queue);
    // printk(KERN_INFO "\n No error pop 02-22-2022\n");
    return task_id++;
}

/**
 * Return until all outstanding tasks for the calling process are done
 */
int blockmma_sync(struct blockmma_cmd __user *user_cmd)
{
    printk("sync incomplete_task: %d", incomplete_task);
    // if(incomplete_task>0){
    //     return -1;
    // }
    // return 0;
    return 0;
}

/**
 * Return the task id of a task to the caller/accelerator to perform computation.
 */
int blockmma_get_task(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct blockmma_hardware_cmd *task;
    if(incomplete_task==0){
        return -1;
    }
    task = pop_queue(task_queue);
    copy_to_user((float *)user_cmd->a, (float *)task->a, sizeof(float)*128*128);
    copy_to_user((float *)user_cmd->b, (float *)task->b, sizeof(float)*128*128);
    copy_to_user((float *)user_cmd->c, (float *)task->c, sizeof(float)*128*128);
    printk("get_task no errors");
    printk("get_task task_id: %d", (int)(task->tid));
    return (int)(task->tid);
}


/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{
    float * result;
    result = (float *)(user_cmd->c);
    printk("starting to copy result");
    copy_to_user(op_address, result, sizeof(float)*128*128);
    printk("copied result to user");
    incomplete_task--;
    return (int)((int*)(user_cmd->tid));
}

/*
 * Tell us who wrote the module
 */
int blockmma_author(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct blockmma_hardware_cmd cmd;
    char authors[] = "Krishna Kabi(kkabi004), (SID: 862255132) "; //"Yu-Chia Liu (yliu719), 987654321 and Hung-Wei Tseng (htseng), 123456789";
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    copy_to_user((void *)cmd.a, (void *)authors, sizeof(authors));
    return 0;
}

int blockmma_init(void)
{
    int ret =0;
    if ((ret = misc_register(&blockmma_dev)))
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
    misc_deregister(&blockmma_dev);
}

