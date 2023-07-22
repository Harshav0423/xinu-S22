#include <xinu.h>
#include <prodcons.h>
#include <run.h>
#include <prodcons_bb.h>
void consumer_bb(int id, int count) {
  // TODO: implement the following:
  // - Iterate from 0 to count (count excluding)
  //   - read the next available value from the global array `arr_q`
  //   - print consumer id (starting from 0) and read value as:
  //     "name : consumer_X, read : X"
  // printf("In consumer %d, %d", id,count);
  for (int i = 0; i < count; i++) {
    
    wait(prod_bb_can_read);
      printf("name : consumer_%d, read : %d\n",id, arr_q[tail%5]);
      tail++;
      gb_count --;
      signal(prod_bb_can_write);
  }
  if (gb_count == 0) {
        signal(prod_bb_can_end);
    }
  return;
}


void consumer(int count) {
  // TODO: implement the following:
  // - Iterates from 0 to count (count including)
  //   - reading the value of the global variable 'n' each time
  //   - print consumed value (the value of 'n'), e.g. "consumed : 8"

  int i = 0;
  for(i = 0; i<=count; i++) {
    // checking for resource to read from buffer
    wait(can_read);
    printf("consumed : %d\n", n);
    signal(can_write); // freeing up resource
  }
  signal(prodcon_can_join);
  return;
}
