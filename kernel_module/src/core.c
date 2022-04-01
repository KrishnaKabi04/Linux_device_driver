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
//   Author:  Krishna Kabi
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

bool debug= false;
extern struct miscdevice blockmma_dev;

struct mutex send_lock;
struct mutex comp_lock;
struct mutex sync_lock;
struct mutex sync_hashmap_lock;
struct mutex task_cnt_lock;

DEFINE_MUTEX(send_lock);
DEFINE_MUTEX(comp_lock);
DEFINE_MUTEX(sync_lock);
DEFINE_MUTEX(sync_hashmap_lock);
DEFINE_MUTEX(task_cnt_lock);

struct k_list {
    struct list_head task_queue; 
    struct list_head comp_queue;
    struct list_head sync_queue;

    __u64 user_add_c, hw_add_c;
    float *ker_mat_a, *ker_mat_b, *ker_mat_c; //kernel space matrices
    int bench_pid, hw_pid, tsk_id, k;

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


static int global_tsk_count=0;
static size_t mat_size = (128 * 128)*sizeof(int);

void push_queue(struct blockmma_cmd k_cmd, float *a, float *b, float *c) {

    struct k_list *node_send;
    struct sync_hash_map *s_entry;
    struct list_head  *ptr_sync;
    int key_found;

    mutex_lock(&task_cnt_lock);
    global_tsk_count= global_tsk_count+1;
    mutex_unlock(&task_cnt_lock);

    node_send = kmalloc(sizeof(struct k_list ), GFP_KERNEL); 
    node_send -> user_add_c = k_cmd.c;
    node_send -> k= k_cmd.k;
    node_send -> ker_mat_a= a;
    node_send -> ker_mat_b= b;
    node_send -> ker_mat_c= c;
    node_send->bench_pid= current->pid;
    node_send -> tsk_id= global_tsk_count; 

    mutex_lock(&send_lock);
    list_add_tail(&node_send -> task_queue, &task_head);
    mutex_lock(&sync_hashmap_lock);
    mutex_unlock(&send_lock);

    key_found=-1;
    list_for_each(ptr_sync, &sync_hashmap_head)
    {
            s_entry=list_entry(ptr_sync, struct sync_hash_map, sync_hash_list);

            //inc task count of same entry in queue
            if (s_entry->bench_pid == current->pid) 
            {
                key_found=1;
                s_entry->task_cnt= s_entry->task_cnt+1;
            }
    }

    if (key_found==-1)
    {
        s_entry = kmalloc(sizeof(struct sync_hash_map ), GFP_KERNEL); 
        s_entry->bench_pid= current->pid;
        s_entry->task_cnt= 1;
        list_add_tail( &s_entry -> sync_hash_list, &sync_hashmap_head);
    }
    mutex_unlock(&sync_hashmap_lock);

}

/*Queue implementation ends*/

/**
 * Enqueue a task for the caller/accelerator to perform computation.
 */
long blockmma_send_task(struct blockmma_cmd __user *user_cmd)
{

    struct blockmma_cmd user;
    float *mat_a, *mat_b, *mat_c;
    int res, row=0;

    res = copy_from_user(&user, user_cmd, sizeof(user));
    if(res != 0){
        printk("failed copying user_cmd into kern");
        printk("failed bytes: %d", (int)res);
    }

    mat_a = kmalloc(mat_size, GFP_KERNEL);
    mat_b = kmalloc(mat_size, GFP_KERNEL);
    mat_c = kmalloc(mat_size, GFP_KERNEL);


    for(row=0;row<128;row++)
    {   
        res = copy_from_user(mat_a+(row*128), (float *)user.a+(row*user.n), sizeof(float)*128);
        if(res != 0){
            printk("failed copying a into kern");
            printk("failed bytes: %d", (int)res);
        }

        res = copy_from_user(mat_b+(row*128), (float *)user.b+(row*user.k), sizeof(float)*128);
        if(res != 0){
            printk("failed copying b into kern");
            printk("failed bytes: %d", (int)res);
        }

        res = copy_from_user(mat_c+(row*128), (float *)user.c+(row*user.k), sizeof(float)*128);
        if(res != 0){
            printk("failed copying c into kern");
            printk("failed bytes: %d", (int)res);
        }
    }
    
    push_queue(user, mat_a, mat_b, mat_c);
    return 0;
}

/**
 * Return until all outstanding tasks for the calling process are done
 */
int blockmma_sync(struct blockmma_cmd __user *user_cmd)
{

    struct k_list *node_sync, *temp;
    struct sync_hash_map * s_node, *s_temp;
    int k, res, row=0;

    //iterate through sync_hash_map_queue and check task_count for your pid
    mutex_lock(&sync_hashmap_lock);
    list_for_each_entry_safe(s_node, s_temp, &sync_hashmap_head, sync_hash_list)
    {
        if ((s_node->bench_pid == current->pid) & (s_node->task_cnt == 0) )
        {

            //iterate through sync_queue -> copy to user -> release memory
            mutex_lock(&sync_lock);
            list_for_each_entry_safe(node_sync, temp, &sync_head, sync_queue)
            {
                if (node_sync->bench_pid == current->pid)
                {
                    row = 0;
                    k= node_sync->k;
                    for(row=0;row<128;row++){
                        res=copy_to_user(((float *)node_sync->user_add_c)+(row*k), ((float *)node_sync->ker_mat_c)+(row*128), sizeof(float)*128);
                        if(res != 0){
                            printk("failed copying result matrix c back to user! ");
                            printk("failed bytes: %d", (int)res);
                        }
                    }

                    kfree(node_sync->ker_mat_a);
                    kfree(node_sync->ker_mat_b);
                    kfree(node_sync->ker_mat_c);
                    list_del(&node_sync->sync_queue);
                    kfree(node_sync);

                }
            }
            list_del(&s_node->sync_hash_list);
            kfree(s_node);
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

int blockmma_get_task(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct blockmma_hardware_cmd hw_cmd;
    struct k_list *entry;
    int res;
    
    mutex_lock(&send_lock); 
    if (list_empty(&task_head)!=0) //non zero-> empty
    {
        //release lock task_queue
        mutex_unlock(&send_lock); 
        return -1;
    }
    mutex_unlock(&send_lock);

    res=copy_from_user(&hw_cmd, user_cmd, sizeof(hw_cmd));
    if(res != 0){
        printk("failed copying hw_cmd from acc to kernel !! ");
        printk("failed bytes: %d", (int)res);
    }

    //grab first task in queue
    mutex_lock(&send_lock);
    entry = list_first_entry(&task_head, struct k_list, task_queue);

    res=copy_to_user((float *)hw_cmd.a, (float *)entry->ker_mat_a, mat_size);
    if(res != 0){
        printk("failed copying a from kernel to acc !! ");
        printk("failed bytes: %d", (int)res);
        mutex_unlock(&send_lock);
        return -1;
    }
    res= copy_to_user((float *)hw_cmd.b, (float *)entry->ker_mat_b, mat_size);
    if(res != 0){
        printk("failed copying b from kernel into acc !! ");
        printk("failed bytes: %d", (int)res);
        mutex_unlock(&send_lock);
        return -1;
    }
    res= copy_to_user((float *)hw_cmd.c, (float *)entry->ker_mat_c, mat_size);
    if(res != 0){
        printk("failed copying c from kernel to acc !! ");
        printk("failed bytes: %d", (int)res);
        mutex_unlock(&send_lock);
        return -1;
    }

    //add the current node details to another queue and pop from current queue
    entry->hw_add_c = hw_cmd.c;
    entry->hw_pid = current->pid;
    mutex_lock(&comp_lock);
    list_add_tail(&entry -> comp_queue, &comp_head);
    mutex_unlock(&comp_lock);

    //remove from task_queue_list
    list_del(&entry->task_queue);
    mutex_unlock(&send_lock);
    return entry->tsk_id; 

}


/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct k_list *entry, *temp;
    struct sync_hash_map *s_entry;
    struct list_head  *ptr_sync;
    int res;

    mutex_lock(&comp_lock);
    list_for_each_entry_safe(entry, temp, &comp_head, comp_queue)
    {

        if (entry->hw_pid == current->pid)
        {
            
            //copy from hw to kernel
            res= copy_from_user(entry->ker_mat_c, (float *)entry->hw_add_c, mat_size);
            if(res != 0){
                printk("failed copying c from accelrator into kern !! ");
                printk("failed bytes: %d", (int)res);
            }

            //add task to sync_queue
            mutex_lock(&sync_lock);
            list_add_tail(&entry -> sync_queue, &sync_head); //adding to sync queue
            mutex_unlock(&sync_lock);

            list_del(&entry->comp_queue); //pop from comp_queue
            
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

            mutex_unlock(&comp_lock);
            return 0;
        }
    }   
    mutex_unlock(&comp_lock);
   return 0;
}

/*
 * Tell us who wrote the module
 */
int blockmma_author(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct blockmma_hardware_cmd cmd;
    char authors[] = "Krishna Kabi(kkabi004)"; 
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
