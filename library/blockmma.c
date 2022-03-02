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
//     APIs of CSE202's Emulated MMA Accelerators in User Space
//
////////////////////////////////////////////////////////////////////////
#include "blockmma.h"

// This function submit tasks to the kernel module.
int blockmma(int devfd, int *a, int *b, int *c, int M, int N, int K)
{
  int i, j, k;  

  for(i = 0; i < M; i+=128)
  {
    for(j = 0; j < N; j+=128)
    {
      for(k = 0; k < K; k+=128)
      {

        blockmma_f128(devfd, &a[i*N+j], &b[j*K+k], &c[i*K+k], M, N, K, 128); //0 0 0 
                                                                              //0 128 [0 128 256 ...]   : queue : FIFO workqueue, tasklet 
      }
      blockmma_sync(devfd); //copy data back to user-space ? free memory
    }
  }  

  return 0;
}

int blockmma_bonus(int devfd, int *a, int *b, int *c, int M, int N, int K)
{
  int i, j, k;
  for(i = 0; i < M; i+=128)
  {
    for(j = 0; j < N; j+=128)
    {
      for(k = 0; k < K; k+=128)
      {
        blockmma_f128(devfd, &a[i*N+j], &b[j*K+k], &c[i*K+k], M, N, K, 128);
      }
    }
  }  
  blockmma_sync(devfd);
 
  return 0;
}

int blockmma_f128(int devfd, int *a, int *b, int *c, int m, int n, int k, int tile)
{
    
    printf("----------- blockmma_f128 --------- \n");
    /*
    printf("----------- matrix A --------- \n");
    printf("value of a[0]: %d \n", a[0]);
    printf("val of a[1]: %d \n", *(a+1));
    printf("value of a[128]: %d \n", *(a+128));
    printf("value of a[M]: %d \n", *(a+tile));

    printf("address of a[0]: %lld \n", &a[0]);
    printf("address of a[1]: %lld \n", &a[1]);
    printf("address of a[tile]: %lld \n", &a[tile]); */

    printf("----------- matrix B --------- \n");
    printf("value of b[0]: %d, b[1]= %d, b[126]= %d, b[127]= %d, b[tile]= %d \n", *b, *(b+1), *(b+126), *(b+127), *(b+tile));
    printf("value of b[0]: %d, b[1]= %d, b[126]= %d, b[127]= %d, b[tile]= %d \n", b[0], b[1], b[126], b[127], b[tile]);
    printf(" b[127-127]: %d  %d \n", *(b + 127*128+127), b[127*128+127]);
    printf("address of b[0]: %lld b[1]: %lld, b[127]: %lld, b[tile]: %lld \n", &b[0], &b[1], &b[127], &b[tile]);

    printf("----------- matrix C --------- \n");
    printf("value of c[0]: %d, c[1]= %d, c[126]= %d, c[127]= %d, c[tile]= %d \n", *c, *(c+1), *(c+126), *(c+127), *(c+tile));
    printf("address of c[0]: %lld c[1]: %lld c[126]: %lld c[127]: %lld \n", &c[0], &c[1], &c[126], &c[127]);


    struct blockmma_cmd cmd;
    cmd.op = (__u64)0;
    cmd.a = (__u64)a;
    cmd.b = (__u64)b;
    cmd.c = (__u64)c;
    cmd.m = (__u64)m; //256*256
    cmd.n = (__u64)n; //256*256
    cmd.k = (__u64)k; //256*256
    cmd.tile = (__u64)tile;

    /*
    int *y= (void *)cmd.b;

    printf("cmd b: %lld y[0] %d y[1] %d \n", y, *y, *(y+1));
    y= y + 128;
    printf("cmd b: %lld y[0] %d y[1] %d \n", y, *y, *(y+1));
    y= y + 128;
    printf("cmd b: %lld y[0] %d y[1] %d \n", y, *y, *(y+1));
    */

    printf("Adding task to send queue \n");
    while(ioctl(devfd, BLOCKMMA_IOCTL_SEND_TASK, &cmd)== -1); // the ioctl func needs to return an exit code to break loop on success
    return 0;
}

// This function will wait until all updates are presented to the userspace from 
// the kernel module.
int blockmma_sync(int devfd)
{
    struct blockmma_cmd cmd;
    cmd.op = (__u64)0;
    while(ioctl(devfd, BLOCKMMA_IOCTL_SYNC, &cmd) == -1); 
    return 0;
}

void sigquit();
int counter=0;
// This is the function emulating the effect of hardware accelerated 128x128 
// matrix multiplications.
int blockmma_f128_accelerator(int devfd)
{
    struct blockmma_hardware_cmd cmd;
    int * a, *b, *c;
    int i, j, k;
    int tid;
    a = (int *)malloc(128*128*sizeof(int)); //VMem copy_to__user
    b = (int *)malloc(128*128*sizeof(int)); //VMem
    c = (int *)malloc(128*128*sizeof(int)); //VMem
    cmd.op = (__u64)0;
    cmd.a = (__u64)a;
    cmd.b = (__u64)b;
    cmd.c = (__u64)c;
    signal(SIGQUIT, sigquit); //terminate a  process
    //int ctr=1;

    printf("------------------ accelerator---------------\n");

    while(1)
    {
        if((tid=ioctl(devfd, BLOCKMMA_IOCTL_GET_TASK, &cmd))>=0) 
        {

            //printf("fetch value of matrix b: %d, %d, %d, %d \n", b[0], b[1], b[2], b[127]);
            for(i = 0; i < 128; i++)
                for(j = 0; j < 128; j++)
                    for(k = 0; k < 128; k++)
                        c[i*128+j] += a[i*128+k]*b[k*128+j]; //acc
            cmd.tid = tid;
            //printf("Copy to user var : %d \n", cmd.tid);
            printf("fetch value of matrix c: c[0]: %d, c[1]: %d, c[2]: %d, c[32]: %d , c[126]: %d, c[127]: %d", c[0], c[1], c[2], c[32], c[126], c[127]);
            printf(" c[127x127]: %d  %d \n", *(c + 127*128+127), c[127*128+127]);
            // (__u64)a[0]
            //b[2]= 123;
            //b[3]=124;
            ioctl(devfd, BLOCKMMA_IOCTL_COMP, &cmd);
            counter++;

        }
    }
    exit(1);
    return 0;
}

void sigquit()
{
    printf("Accelerator %d has performed %d tasks\n", getpid(), counter);
    signal(SIGQUIT, sigquit);
    exit(0);
}