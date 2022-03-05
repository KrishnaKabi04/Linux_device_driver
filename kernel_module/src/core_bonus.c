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

bool debug= false;
extern struct miscdevice blockmma_dev;

struct mutex send_lock;
struct mutex comp_lock;
struct mutex sync_lock;
struct mutex sync_hashmap_lock;
struct mutex task_cnt_lock;
struct mutex bonus_lock;
struct mutex user_lock;

DEFINE_MUTEX(send_lock);
DEFINE_MUTEX(comp_lock);
DEFINE_MUTEX(sync_lock);
DEFINE_MUTEX(sync_hashmap_lock);
DEFINE_MUTEX(task_cnt_lock);
DEFINE_MUTEX(bonus_lock);
DEFINE_MUTEX(user_lock);

struct k_list {
    struct list_head task_queue; 
    struct list_head comp_queue;
    struct list_head sync_queue;
    struct list_head bonus_queue;
    struct list_head user_add_c_queue;

    __u64 user_add_c, hw_add_c;
    float *ker_mat_a, *ker_mat_b, *ker_mat_c; //kernel space matrices
    int bench_pid, hw_pid, tsk_id, k;

};

struct set_add_c{
    struct list_head set_c; 
    int bench_pid;
    float *ker_mat_c;
};

struct sync_hash_map{
    struct list_head sync_hash_list;
    int bench_pid;
    int task_cnt;
};

LIST_HEAD(task_head);  //submit task to this queue 
LIST_HEAD(comp_head); 
LIST_HEAD(sync_head);
LIST_HEAD(bonus_head);
LIST_HEAD(user_add_head);
LIST_HEAD(sync_hashmap_head);


static int global_tsk_count=0;
static size_t mat_size = (128 * 128)*sizeof(int);

