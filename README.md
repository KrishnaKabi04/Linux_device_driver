# CS202 (2022 Winter) Project: Interacting with "emulated" hardware accelerators

## Overview

Hardware accelerators that implement specialized circuits for compute-intensive algorithms are ubiquitous in modern computers. Famous examples includes Google's Tensor Processing Units and Apple's Nueral Engine. Instead of exposing internal instructions to the rest of the world, these accelerators typically reveal the ``functions'' that they implement and allow users to offload the whole compute kernel through a domain-specific function call. However, as you can imagine, lots of work need to be done to bridge the exposed hardware/software interface to the interface presented to the user.

In this project, you will be playing a role as an engineer with a partner engineer you choose to develop the system software for a hardware accelerator that your start-up company, Universal Chip of Riverside, is going to produce. The accelerator is a set of matrix multiplication units (MMUs) that can perform multiple 128 * 128 * 128 matrix multiplications simultaneously. But as you could imagine, the hardware team is always falling behind and we cannot wait to start our development! Therefore, we have developed an emulated hardware accelerators, essentially a user program performing 128 * 128 * 128 matrix multiplications. With multiple processes of this program running in your system, your system now have multiple emulated MMUs!

UCR also plans to present an API function -- blockmma for user programs. The programmer using this API simply needs to specify which the pointer to three distinct floating-point matrices (two as inputs and one as the result) and the M, N, Ks of these three matrices. The library function (already provided by the language team of UCR) will partition the given matrices into triplets of 128 * 128 submatrices from the three matrices. 

Now, the problem is back to you -- how to provide the appropriate system software support for these accelerators. You will need to implement a Linux kernel module, working as the device driver of the backend accelerators. The device driver receives commands from the user-program/library that are essentailly MMA requests broken down into 128 * 128 * 128 ones and a synchronization request that reflects all local updates on the accelerators to the user program. However, there are a few challenges here: 

(1) Data exchange/update between different virtual address spaces -- remember accelerators are emulated as separate processes, they cannot see the memory content of other user applications directly.

(2) The memory layout -- the user program stores matrices in row-major order that each row is sized as N or K. However, your accelerator only accepts/layout matrices in 128 elements per row.

(3) Task management -- we have multiple MMUs running at the backend. Can you evenly distribute tasks to each? Can you make sure each task is exactly executed only once? Can you distinguish requests from different user processes and distribute results to the correct ones?

(4) Supporting multi processes -- you will have multiple user processes running simultaneously. You have to make sure none of them gets stuck.

In this project, you will be given the prototype of the kernel module with a core.c file in its source directory that only contains empty functions. You're only allowed to turn in and modify this file (as you are working in the kernel module team). We also provide a user-space library that allows an application to interact with this kernel module through the ioctl interface as well as a sample benchmark application that you may extend to test if your kernel module functions correctly.

You are strongly encouraged to work in a group of 2 but not to exceed 3. Groups do the same project as individuals. Both members will receive the same grade. Note that working in groups may or may not make the project easier, depending on how the group interactions work out. If collaboration issues arise, contact your instructor as soon as possible: flexibility in dealing with such issues decreases as the deadline approaches.

## Objective

- Learning UNIX/Linux kernel programming as well as the constraints
- Learning UNIX/Linux system process scheduling
- Learning UNIX/Linux kernel modules
- Learning UNIX/Linux system memory management
- Learning the memory layout of the C programming language

## How to start 

