#include <xinu.h>
#include <prodcons.h>
#include <stdlib.h>
#include <run.h>
int n;                 // Definition for global variable 'n'

/* Now global variable n is accessible by all the processes i.e. consume and produce */
sid32 can_read;
sid32 can_write;
sid32 prodcon_can_join;

shellcmd xsh_prodcons(int nargs, char *args[]) {
  // Argument verifications and validations
  int count = 200;    // local varible to hold count
  can_write = semcreate(1);
  can_read = semcreate(0);
  prodcon_can_join = semcreate(0);
    if (nargs > 2) {
        printf("Too many arguments\n");
        return 0;
    }
    if(nargs == 2) {
        count = atoi(args[1]);
    }
    
  // create the process producer and consumer and put them in ready queue.
  // Look at the definations of function create and resume in the system folder for reference.
  resume(create(producer, 1024, 20, "producer", 1, count));
  resume(create(consumer, 1024, 20, "consumer", 1, count));
  wait(prodcon_can_join);
  signal(other_can_join);
  return (0);
}