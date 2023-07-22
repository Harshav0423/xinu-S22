#include <xinu.h>
#include <shprototypes.h>
#include <stdlib.h>
#include <ctype.h>
#include <run.h>
#include <prodcons_bb.h>
#include <future.h>
#include <future_prodcons.h>
#include <stream.h>
#include <fs.h>

// Assingment-9 phase 1
uint fstest(int nargs, char *args[]);
// joining process
sid32 other_can_join;

// definition of array, semaphores and indices
int arr_q[5];
sid32 prod_bb_can_write;
sid32 prod_bb_can_read;
sid32 prod_bb_can_end;

// assignment-4
int head;
int tail;
int gb_count;

// assignment-5
sid32 print_sem;

// // Assignment-6
// int future_fib(int nargs, char *args[]);
// int future_free_test(int nargs, char *args[]);

// assignment-5
int is_number(char* ar) {
  for (int i =0; i< strlen(ar); i++) {
    if(isdigit(ar[i]) <=0) {
      return -1;
    }
  }
  return 1;
 }
void future_prodcons(int nargs, char *args[]) {

 

  // First, try to iterate through the arguments and make sure they are all valid based on the requirements
  // (you should not assume that the argument after "s" is always a number)
  int i = 2;
  // assignment-6
  if(strcmp(args[1], "--free") == 0) {
    resume(create(future_free_test, 2048, 20, "fprodtest", 2, nargs, args ));
  }
  else if (strcmp(args[1], "-f") == 0) {
    resume(create(future_fib, 2048, 20, "fprodfib", 2, nargs, args ));
  }

  else if (strcmp(args[1], "-pc") == 0 && nargs > 3) {
      
   
  while (i < nargs) {
    // TODO: write your code here to check the validity of arguments
    if ((strcmp(args[i], "g") != 0) && (strcmp(args[i], "s") != 0) && (is_number(args[i])==-1)){
              signal(other_can_join);
              printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
              return (0);
          }
        else if ( (strcmp(args[i], "g") == 0) && (is_number(args[i+1])==1)){
            signal(other_can_join);
            printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
            return (0);
        }
        else if ( (strcmp(args[i], "s") == 0) && (is_number(args[i+1])==-1) )
        {
          signal(other_can_join);
             printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
            return (0);
        }
        else if ( (is_number(args[i])==1) && (strcmp(args[i-1], "s") != 0) ){
            signal(other_can_join);
            printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
            return (0);
        }
    i++;
    }

  
  }
   else if(strcmp(args[1], "-pcq") == 0) {
    if (nargs <= 3) {
      printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
      return(0);
    }
    int i = 3; 
    while (i < nargs) {
      if (strcmp(args[i], "g") != 0 && strcmp(args[i], "s") != 0 && is_number(args[i]) != 1 ){
        printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
        return(0);
      }
      if (strcmp(args[i], "s") == 0 && is_number(args[i+1]) != 1) {
        printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
        return(0);
      }
      
      if (is_number(args[i]) == 1 && strcmp(args[i-1], "s") != 0){
        printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
        return(0);
      }
      i++;
    }
  }

  else {
    printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
  }
  print_sem = semcreate(1);
  
  char *val;
  future_t* f_exclusive;
  f_exclusive = future_alloc(strcmp(args[1], "-pcq") == 0 ? FUTURE_QUEUE : FUTURE_EXCLUSIVE, sizeof(int), 1);
  if(strcmp(args[1], "-pcq") == 0) {
    if (is_number(args[2]) != 1) {
      printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
      signal(other_can_join);
        return;
    }
		f_exclusive = future_alloc(FUTURE_QUEUE, sizeof(int), atoi(args[2]));
	} else {
		f_exclusive = future_alloc(FUTURE_EXCLUSIVE, sizeof(int), 1);
	}
  int num_args = i;  // keeping number of args to create the array
  i = 2; // reseting the index
  val  =  (char *) getmem(num_args); // initializing the array to keep the "s" numbers

  // Iterate again through the arguments and create the following processes based on the passed argument ("g" or "s VALUE")
  while (i < nargs) {
    if (strcmp(args[i], "g") == 0) {
      char id[10];
      sprintf(id, "fcons%d",i);
      resume(create(future_cons, 2048, 20, id, 1, f_exclusive));
    }
    if (strcmp(args[i], "s") == 0) {
      i++;
      uint8 number = atoi(args[i]);
      val[i] = number;
      resume(create(future_prod, 2048, 20, "fprod1", 2, f_exclusive, &val[i]));
      sleepms(5);
    }
    i++;
  }
  sleepms(100);
  future_free(f_exclusive);
  // signal(other_can_join);
  return;
}


