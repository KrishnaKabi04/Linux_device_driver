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
      a[i] = i; //rand()
  }
  b = (int *)memalign(pagesize, (ARRAY_SIZE*ARRAY_SIZE*sizeof(int *)/pagesize) * pagesize + 1);
  for(i = 0; i < ARRAY_SIZE*ARRAY_SIZE; i++)
  {
      b[i] = i; //rand();
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
  

  printf("KK value of a[0]: %d, a[1]: %d a[2]: %d a[126]: %d a[127]:  %d \n", a[0], a[1], a[2], a[126], a[127]);
  printf("value of first value of matix B: b= %d,  b[1]= %d, b[126]= %d, b[127]=%d b[128]= %d \n", *b, *(b+1), *(b+126), *(b+127), *(b+128));
  printf(" b[127-127]: %d  %d \n", *(b + 127*128+127), b[127*128+127]);
  printf("KK value of b[0]: %d, b[1]: %d b[2]: %d b[126]: %d b[127]:  %d, B[126][0]: %d b[127][0]: %d \n", b[0], b[1], b[2], b[126], b[127], b[126*128+0], b[127*128+0]);
  

  // Accelerated BLOCKMM
  blockmma(devfd, a, b, c, ARRAY_SIZE, ARRAY_SIZE, ARRAY_SIZE);

  printf("Accelartor computation completed \n");
  printf("KK value of C: c[0]: %d, c[1]: %d c[2]: %d c[3]: %d c[127]:  %d \n", c[0], c[1], c[2], c[3], c[127]);



  // CPU BLOCKMM
  blockmm(validate_a, validate_b, validate_c, ARRAY_SIZE, ARRAY_SIZE, ARRAY_SIZE);
  printf("KK value of validate_c c[0]: %d, c[1]: %d c[2]: %d c[3]: %d c[127]:  %d \n", validate_c[0], validate_c[1], validate_c[2], validate_c[3], validate_c[127]);


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
