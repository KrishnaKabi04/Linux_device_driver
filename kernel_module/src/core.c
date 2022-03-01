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

struct mutex send_lock;
struct mutex comp_lock;
struct mutex sync_lock;
struct mutex sync_hashmap_lock;
struct mutex task_cnt_lock;

//Linked list node

struct k_list {
    struct list_head task_queue; 
    struct list_head comp_queue;
    struct list_head sync_queue;

    struct blockmma_cmd kernel_cmd_d; //pushed in send task -- user space address
    struct blockmma_hardware_cmd hw_cmd_d; // pushed in get task -- hardware space address
    int *mat_b_d; //kernel space
    int bench_pid, hw_pid, tsk_id;

};


struct sync_hash_map{
    struct list_head sync_hash_list;
    int bench_pid;
    int task_cnt;
};

LIST_HEAD(task_head);  //submit task to this queue 
LIST_HEAD(comp_head); 
LIST_HEAD(sync_head);
LIST_HEAD(sync_hashmap_head);


static int global_tsk_count;
static size_t mat_size = (128 * 128)*sizeof(int);

void push_queue(struct blockmma_cmd k_cmd, int *b) {

    struct k_list *node_send;
    struct sync_hash_map *s_entry;
    struct list_head  *ptr_send, *ptr_sync;

    mutex_lock(&task_cnt_lock);
    global_tsk_count= global_tsk_count+1;
    mutex_unlock(&task_cnt_lock);
    

    node_send = kmalloc(sizeof(struct k_list * ), GFP_KERNEL); 
    //printk("size of point node_send after allocation: %zu", ksize(node_send));
    node_send -> kernel_cmd_d = k_cmd;
    //node_send -> mat_a_d= a;
    node_send -> mat_b_d= b;
    //node_send -> mat_c_d= c;
    node_send->bench_pid= current->pid;
    node_send -> tsk_id= global_tsk_count; //lock on variable

    //acq lock
    mutex_lock(&send_lock);
    list_add_tail(&node_send -> task_queue, &task_head);
    mutex_unlock(&send_lock);
    //release lock


    int key_found=-1;

    //adding node_send was success -> inc count of task in sync_map if pid found

    mutex_lock(&sync_hashmap_lock);
    list_for_each(ptr_sync, &sync_hashmap_head)
    {
            s_entry=list_entry(ptr_sync, struct sync_hash_map, sync_hash_list);

            //inc count of same entry in queue
            if (s_entry->bench_pid == current->pid) 
            {
                printk("found entry for current benchmark process \n ");
                key_found=1;
                s_entry->task_cnt= s_entry->task_cnt+1;
            }
    }
    mutex_unlock(&sync_hashmap_lock);

    mutex_lock(&sync_hashmap_lock);
    if (key_found==-1)
    {
        s_entry = kmalloc(sizeof(struct sync_hash_map * ), GFP_KERNEL); 
        s_entry->bench_pid= current->pid;
        s_entry->task_cnt= 1;
        list_add_tail( &s_entry -> sync_hash_list, &sync_hashmap_head);
    }
    mutex_unlock(&sync_hashmap_lock);
    //traverse the list
    if (debug){
        printk("Traversing the list \n");

        //acquire lock: task_queue
        mutex_lock(&send_lock);
        list_for_each(ptr_send,&task_head)
        {
            node_send=list_entry(ptr_send, struct k_list, task_queue);  
            printk(KERN_INFO "task id:  %d   \n ", node_send->tsk_id);
            printk(KERN_INFO "address of b  in user space %lld , kernel space: %lld, value: %d %d \n ", node_send->kernel_cmd_d.b, node_send->mat_b_d, *(node_send->mat_b_d), *(node_send->mat_b_d +1));
    	    *(node_send->mat_b_d) = 444;
		    *(node_send->mat_b_d+1)=445;

        }
        mutex_unlock(&send_lock);  
        //release lock 

    }


}

/*Queue implementation ends*/

/**
 * Enqueue a task for the caller/accelerator to perform computation.
 */
