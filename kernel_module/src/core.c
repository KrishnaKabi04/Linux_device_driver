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

bool debug= true;
extern struct miscdevice blockmma_dev;

//Linked list node
struct k_list {
    struct list_head queue_list; //struct of linked list
    struct task_struct *proc;
    struct blockmma_cmd kernel_cmd_d;
    int *mat_b_d; //*mat_a_d, *mat_b_d, *mat_c_d;
    int tsk_id;
};

struct hw_data {
    struct list_head comp_list;
    struct task_struct *proc_hw;
    //struct blockmma_cmd kernel_cmd_d;
    struct blockmma_hardware_cmd hw_cmd_d;
    int *ker_mat_b; // *ker_mat_a, *ker_mat_b --to release memory from kernel
    int *user_mat_b; // kernel_cmd_d.c
    int bench_pid;
    int tsk_id; 

};

struct sync_map{
    struct list_head sync_list;
    int bench_pid;
    int task_cnt=0;
};

static size_t mat_size = ( 128 * 128)*sizeof(int);
static struct k_list * node, *entry, *temp;
static struct hw_data * h_node, *h_entry, *h_temp;
static struct sync_map * s_node, *s_entry, *s_temp;
static struct list_head  *ptr ;

LIST_HEAD(task_queue);  //submit task to this queue ?
LIST_HEAD(comp_queue); 
LIST_HEAD(sync_queue);

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


void push_queue(struct blockmma_cmd k_cmd, int *b) {
    
    //printk("size of point node before allocation: %zu", ksize(node));
    node = kmalloc(sizeof(struct k_list * ), GFP_KERNEL); 
    //printk("size of point node after allocation: %zu", ksize(node));
    node -> kernel_cmd_d = k_cmd;
    //node -> mat_a_d= a;
    node -> mat_b_d= b;
    //node -> mat_c_d= c;
    node->proc= current;
    node -> tsk_id= count+1;
    //count++;
    list_add_tail( &node -> queue_list, &task_queue);
    
    int key_found=-1;
    //adding node was success -> inc count of task in sync_map if pid found
    list_for_each(ptr,&sync_queue)
    {
            entry=list_entry(ptr, struct sync_map, sync_list);

            //inc count of same entry in queue
            if (entry->bench_pid == current->pid) 
            {
                printk("found entry for current benchmark process \n ");
                key_found=1;
                s_node->task_cnt= s_node->task_cnt+1;
            }
    }

    if (key_found==-1)
    {
        s_node = kmalloc(sizeof(struct sync_map * ), GFP_KERNEL); 
        s_node->bench_pid= current->pid;
        s_node->task_cnt= s_node->task_cnt+1;
    }
    else{
        key_found=-1;
    }

    //traverse the list
    if (debug){
        printk("Traversing the list \n");

        list_for_each(ptr,&task_queue)
        {
            entry=list_entry(ptr, struct k_list, queue_list);  //returns address of list in current sructure
            printk(KERN_INFO "task id:  %d   \n ", entry->tsk_id);
            printk(KERN_INFO "address of b  in user space %lld , kernel space: %lld, value: %d %d \n ", entry->kernel_cmd_d.b, entry->mat_b_d, *(entry->mat_b_d), *(entry->mat_b_d +1));
    	    *(entry->mat_b_d) = 444;
		    *(entry->mat_b_d+1)=445;

        }   

    }
    entry=NULL;
    node=NULL;

    printk("Total Nodes = %d \n", count);

}

/*Queue implementation ends*/

/**
 * Enqueue a task for the caller/accelerator to perform computation.
 */
long blockmma_send_task(struct blockmma_cmd __user *user_cmd)
{

    struct task_struct *p=current;
    struct blockmma_cmd kernel_cmd;
    int *mat_b; //*mat_a, *mat_b, mat_c;
    printk("Process accessing: %s and PID is %i \n", current->comm, current->pid);
    printk("list empty: %d",  list_empty(&task_queue));

	//printk("PID in send task is: %d, tgid: %d \n",p->pid, p->tgid);
    
    copy_from_user(&kernel_cmd, user_cmd, sizeof(kernel_cmd)); 
    //    size_t mat_size = ( kernel_cmd.tile * kernel_cmd.tile)*sizeof(int);

    //mat_a = kmalloc(mat_size, GFP_KERNEL);
    mat_b = kmalloc(mat_size, GFP_KERNEL);
    //mat_c = kmalloc(mat_size, GFP_KERNEL);
    printk("I got: %zu bytes of memory\n", ksize(mat_b));
    
    //copy_from_user(mat_a, (void *)kernel_cmd.a, mat_size);
    copy_from_user(mat_b, (void *)kernel_cmd.b, mat_size);
    //copy_from_user(mat_c, (void *)kernel_cmd.c, mat_size);

    if (debug)
    {
        printk(KERN_INFO "\n address of a in user space %lld add of b: %lld \n", kernel_cmd.a, kernel_cmd.b);
        printk("value of first value of matix B: %d, %d, %d, %d", *mat_b, *(mat_b+1), *(mat_b+2), *(mat_b+127));
    }
    
    push_queue(kernel_cmd, mat_b); // mat_a, mat_b, mat_c);
	printk("task pushed for pid %d \n", current->pid);

    return 0;
}

