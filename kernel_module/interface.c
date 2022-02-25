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
//     User APIs of CSE202's Block MMA Accelerator Project in User Space
//
////////////////////////////////////////////////////////////////////////

#include "blockmma.h"

//#include <asm/uaccess.h>
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

extern long blockmma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
//extern long my_ioctl(struct file *file, unsigned cmd, unsigned long arg);

//extern int blockmma_mmap(struct file *filp, struct vm_area_struct *vma);
extern int blockmma_init(void);
extern void blockmma_exit(void);


static const struct file_operations blockmma_fops = {
    .owner = THIS_MODULE, //This field is used to prevent the module from being unloaded while its operations are in use.
    .unlocked_ioctl = blockmma_ioctl, //to get device specific commands. 
                                     // This field is used to prevent the module from being unloaded while its operations are in use.

//    .mmap = blockmma_mmap,
};

struct miscdevice blockmma_dev = {
    .minor = MISC_DYNAMIC_MINOR, //dynamic creation
    .name = "blockmma",
    .fops = &blockmma_fops,
    // struct miscdevice *next, *prev;
};

static int __init blockmma_module_init(void)
{
    printk("Device file blockmma initialized");
    return blockmma_init();
}

static void __exit blockmma_module_exit(void)
{
    printk("Device file blockmma exiting");
    blockmma_exit();
}


MODULE_AUTHOR("Hung-Wei Tseng <htseng@ucr.edu>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(blockmma_module_init);
module_exit(blockmma_module_exit);