Linux OS: Ubuntu 20.04 (https://ubuntu.com/download/desktop)
Your virtual machine should at least have 4GB of physical memory and 2 processors. 

Packages required: make, git

This module has 3 sub-modules:

1. kernel_module -- the directory where we have the kernel module code.

2. library -- the directory of the user-space library code.

3. benchmark -- the directory with a sample program using this kernel.

To compile the kernel module in the kernel_module directory: 
``sudo make install``
to install headers and the module in the right place. You should be able to find a blockmma.ko if your compilation success and this is the binary of the kernel module. 

However, this kernel module isn't in your kernel yet. To get this kernel module loaded into the system kernel, try 
``sudo insmod blockmma.ko``.
Upon success, you should find an "blockmma" device file under /dev directory in your linux system. By default, this device may not be available for non-root users. Therefore, you need to use 
``sudo chmod 777 /dev/blockmma`` command to make it accessible by anyone and any process in the system. 

If you don't want this device to be available in the system anymore, you can use ``sudo rmmod blockmma`` to remove this device.

Now, you can navigate to the library path and again use ``make`` to generate this dynamic link library. You need to then use "sudo make install" to make this library publicly available for the system. You should read the code and figure out how this library interacts with the kernel module. 


## Testing

Finally, in the benchmark directory, the benchmark program can be compiled using ``make`` command. The run.sh script should be used ("source ./run.sh") in the benchmark folder to test and validate implementation. 

After succesfully making the files, the benchmark will contain four user programs:

(1) benchmark -- the user program that calls the blockmma API function. The argument of the program specifies the size of the sqaured matrices.

(2) accelerators -- a server program receiving arguments specifying the number of accelerators to emulate. The children of these programs are accelerators that keep retrieving requests from your kernel module and perform the tasks. The server program also launches sockets as a route to receive control signals from other user programs. Once it receives a "quit" command from the socket, it will kill all accelerators and each accelerator will report the number of tasks it performed during this period.

(3) accelerators_control -- a user program that simply acts as the sender of commands to accelerators.

(4) benchmark_bonus -- the user program that calls the blockmma_bonus API function -- a more efficient function call that only synchronization results once.

For each run, the script will output the result of each running program. If you see the program output contains "Diff", it means your result is incorrect. If your matrix output is identical to CPU computation, you will see a "Passed" in the output. The accelerators will also report tasks each performed. You should expect each accelerator performs similar amount of tasks. 


## Modules in Core:

1. Implementing the blockmma kernel module: it needs the following features:

- blockmma_send_task: This operation allows a user program to send memory pointers describing three matrices, their sizes and store them appropriately before the task is finished. These blockmma_send_task requests are invoked by the user-space library using BLOCKMMA_IOCTL_SEND_TASK in the ioctl interface. The ioctl system call will be redirected to blockmma_send_task function located in src/ioctl.c. Upon success, the function returns a task id. Otherwise, returns -1.

- blockmma_get_task: This operation allows an accelerator to retrieve a task from your kernel module. The accelerator sends three pointers describing three 128 by 128 matrices. Your kernel module needs to let those content visible to the requesting accelerator. These blockmma_get_task requests are invoked by the user-space library using BLOCKMMA_IOCTL_GET_TASK in the ioctl interface. The ioctl system call will be redirected to blockmma_get_task function located in src/core.c. Upon success, the function returns a task id. Otherwise, returns -1;


- blockmma_comp: This operation allows an accelerator to report/update a task to your kernel module. The accelerator sends three pointers describing three 128 by 128 matrices being used for computation and a task id your previously returned for update. Your kernel module needs to grab the accelerator's result -- the c matrix and store it somewhere before the user program originated the task asking for synchronization. These blockmma_comp requests are invoked by the user-space library using BLOCKMMA_IOCTL_COMP in the ioctl interface. The ioctl system call will be redirected to blockmma_comp function located in src/core.c. Upon success, the function returns a task id. Otherwise, returns -1;

- blockmma_sync: This operation allows a user program to collect all outstanding requests. The function should places all computation results from accelerators to their original locations in the user program. These blockmma_sync requests are invoked by the user-space library using BLOCKMMA_IOCTL_SYNC in the ioctl interface. The ioctl system call will be redirected to blockmma_sync function located in src/core.c. If all outstanding tasks are updated, the function returns 0. Otherwise, returns -1;

