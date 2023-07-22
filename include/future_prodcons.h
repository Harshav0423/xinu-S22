#include <future.h>
uint future_prod(future_t* fut, char* value);
uint future_cons(future_t* fut);
void future_prodcons(int nargs, char *args[]);

extern char *val;
extern sid32 print_sem;

// Assignment-6
int future_fib(int nargs, char *args[]);
int future_free_test(int nargs, char *args[]);