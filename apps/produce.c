#include <xinu.h>
#include <prodcons.h>
#include<prodcons_bb.h>

void producer_bb(int id, int count) {
  // TODO: implement the following:
  // - Iterate from 0 to count (count excluding)
  //   - add iteration value to the global array `arr_q`
  //   - print producer id (starting from 0) and written value as:
  //     "name : producer_X, write : X"
  for(int i = 0; i < count; i++) {
    
      wait(prod_bb_can_write); // stopping the process execution until its a valid number
      arr_q[head%5] = i;
      head++;
      printf("name : producer_%d, write : %d\n",id, i);
      signal(prod_bb_can_read); // giving signal to consumer
  }
  return;

}

void producer(int count) {
  // TODO: implement the following:
  // - Iterates from 0 to count (count including)
  //   - setting the value of the global variable 'n' each time
  //   - print produced value (new value of 'n'), e.g.: "produced : 8"

  int i = 0;
  for(i = 0; i<=count; i++) {
    // checking for resource to write into buffer
    wait(can_write);
    n = i;
    printf("produced : %d\n", n);
    signal(can_read);
  }
  return;
}
