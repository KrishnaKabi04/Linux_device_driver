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
  printf("------- Blockmma -------- \n");
  printf("value of a[0]: %d \n", a[0]);
  printf("val of a[1]: %d \n", *(a+1));
  printf("val of a[M]: %d \n", *(a+M));
  printf("adress of a[M]: %d \n", &a[M]);

  printf("value of b[0]: %f \n", b[0]);
  printf("val of b[1]: %f \n", *(b+1));
  printf("val of b[N]: %f \n", *(b+N));
  printf("address of b[N]: %lld \n",&b[N]);

  printf("value of c[0]: %d \n", c[0]);
  printf("val of c[1]: %d \n ", *(c+1));
  printf("val of c[K]: %d \n", *(c+K));
  printf("address of c[K]: %lld \n \n",&c[K]);


  for(i = 0; i < M; i+=128)
  {
    for(j = 0; j < N; j+=128)
    {
      for(k = 0; k < K; k+=128)
      {
        printf("i= %d, j= %d, k= %d \n \n", i,j,k);
        blockmma_f128(devfd, &a[i*N+j], &b[j*K+k], &c[i*K+k], M, N, K, 128); //0 0 0 
                                                                              //0 128 [0 128 256 ...]   : queue : FIFO workqueue, tasklet 
        
      }
      blockmma_sync(devfd); //copy data back to user-space ? free memory
      return 0;
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
    printf("----------- matrix A --------- \n");
    printf("value of a[0]: %d \n", a[0]);
    printf("val of a[1]: %d \n", *(a+1));
    printf("value of a[128]: %d \n", *(a+128));
    printf("value of a[M]: %d \n", *(a+tile));

    printf("address of a[0]: %lld \n", &a[0]);
    printf("address of a[1]: %lld \n", &a[1]);
    printf("address of a[tile]: %lld \n", &a[tile]);

    printf("----------- matrix B --------- \n");
    printf("value of b[0]: %d \n", b[0]);
    printf("val of b[1]: %d \n", *(b+1));
    printf("value of b[128]: %d \n", *(b+128));
    printf("value of b[N]: %d \n", *(b+tile));

    printf("address of b[0]: %lld \n", &b[0]);
    printf("address of b[1]: %lld \n", &b[1]);
    printf("address of b[tile]: %lld \n", &b[tile]);

    printf("----------- matrix C --------- \n");
    printf("value of c[0]: %d \n", c[0]);
    printf("val of c[1]: %d \n", *(c+1));
    printf("value of c[128]: %d \n", *(c+128));
    printf("value of c[K]: %d \n", *(c+tile));

    printf("address of c[0]: %lld \n", &c[0]);
    printf("address of c[1]: %lld \n", &c[1]);
    printf("address of c[tile]: %lld \n", &c[tile]);

    struct blockmma_cmd cmd;
    cmd.op = (__u64)0;
    cmd.a = (__u64)a;
    cmd.b = (__u64)b;
    cmd.c = (__u64)c;
    cmd.m = (__u64)m; //256*256
    cmd.n = (__u64)n; //256*256
    cmd.k = (__u64)k; //256*256
    cmd.tile = (__u64)tile;

    int *x= (void *)cmd.a;
    int *y= (void *)cmd.b;
    int *z= (void *)cmd.c;

    printf("cmd a: %lld x[0] %d x[1] %d, x[128]: %d, x[M]:  %d \n", cmd.a, *x, *(x+1), *(x+128), *(x+tile));
    printf("cmd b: %lld y[0] %d y[1]%d, y[128]:%d, y[M]:  %d \n", cmd.b, *y, *(y+1), *(y+128), *(y+tile));
    printf("cmd c: %lld z[0] %d z[1] %d, z[128]: %d, z[M]:  %d \n \n ", cmd.c, *z, *(z+1), *(z+128), *(z+tile));

    //printf("size of a: %ld \n", sizeof(cmd.a));
    //printf("size of a: %ld \n", sizeof(a));

    while(ioctl(devfd, BLOCKMMA_IOCTL_SEND_TASK, &cmd)== -1); // the ioctl func needs to return an exit code to break loop on success
    printf(" break loop in blockmma_f128");
    return 0;
}

// This function will wait until all updates are presented to the userspace from 
// the kernel module.
int blockmma_sync(int devfd)
{
    struct blockmma_cmd cmd;
    cmd.op = (__u64)0;
    while(ioctl(devfd, BLOCKMMA_IOCTL_SYNC, &cmd) == -1); //break loop
    return 0;
}

int counter = 0;
void sigquit();

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

    while(1)
    {
        if((tid=ioctl(devfd, BLOCKMMA_IOCTL_GET_TASK, &cmd))>=0) //makes sure data is mapped to .. queue2 : data from kernel
        {
            
            printf("fetch value of matrix b: %d, %d, %d, %d ", b[0], b[1], b[2], b[127]);
            for(i = 0; i < 128; i++)
                for(j = 0; j < 128; j++)
                    for(k = 0; k < 128; k++)
                        c[i*128+j] += a[i*128+k]*b[k*128+j]; //acc
            cmd.tid = tid;
            printf("Copy to user var : %d \n", cmd.tid);
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