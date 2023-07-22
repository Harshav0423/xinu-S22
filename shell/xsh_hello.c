/* xsh_hello.c - xsh_hello */

#include <xinu.h>
#include <string.h>
#include <stdio.h>
#include <run.h>
/*------------------------------------------------------------------------
 * xsh_hello - printing the string, error for few or too many arguments.
 *------------------------------------------------------------------------
 */
shellcmd xsh_hello(int nargs, char *args[]) {


    if (nargs <= 1 ) {
        printf("too few arguments\n");
        printf("Try --help for more information");
    }
    else if (nargs == 2) {
        printf("Hello %s, Welcome to the world of Xinu!!", args[1]);
    }
    else {
        printf("too many arguments\n");
        printf("Try --help for more information");
    }
    printf("\n");
    signal(other_can_join);
    return 0;
}