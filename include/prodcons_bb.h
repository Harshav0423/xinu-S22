// declare globally shared array
extern int arr_q[5];
// declare globally shared semaphores
sid32 prod_bb_can_write;
sid32 prod_bb_can_read;
sid32 prod_bb_can_end;
// declare globally shared read and write indices
extern int head;
extern int tail;
extern int gb_count;
// function prototypes
void consumer_bb(int id, int count);
void producer_bb(int id, int count);