long blockmma_send_task(struct blockmma_cmd __user *user_cmd)
{

    struct blockmma_cmd kernel_cmd;
    int *mat_b; //*mat_a, *mat_b, mat_c;

    printk("Process accessing: %s and PID is %i \n", current->comm, current->pid);
    //printk("list empty: %d",  list_empty(&task_head));

	//printk("PID in send task is: %d, tgid: %d \n",p->pid, p->tgid);
    
    copy_from_user(&kernel_cmd, user_cmd, sizeof(kernel_cmd)); 
    //size_t mat_size = ( kernel_cmd.tile * kernel_cmd.tile)*sizeof(int);

    //mat_a = kmalloc(mat_size, GFP_KERNEL);
    mat_b = kmalloc(mat_size, GFP_KERNEL);
    //mat_c = kmalloc(mat_size, GFP_KERNEL);
    printk("I got: %zu bytes of memory\n", ksize(mat_b));
    
    //copy_from_user(mat_a, (void *)kernel_cmd.a, mat_size);
    copy_from_user(mat_b, (void *)kernel_cmd.b, mat_size);
    //copy_from_user(mat_c, (void *)kernel_cmd.c, mat_size); 

    int res, row=0;

    for(row=0;row<128;row++){
        res = copy_from_user(&(mat_a[row]), &(((int *)kernel_cmd.a)[row*kernel_cmd.n]), sizeof(int)*128);
        if(res != 0){
            printk("failed copying a into kern");
            printk("failed bytes: %d", (int)res);
        }
        res = copy_from_user(&(mat_b[row]), &(((int *)kernel_cmd.b)[row*kernel_cmd.k]), sizeof(int)*128);
        if(res != 0){
            printk("failed copying b into kern");
            printk("failed bytes: %d", (int)res);
        }
        res = copy_from_user(&(mat_c[row]), &(((int *)kernel_cmd.c)[row*kernel_cmd.k]), sizeof(int)*128);
        if(res != 0){
            printk("failed copying c into kern");
            printk("failed bytes: %d", (int)res);
        }
    }

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

    struct k_list *node_sync, *temp;
    struct sync_hash_map * s_node;
    struct list_head  *ptr_sync;
    int k, row=0;

    //iterate through sync_hash_map_queue and check task_count for your pid:
    mutex_lock(&sync_hashmap_lock);
    list_for_each(ptr_sync,&sync_hashmap_head)
    {
        s_node=list_entry(ptr_sync, struct sync_hash_map, sync_hash_list);  //returns address of list in current sructure
        
        //printk(KERN_INFO " s_node->benchpid: %d task cnt : %d  \n", s_node->bench_pid, s_node->task_cnt );

        if ((s_node->bench_pid == current->pid) & (s_node->task_cnt == 0) )
        {
            printk("\n ---------- Inside sync -------------- \n");
            //iterate through sync_queue -> copy to user -> release memory
            mutex_lock(&sync_lock);
            list_for_each_entry_safe(node_sync, temp, &sync_head, sync_queue)
            {
                if (node_sync->bench_pid == current->pid)
                {

                    printk(KERN_INFO "address of b  in user space  %lld,  kernel space: %lld ", node_sync->kernel_cmd_d.b, node_sync->mat_b_d);
                    printk("value: %d %d %d %d  \n ",  *(node_sync->mat_b_d), *(node_sync->mat_b_d +1), *(node_sync->mat_b_d+2), *(node_sync->mat_b_d+3));
                    
                    row = 0;
                    k= node_sync->kernel_cmd_d.k;
                    for(row=0;row<128;row++){
                        copy_to_user(&(((int *)node_sync->kernel_cmd_d.b)[row*k]), &(((int *)node_sync->mat_b_d)[row]), sizeof(int)*128);
                    }
                    //copy_to_user((int *)node_sync->kernel_cmd_d.b, (int *)node_sync->mat_b_d, mat_size);
                    printk("Copied to user space ! \n");

                    printk("size of point read before mat_b kfree: %zu", ksize(node_sync->mat_b_d));
                    //kfree(node_sync->mat_a_d);
                    kfree(node_sync->mat_b_d);
                    //kfree(node_sync->mat_c_d);
                    //printk("size of point read before mat_b kfree: %zu", ksize(node_sync->mat_b_d));
                    list_del(&node_sync->sync_queue);
                    kfree(node_sync);

                }
            }
            mutex_unlock(&sync_lock);
            mutex_unlock(&sync_hashmap_lock);
            return 0;
        }

    }  
    mutex_unlock(&sync_hashmap_lock);
    return -1;
    
}

