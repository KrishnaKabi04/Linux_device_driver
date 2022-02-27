//////////////////////////////////////////////////////////////////////
//                     University of California, Riverside
//
//
//
//                             Copyright 2021
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
//     Running Applications on Resource Containers
//
////////////////////////////////////////////////////////////////////////

#include <blockmma.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>
       #include <malloc.h>
#include <sys/syscall.h>
#include <sys/prctl.h> // prctl(), PR_SET_PDEATHSIG
#include <signal.h> // signals

int blockmm(int *a, int *b, int *c, int m, int n, int k);


int main(int argc, char *argv[])
{
    // variable initialization
    int i, j, k;
    int *a,*b, *c;
    int *validate_a, *validate_b, *validate_c;
    unsigned long long msec_time;
    FILE *fp;
    int devfd, child_pid;
    struct timeval current_time;
    time_t t;
    int ARRAY_SIZE = 256;
    int pagesize = sysconf(_SC_PAGE_SIZE);
    printf("Page size is : %d \n", pagesize);
    pid_t ppid_before_fork = getpid();
    // takes arguments from command line interface.
    devfd = open("/dev/blockmma", O_RDWR);
    if (argc > 1)
    {
      ARRAY_SIZE = atoi(argv[1]);
    }
    if (devfd < 0)
    {
        fprintf(stderr, "Device open failed");
        exit(1);
    }
  srand((unsigned) time(&t));
  a = (int *)memalign(pagesize, (ARRAY_SIZE*ARRAY_SIZE*sizeof(int *)/pagesize) * pagesize + 1);
  for(i = 0; i < ARRAY_SIZE*ARRAY_SIZE; i++)
  {
      a[i] = (i+1)*2; 
  }
  b = (int *)memalign(pagesize, (ARRAY_SIZE*ARRAY_SIZE*sizeof(int *)/pagesize) * pagesize + 1);
  for(i = 0; i < ARRAY_SIZE*ARRAY_SIZE; i++)
  {
      b[i] = (i+1)*3; //rand();
  }
  c = (int *)memalign(pagesize, (ARRAY_SIZE*ARRAY_SIZE*sizeof(int *)/pagesize) * pagesize + 1);
  for(i = 0; i < ARRAY_SIZE*ARRAY_SIZE; i++)
  {
      c[i] = 0;
  }
  validate_a = (int *)malloc(ARRAY_SIZE*ARRAY_SIZE*sizeof(int *));
  validate_b = (int *)malloc(ARRAY_SIZE*ARRAY_SIZE*sizeof(int *));
  validate_c = (int *)malloc(ARRAY_SIZE*ARRAY_SIZE*sizeof(int *));
  memcpy(validate_a, a, ARRAY_SIZE*ARRAY_SIZE*sizeof(int *));
  memcpy(validate_b, b, ARRAY_SIZE*ARRAY_SIZE*sizeof(int *));
  memcpy(validate_c, c, ARRAY_SIZE*ARRAY_SIZE*sizeof(int *));
  
  printf("KK a[0]: %d \n", a[0]);
  printf("KK a[1]: %d \n", a[1]);
    printf("KK a[5]: %d \n", a[5]);
  printf("KK address of a[0]: %lld , pointer: %p \n", &a[0],  &a[0]);
  printf("KK: add a[1] %lld , pointer: %p \n", &a[1], &a[1]);
  printf("KK: add of a  %lld, %p, value of a: %d \n", &a, &a, *a);

  printf("KK address of b[0]: %lld,  %p,  0x%x \n", &b[0], &b[0], &b[0]);
  printf("KK address of b[1]: %lld,  %p,  0x%x \n", &b[1], &b[1], &b[1]);
  printf("KK value of b[0]: %d, b[1]: %d b[2]: %d b[3]: %d b[4]:  %d \n", b[0], b[1], b[2], b[3], b[4]);
  
  // Accelerated BLOCKMM
  blockmma(devfd, a, b, c, ARRAY_SIZE, ARRAY_SIZE, ARRAY_SIZE);

  printf("Accelartor computation completed \n");
  printf("KK value of b[0]: %d, b[1]: %d b[2]: %d b[3]: %d b[4]:  %d \n", b[0], b[1], b[2], b[3], b[4]);


  // CPU BLOCKMM
  blockmm(validate_a, validate_b, validate_c, ARRAY_SIZE, ARRAY_SIZE, ARRAY_SIZE);
  for(i = 0; i < ARRAY_SIZE; i++)
  {
    for(j = 0; j < ARRAY_SIZE; j++)
    {
      if(c[i*ARRAY_SIZE + j] != validate_c[i*ARRAY_SIZE + j])
      {
          printf("Incorrect Result: %d, %d, %d %d \n",i, j, c[i*ARRAY_SIZE + j], validate_c[i*ARRAY_SIZE + j]);
          exit(1);
      }
    }
  }
  printf("Passed\n");
  exit(1);
    return 0;
}

//CPU call
int blockmm(int *a, int *b, int *c, int M, int N, int K)
{
  int i,j,k, ii, jj, kk;
  int chunk_size = 128;
  
  for(i = 0; i < M; i+=chunk_size)
  {
    for(j = 0; j < N; j+=chunk_size)
    {
      for(k = 0; k < K; k+=chunk_size)
      {        
          for(ii = i; ii < i+chunk_size; ii++)
            for(jj = j; jj < j+chunk_size; jj++)
              for(kk = k; kk < k+chunk_size; kk++)
                c[ii*M+jj] += a[ii*M+kk]*b[kk*K+jj];
      }
    }
  }  
  return 0;
}