/**
 * Return until all outstanding tasks for the calling process are done
 */
int blockmma_sync(struct blockmma_cmd __user *user_cmd)
{


    if (list_empty(&sync_queue)!=0) //non zero-> empty
	{
        return -1;
    }

	printk("\n ---------- Inside sync -------------- \n");
    ///* Go through the list and free the memory. */

    /*
    printk("Copying tasks to user space");
    //list_for_each_entry_safe(node, temp, &task_queue, queue_list)
    list_for_each_entry_safe(h_node, h_temp, &comp_queue, comp_list) 
    {
        //node=list_entry(ptr, struct k_list, queue_list);  //returns address of list in current sructure
        if (debug)
        {
            printk(KERN_INFO "PID is %d and task id:  %d   \n ", node->proc->pid, node->tsk_id);
            printk(KERN_INFO "address of b  in user space  %lld,  kernel space: %lld \n", node->kernel_cmd_d.b, node->mat_b_d);
            printk("value: %d  %d  %d  %d  \n ",  *(node->mat_b_d), *(node->mat_b_d +1), *(node->mat_b_d+2), *(node->mat_b_d+3));
            printk("\n");
        }

        printk("node->proc->pid: %d %d", node->proc->pid, current->pid);

        if (node->proc->pid == current->pid)
        {

            *(node->mat_b_d)= 612;
            *(node->mat_b_d+1)= 613;
            printk("value: %d  %d  %d  %d  \n ",  *(node->mat_b_d), *(node->mat_b_d +1), *(node->mat_b_d+2), *(node->mat_b_d+3));
            copy_to_user((int *)node->kernel_cmd_d.b, (int *)node->mat_b_d, mat_size);
            printk("Copied to user space ! ");

            printk("size of point read before mat_b kfree: %zu", ksize(node->mat_b_d));
            //kfree(node->mat_a_d);
            kfree(node->mat_b_d);
            //kfree(node->mat_c_d);
            //kfree(node->kernel_cmd_d);
            //printk("size of point read before mat_b kfree: %zu", ksize(node->mat_b_d));
            printk("size of point read before kfree: %zu \n", ksize(node));

            list_del(&node->queue_list);
            kfree(node);
            node=NULL;
            //printk("size of point read after kfree: %zu \n", ksize(node));
            count--;

        }
    }   

	printk("Check if list is empty! \n ");
	printk("list empty: %d",  list_empty(&task_queue));
	printk("Done sync! \n");

    return 0;
    */
}

/**
 * Return the task id of a task to the caller/accelerator to perform computation.
 */
static struct blockmma_hardware_cmd hw_cmd;