/**
 * Return the task id of a task to the caller/accelerator to perform computation.
 */
static struct blockmma_hardware_cmd hw_cmd;
int blockmma_get_task(struct blockmma_hardware_cmd __user *user_cmd)
{

    //acquire lock: task_queue??
    mutex_lock(&send_lock); 
    if (list_empty(&task_head)!=0) //non zero-> empty
	{
        //release lock task_queue
        mutex_unlock(&send_lock); 
        return -1;
    }
    mutex_unlock(&send_lock);

    struct k_list *entry, *node_get, *temp;
    struct list_head  *ptr_get, *ptr_comp ;

    printk("inside get task \n");
    copy_from_user(&hw_cmd, user_cmd, sizeof(hw_cmd)); //handle error

	//grab first task in queue
    
    mutex_lock(&send_lock);
    entry = list_first_entry(&task_head, struct k_list, task_queue);

    printk(KERN_INFO "\n Benchmark PID: %d, task id:  %d   \n ", entry->bench_pid, entry->tsk_id);
    printk("mat b address in user space: %lld, in kernel space: %lld, mat_b values: %d %d \n", entry->kernel_cmd_d.b, entry->mat_b_d, *(entry->mat_b_d), *(entry->mat_b_d +1));
    
    //*(entry->mat_b_d)= 444;
    //*(entry->mat_b_d+1)=445;

    //copy_to_user((void *)hw_cmd.a, (void *)node->mat_a_d, mat_size);
    copy_to_user((void *)hw_cmd.b, (void *)entry->mat_b_d, mat_size);
	//copy_to_user((void *)hw_cmd.c, (void *)node->mat_c_d, mat_size);
    printk("Copy to hardware success! \n");

    //add the current node details to another queue and pop from current queue
    entry->hw_cmd_d = hw_cmd;
    entry->hw_pid = current->pid;
    mutex_lock(&comp_lock);
    list_add_tail(&entry -> comp_queue, &comp_head);
    mutex_unlock(&comp_lock);


    //remove from task_queue_list
    list_del(&entry->task_queue);
    mutex_unlock(&send_lock);

    if (debug)
    {
        printk("check for other elemenst in task_queue list! \n");
        mutex_lock(&send_lock);
        list_for_each(ptr_get, &task_head)
        {
            node_get=list_entry(ptr_get, struct k_list, task_queue);  //returns address of list in current sructure
            printk(KERN_INFO "address of b  in user space  %lld,  kernel space: %lld ", node_get->kernel_cmd_d.b, node_get->mat_b_d);
            printk("value: %d %d %d %d  \n ",  *(node_get->mat_b_d), *(node_get->mat_b_d +1), *(node_get->mat_b_d+2), *(node_get->mat_b_d+3));
        }
        mutex_unlock(&send_lock);

        printk("check for other elemenst in comp_queue list! \n");
        mutex_lock(&comp_lock);
        list_for_each(ptr_comp,&comp_head)
        {
            node_get=list_entry(ptr_comp, struct k_list, comp_queue);  //returns address of list in current sructure
            printk(KERN_INFO "address of b  in user space  %lld, kernel space: %lld ", node_get->kernel_cmd_d.b, node_get->mat_b_d);
            printk("value: %d %d %d %d  \n ",  *(node_get->mat_b_d), *(node_get->mat_b_d +1), *(node_get->mat_b_d+2), *(node_get->mat_b_d+3));
        }
        mutex_unlock(&comp_lock);
    }

    return entry->tsk_id; 
    //return 0;
}