void prodcons_bb(int nargs, char *args[]) {
  //create and initialize semaphores to necessary values
    prod_bb_can_write = semcreate(1);
    prod_bb_can_read = semcreate(0);
  //initialize read and write indices for the queue
    head = 0;
    tail = 0;
  //create producer and consumer processes and put them in ready queue
    int prod_count = atoi(args[1]);
    int cons_count = atoi(args[2]);
    int prod_iter = atoi(args[3]);
    int cons_iter = atoi(args[4]);
    
    // initialising global count
    gb_count = prod_count * prod_iter;

    for(int i = 0; i < prod_count; i++) {
        resume(create(producer_bb, 1024, 20, "producer", 2, i, prod_iter));
    }
    int j = 0;
    for(j = 0; j < cons_count; j++) {
        resume(create(consumer_bb, 1024, 20, "consumer", 2, j, cons_iter));
    }
    wait(prod_bb_can_end);
    
    signal(other_can_join);
    return;
}
shellcmd xsh_run(int nargs, char *args[]) {
    other_can_join = semcreate(0);
    // prodcon_can_join = semcreate(0);
    // Print list of available functions
    if ((nargs == 1) || (strncmp(args[1], "list", 4) == 0)) {
    printf("hello\n");
    printf("list\n");
    printf("prodcons\n");
    printf("prodcons_bb\n");
    printf("futest\n");
    printf("tscdf\n");
    // printf("tscdf_fq\n");
    // printf("fstest\n");
    return OK;
    }

    /* This will go past "run" and pass the function/process name and its arguments */
    args++;
    nargs--;

    if(strncmp(args[0], "hello", 5) == 0) {
    /* create a process with the function as an entry point. */
    //resume (create((void *) my_function_1, 4096, 20, "my_func_1", 2, nargs, args));
        if(nargs == 2) {
            resume (create(xsh_hello, 4096, 20, "hello", 2, nargs, args));
            wait(other_can_join);
        }

        else{
            printf("Syntax: run hello name\n");
        }
    }
    else if(strncmp(args[0], "prodcons_bb",11) == 0) {
        if (nargs == 5) {
            if (atoi(args[1]) * atoi(args[3]) == atoi(args[2]) * atoi(args[4])) {
                resume(create(prodcons_bb, 4096, 20, "prodcons_bb", 2, nargs, args));
                wait(other_can_join);
            }
            else {
                printf("Iteration Mismatch Error: the number of producer(s) iteration does not match the consumer(s) iteration\n");
                return 0;
            }
        }
        else {
            printf("Syntax: run prodcons_bb <# of producer processes> <# of consumer processes> <# of iterations the producer runs> <# of iterations the consumer runs>\n");
        }
    }
    else if(strncmp(args[0], "prodcons",8) == 0){
        if(nargs <= 2) {
        resume (create(xsh_prodcons, 4096, 20, "prodcons", 2, nargs, args));
        wait(other_can_join);
        }
        else {
            printf("Syntax: run prodcons [counter]\n");
        }
    }
    else if(strncmp(args[0], "tscdf_fq", 8) == 0) {
            resume (create(stream_proc_futures, 4096, 20, "stream_proc_futures", 2, nargs, args));
        }
    else if (strncmp(args[0], "tscdf", 5) == 0) {
      resume(create(stream_proc, 4096, 20, "stream_proc", 2, nargs, args));
  }

    else if(strncmp(args[0], "futest",6) == 0) {
      if (nargs > 1) {

      resume(create(future_prodcons, 4096, 20, "future_prodcons", 2, nargs, args));
      // wait(other_can_join);
      }
      else {
        printf("Syntax: run futest [-pc [g ...] [s VALUE ...]] | [-pcq LENGTH [g ...] [s VALUE ...]] | [-f NUMBER] | [--free]\n");
      }
    }
    else if(strncmp(args[0], "fstest",6) == 0) {
      fstest(nargs, args);
    }
    else {
        printf("hello\n");
        printf("list\n");
        printf("prodcons\n");
        printf("prodcons_bb\n");
        printf("futest\n");
        printf("tscdf\n");
        // printf("tscdf_fq\n");
        // printf("fstest\n");
        return OK;
    }
  return (0);
}