int blockmma_get_task(struct blockmma_hardware_cmd __user *user_cmd)
{

    if (list_empty(&task_queue)!=0) //non zero-> empty
	{
        return -1;
    }

    copy_from_user(&hw_cmd, user_cmd, sizeof(hw_cmd)); //handle error

    
	//grab first task in queue
    node = list_first_entry(&task_queue, struct k_list, queue_list);
    printk(KERN_INFO "\n PID: %d, task id:  %d   \n ", node->proc->pid, node->tsk_id);
    printk("mat b address in user space: %lld, in kernel space: %lld, mat_b values: %d %d \n", node->kernel_cmd_d.b, node->mat_b_d, *(node->mat_b_d), *(node->mat_b_d +1));
    //copy_to_user((void *)hw_cmd.a, (void *)node->mat_a_d, mat_size);
    copy_to_user((void *)hw_cmd.b, (void *)node->mat_b_d, mat_size);
	//copy_to_user((void *)hw_cmd.c, (void *)node->mat_c_d, mat_size);
    printk("Copy to hardware success! \n");

    //add the current matrix to another queue and pop from current queue
    h_node = kmalloc(sizeof(struct hw_data * ), GFP_KERNEL); 
    h_node->proc_hw= current;
    h_node->hw_cmd_d= hw_cmd;
    h_node ->ker_mat_b= node->mat_b_d;
    h_node->user_mat_b= node->kernel_cmd_d.b;
    h_node->bench_pid= node->proc->pid;
    h_node -> tsk_id= node->tsk_id;
    list_add_tail( &h_node -> comp_list, &comp_queue);

    //kfree(node->mat_a_d); //can free mata and mat_b after copying to hardware. only keep mat_c
    //kfree(node->mat_b_d);
    //kfree(node->mat_c_d);
    //kfree(node->kernel_cmd_d);
    list_del(&node->queue_list);
    kfree(node);


    printk("Remaining nodes: %d \n", count);

    printk("check for other elemenst in task_queue list! \n");
    list_for_each(ptr, &task_queue)
    {
        node=list_entry(ptr, struct k_list, queue_list);  //returns address of list in current sructure
        printk(KERN_INFO "address of b  in user space  %lld,  kernel space: %lld ", node->kernel_cmd_d.b, node->mat_b_d);
        printk("value: %d %d %d %d  \n ",  *(node->mat_b_d), *(node->mat_b_d +1), *(node->mat_b_d+2), *(node->mat_b_d+3));
    } 

    printk("check for other elemenst in comp_queue list! \n");
    list_for_each(ptr,&comp_queue)
    {
        h_node=list_entry(ptr, struct hw_data, comp_list);  //returns address of list in current sructure
        printk(KERN_INFO "address of b  in user space  %lld, kernel space: %lld ", h_node->user_mat_b, h_node->ker_mat_b);
        printk("value: %d %d %d %d  \n ",  *(h_node->ker_mat_b), *(h_node->ker_mat_b +1), *(h_node->ker_mat_b+2), *(h_node->ker_mat_b+3));
    } 

    return h_node->tsk_id; 
    return 0;
}


/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{
    /*
    float * result;
    result = (float *)(user_cmd->c);
    printk("starting to copy result");
    copy_to_user(op_address, result, sizeof(float)*128*128);
    printk("copied result to user");
    incomplete_task--;
    return (int)((int*)(user_cmd->tid));
    */
    
    printk("Entering comp for pid %d", current->pid);

    list_for_each_entry_safe(h_node, h_temp, &comp_queue, comp_list)
    {
        //node=list_entry(ptr, struct k_list, queue_list);  //returns address of list in current sructure
        if (debug)
        {
            printk(KERN_INFO "PID is %d and task id:  %d   \n ", h_node->proc_hw->pid, h_node->tsk_id);
            printk(KERN_INFO "address of b  in user space  %lld,  kernel space: %lld \n", h_node->user_mat_b, h_node->ker_mat_b);
            printk("value: %d  %d  %d  %d  \n ",  *(h_node->ker_mat_b), *(h_node->ker_mat_b+1), *(h_node->ker_mat_b+2), *(h_node->ker_mat_b+3));
            printk("\n");
        }

        printk("node->proc->pid: %d %d", h_node->proc_hw->pid, current->pid);

        if (h_node->proc_hw->pid == current->pid)
        {
            
            //copy from hw to kernel
            copy_from_user(h_node->ker_mat_b, (void *)h_node->hw_cmd_d.b, mat_size);
            printk("value: %d  %d  %d  %d  \n ",  *(h_node->ker_mat_b), *(h_node->ker_mat_b +1), *(h_node->ker_mat_b+2), *(h_node->ker_mat_b+3));
            printk("Copied from hw space ! \n");

            printk("size of point read before mat_b kfree: %zu", ksize(h_node->ker_mat_b));
            //kfree(node->mat_a_d);
            kfree(h_node->ker_mat_b);
            //kfree(node->mat_c_d);
            //kfree(node->kernel_cmd_d);
            //printk("size of point read before mat_b kfree: %zu", ksize(node->mat_b_d));
            printk("size of point read before kfree: %zu \n", ksize(h_node));

            list_del(&h_node->comp_list);
            kfree(h_node);
            h_node=NULL;
            //printk("size of point read after kfree: %zu \n", ksize(node));


        }
    }   

	printk("Check if list is empty! \n ");
	printk("list empty: %d",  list_empty(&comp_queue));
	printk("Done sync! \n");

   return 0;
}

/*
 * Tell us who wrote the module
 */
int blockmma_author(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct blockmma_hardware_cmd cmd;
    char authors[] = "Krishna Kabi(kkabi004), (SID: 862255132) "; 
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