void push_queue(struct blockmma_cmd k_cmd, float *a, float *b, float *c) {

    struct k_list *node_send, *node_check_c;
    struct sync_hash_map *s_entry;
    struct list_head  *ptr_send, *ptr_sync;

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
    //check if user_add_c matches with current user_add_c
    int add_c_found=-1;
    mutex_lock(&user_lock);
    list_for_each(ptr_send, &user_add_head)
    {
        node_check_c=list_entry(ptr_send, struct k_list, user_add_c_queue);
        printk("Push queue: Current tskid: %d ", node_check_c->tsk_id);

        if ( (node_check_c->bench_pid == node_send->bench_pid) & (node_check_c->user_add_c == k_cmd.c))
        {
            add_c_found=1;
            kfree(node_send -> ker_mat_c);
            printk("Task id: %d , user_add_c: %llu , node_ker_mat_c: %llu node_check_c ker_mat_c %llu \n", node_send->tsk_id, k_cmd.c, node_send -> ker_mat_c, node_check_c->ker_mat_c);
            node_send -> ker_mat_c = node_check_c->ker_mat_c;
            printk("Node at c addes: %llu \n", node_send -> ker_mat_c);
            break;
        }
    }
    
    list_add_tail(&node_send -> task_queue, &task_head);
    list_add_tail(&node_send -> user_add_c_queue, &user_add_head);
    mutex_unlock(&user_lock);

    mutex_lock(&sync_hashmap_lock);
    mutex_unlock(&send_lock);

    int key_found=-1;
    list_for_each(ptr_sync, &sync_hashmap_head)
    {
            s_entry=list_entry(ptr_sync, struct sync_hash_map, sync_hash_list);

            //inc count of same entry in queue
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

    //traverse the list
    if (debug){
        printk("Traversing the list \n");

        mutex_lock(&send_lock);
        list_for_each(ptr_send,&task_head)
        {
            node_send=list_entry(ptr_send, struct k_list, task_queue);  
            printk(KERN_INFO "task id:  %d   \n ", node_send->tsk_id);
            // printk(KERN_INFO "address of C in user space %llu , add of b in kernel space: %llu, value: %d %d \n ", node_send->user_add_c, node_send->ker_mat_b, *(node_send->ker_mat_b), *(node_send->ker_mat_b +1));
        
        }
        mutex_unlock(&send_lock);  
    }
    


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

    //printk("Process accessing: %s and PID is %i \n", current->comm, current->pid);
    copy_from_user(&user, user_cmd, sizeof(user)); 

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

    // if (debug)
    // {
    //     printk(KERN_INFO "\n address of a in user space %lld add of b: %lld add of c: %lld \n", user.a, user.b, user.c);
    //     printk("value of first value of matix B: b[0]= %d, b[1]= %d, b[2]= %d, b[126]= %d, b[127]= %d \n", *mat_b, *(mat_b+1), *(mat_b+2), *(mat_b+126), *(mat_b+127));
    //     printk("value of first value of matix C: c[0]= %d, c[1]= %d, c[2]= %d, c[126]= %d, c[127]= %d \n", *mat_c, *(mat_c+1), *(mat_c+2), *(mat_c+126), *(mat_c+127));
    // }
    
    push_queue(user, mat_a, mat_b, mat_c);
    printk("task pushed for pid %d \n", current->pid);

    return 0;
}

/**
 * Return until all outstanding tasks for the calling process are done
 */

int blockmma_sync(struct blockmma_cmd __user *user_cmd)
{

    struct k_list *node_sync, *temp, *node_user_c, *u_temp;
    struct sync_hash_map * s_node, *s_temp;
    struct set_add_c *set_node, *set_n, *set_temp;
    struct list_head *ptr;
    int k, res, row=0, key_found;
    

    //iterate through sync_hash_map_queue and check task_count for your pid:
    mutex_lock(&sync_hashmap_lock);
    list_for_each_entry_safe(s_node, s_temp, &sync_hashmap_head, sync_hash_list)
    {
        if ((s_node->bench_pid == current->pid) & (s_node->task_cnt == 0) )
        {
            LIST_HEAD(set_c_head);
            //iterate through sync_queue -> copy to user -> release memory
            mutex_lock(&sync_lock);
            list_for_each_entry_safe(node_sync, temp, &sync_head, sync_queue)
            {
                if (node_sync->bench_pid == current->pid)
                {
                    //printk(KERN_INFO "address of c  in user space in sync %lld,  kernel space: %lld ", node_sync->user_add_c, node_sync->ker_mat_c);
                    //printk("value of c in sync: c[0] %d  c[1] %d c[31] %d c[32] %d \n ",  *(node_sync->ker_mat_c), *(node_sync->ker_mat_c +1), *(node_sync->ker_mat_c+31), *(node_sync->ker_mat_c+32));
                    
                    row = 0;
                    k= node_sync->k;

                    for(row=0;row<128;row++){
                        res=copy_to_user(((float *)node_sync->user_add_c)+(row*k), ((float *)node_sync->ker_mat_c)+(row*128), sizeof(float)*128);
                        if(res != 0){
                            printk("failed copying result matrix c back to user! ");
                            printk("failed bytes: %d", (int)res);
                        }
                    }

                    //printk("Copied to user space ! \n");
                    kfree(node_sync->ker_mat_a);
                    kfree(node_sync->ker_mat_b);

                    key_found=-1;
                    list_for_each(ptr, &set_c_head)
                    {
                        set_n=list_entry(ptr, struct set_add_c, set_c);

                        if ((set_n -> bench_pid == node_sync->bench_pid) & (set_n->ker_mat_c == node_sync->ker_mat_c))
                        {
                            printk(" set_node found entry for tsk_id %d and ker_mat_c %llu \n ",  node_sync->tsk_id, node_sync->ker_mat_c );
                            key_found=1;
                            break;
                        }
                    }
                    if (key_found ==-1)
                    {
                        kfree(node_sync->ker_mat_c);

                        set_node = kmalloc(sizeof(struct set_add_c ), GFP_KERNEL); 
                        set_node->bench_pid= node_sync->bench_pid;
                        set_node->ker_mat_c= node_sync->ker_mat_c;
                        list_add_tail(&set_node -> set_c, &set_c_head);
                        
                    }
                    list_del(&node_sync->sync_queue);
                    kfree(node_sync);

                }
            }
            //delete for user_add_c
            mutex_lock(&user_lock);
            list_for_each_entry_safe(node_user_c, u_temp, &user_add_head, user_add_c_queue)
            {
                if (node_user_c->bench_pid == current->pid)
                {
                    list_del(&node_user_c->user_add_c_queue);
                }

            }
            //delete node for set of user_address_c 
            list_for_each_entry_safe(set_node, set_temp, &set_c_head, set_c)
            {
                if (set_node->bench_pid == current->pid)
                {
                    list_del(&set_node->set_c);
                    kfree(set_node);
                }

            }
            mutex_unlock(&user_lock);
            list_del(&s_node->sync_hash_list);
            kfree(s_node);

            mutex_unlock(&sync_lock);
            mutex_unlock(&sync_hashmap_lock);
            //printk(" Done sync ! \n");
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
    struct k_list *entry, *node_get, *temp, *node_check_c;
    struct list_head  *ptr_get, *ptr_comp, *bonus_ptr ;
    int res, key_found=-1;
    
    mutex_lock(&send_lock); 
    if (list_empty(&task_head)!=0) //non zero-> empty
    {
        //release lock task_queue
        mutex_unlock(&send_lock); 
        return -1;
    }
    mutex_unlock(&send_lock);


    printk("inside get task %d \n", current->pid);
    res=copy_from_user(&hw_cmd, user_cmd, sizeof(hw_cmd)); //handle error
    if(res != 0){
        printk("failed copying hw_cmd from acc to kernel !! ");
        printk("failed bytes: %d", (int)res);
    }

    mutex_lock(&send_lock);
    list_for_each_entry_safe(entry, temp, &task_head, task_queue)
    {
        mutex_lock(&bonus_lock);
        key_found=-1;
        list_for_each(bonus_ptr, &bonus_head)
        {
            node_check_c=list_entry(bonus_ptr, struct k_list, bonus_queue);
            
            if ((entry->bench_pid == node_check_c->bench_pid) &(entry->user_add_c ==  node_check_c->user_add_c) & (entry->ker_mat_c == node_check_c->ker_mat_c))
            {
                key_found=1;
            }

        }
        if (key_found==1) //found entry pick another task
        {

            mutex_unlock(&bonus_lock);
            continue;
        }
        else
        {   
            break;
        }

    }
    if (key_found==1){
        printk("Returning as bonus_queue has all user_c address for pid %d ! \n", current->pid);
        return -1;
    }

    printk("get_task entry:%d", current->pid);
    res=copy_to_user((float *)hw_cmd.a, (float *)entry->ker_mat_a, mat_size);
    if(res != 0){
        printk("failed copying a from kernel to acc !! ");
        printk("failed bytes: %d", (int)res);
        mutex_unlock(&bonus_lock);
        mutex_unlock(&send_lock);
        return -1;
    }
    res= copy_to_user((float *)hw_cmd.b, (float *)entry->ker_mat_b, mat_size);
    if(res != 0){
        printk("failed copying b from kernel into acc !! ");
        printk("failed bytes: %d", (int)res);
        mutex_unlock(&bonus_lock);
        mutex_unlock(&send_lock);
        return -1;
    }
    res= copy_to_user((float *)hw_cmd.c, (float *)entry->ker_mat_c, mat_size);
    if(res != 0){
        printk("failed copying c from kernel to acc !! ");
        printk("failed bytes: %d", (int)res);
        mutex_unlock(&bonus_lock);
        mutex_unlock(&send_lock);
        return -1;
    }
    printk("Copy to hardware success! \n");
    list_add_tail(&entry -> bonus_queue, &bonus_head);
    printk("Entry added for entry->tskid %d \n ", entry->tsk_id);

    //add the current node details to another queue and pop from current queue
    entry->hw_add_c = hw_cmd.c;
    entry->hw_pid = current->pid;
    mutex_lock(&comp_lock);
    list_add_tail(&entry -> comp_queue, &comp_head);
    mutex_unlock(&comp_lock);
    mutex_unlock(&bonus_lock);

    //remove from task_queue_list
    list_del(&entry->task_queue);
    //printk("Deleted entry from task queue in get task \n ");
    printk("get_task exit:%d", entry->tsk_id);
    mutex_unlock(&send_lock);
    return entry->tsk_id; 

}


/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct k_list *entry, *temp, *b_temp, *bonus_node;
    struct sync_hash_map *s_entry;
    struct list_head  *ptr_sync, *ptr_comp ;
    int res;

    //printk("Entering comp for pid %d", current->pid);

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
            // printk("mat C address in user space: %lld, in kernel space: %lld, mat_c values: %d %d %d %d \n", entry->hw_add_c, entry->ker_mat_c, *(entry->ker_mat_c), *(entry->ker_mat_c+1), *(entry->ker_mat_c+2), *(entry->ker_mat_c+126), *(entry->ker_mat_c+127));
            printk("Copied from hw space ! \n");

            mutex_lock(&bonus_lock);
            list_for_each_entry_safe(bonus_node, b_temp, &bonus_head, bonus_queue)
            {
            
                if ((entry->bench_pid == bonus_node->bench_pid) & (entry->user_add_c == bonus_node->user_add_c) & (entry->ker_mat_c == bonus_node->ker_mat_c))
                {
                    printk("Entry deleted for entry->tskid %d , user_addc: %llu kernel_add_c: %llu \n ", entry->tsk_id, entry->user_add_c, entry->ker_mat_c);
                    list_del(&bonus_node->bonus_queue);
                }

            }
            mutex_unlock(&bonus_lock);

            //add task to sync_queue
            mutex_lock(&sync_lock);
            list_add_tail(&entry -> sync_queue, &sync_head); //adding to sync queue
            mutex_unlock(&sync_lock);
            
            printk("mutex sync lock unloed in comp \n ");

            list_del(&entry->comp_queue); //pop from comp_queue
            printk("Deleted entry from comp queue in comp function \n ");
            
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
            printk("comp_task exit:%d", entry->tsk_id);
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