/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct k_list *entry, *temp;
    struct sync_hash_map *s_entry;
    struct list_head  *ptr_sync, *ptr_comp ;

    printk("Entering comp for pid %d", current->pid);
    //might use read lock later
    mutex_lock(&comp_lock);
    list_for_each_entry_safe(entry, temp, &comp_head, comp_queue)
    {
        //node=list_entry(ptr, struct k_list, queue_list);  //returns address of list in current sructure
        if (debug)
        {
            printk(KERN_INFO "HW PID is %d and task id:  %d   \n ", entry->hw_pid, entry->tsk_id);
            printk(KERN_INFO "address of b  in user space  %lld,  kernel space: %lld \n",  entry->kernel_cmd_d.b, entry->mat_b_d);
            printk("value: %d %d %d %d  \n ",  *(entry->mat_b_d), *(entry->mat_b_d +1), *(entry->mat_b_d+2), *(entry->mat_b_d+3));
            printk("entry->hw_pid: %d %d", entry->hw_pid, current->pid);
            printk("\n");
        }


        if (entry->hw_pid == current->pid)
        {
            
            //copy from hw to kernel
            copy_from_user(entry->mat_b_d, (void *)entry->hw_cmd_d.b, mat_size);
            printk("value: %d  %d  %d  %d  \n ",  *(entry->mat_b_d), *(entry->mat_b_d +1), *(entry->mat_b_d+2), *(entry->mat_b_d+3));
            printk("Copied from hw space ! \n");

            //decrease count in sync_hash_map
            mutex_lock(&sync_hashmap_lock);
            list_for_each(ptr_sync,&sync_hashmap_head)
            {
                s_entry=list_entry(ptr_sync, struct sync_hash_map, sync_hash_list);

                //dec count of task_cnt for corresponding bench process
                if (s_entry->bench_pid == entry->bench_pid) 
                {
                    s_entry->task_cnt= s_entry->task_cnt-1;
                }
            }        
            mutex_unlock(&sync_hashmap_lock);

            mutex_lock(&sync_lock);
            list_add_tail(&entry -> sync_queue, &sync_head); //adding to sync queue
            mutex_unlock(&sync_lock);
            
            list_del(&entry->comp_queue); //pop from comp_queue

            if (debug)
            {
                printk("check for other element in comp_queue list! \n");
                list_for_each(ptr_comp, &comp_head)
                {
                    entry=list_entry(ptr_comp, struct k_list, comp_queue);  
                    printk(KERN_INFO "address of b  in user space  %lld,  kernel space: %lld ", entry->kernel_cmd_d.b, entry->mat_b_d);
                    printk("value: %d %d %d %d  \n ",  *(entry->mat_b_d), *(entry->mat_b_d +1), *(entry->mat_b_d+2), *(entry->mat_b_d+3));
                } 

                printk("check for other elements in sync_queue list! \n");
                mutex_lock(&sync_lock);
                list_for_each(ptr_sync,&sync_head)
                {
                    entry=list_entry(ptr_sync, struct k_list, sync_queue);  
                    printk(KERN_INFO "address of b  in user space  %lld,  kernel space: %lld ", entry->kernel_cmd_d.b, entry->mat_b_d);
                    printk("value: %d %d %d %d  \n ",  *(entry->mat_b_d), *(entry->mat_b_d +1), *(entry->mat_b_d+2), *(entry->mat_b_d+3));
                
                    //printk(" Bench PID: %d task_count= %d \n ",  entry->bench_pid, entry->task_cnt);
                } 
                mutex_unlock(&sync_lock);
                printk("size of point read before mat_b kfree: %zu", ksize(entry->mat_b_d));
                printk("size of point read before kfree: %zu \n", ksize(entry));
            }
        }
    }   
    mutex_unlock(&comp_lock);
	//printk("Check if list is empty! \n ");
	//printk("list empty: %d",  list_empty(&comp_head));
	//printk("Done Comp! \n");
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
    mutex_init(&send_lock);
    mutex_init(&comp_lock);
    mutex_init(&sync_lock);
    mutex_init(&sync_hashmap_lock);
    mutex_init(&task_cnt_lock);
    printk("BlockMMA kernel module installed\n");
    return ret;
}

void blockmma_exit(void)
{
    printk("BlockMMA removed\n");
    misc_deregister(&blockmma_dev);
}

