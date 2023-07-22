#include <xinu.h>
#include <stream.h>
#include <stdio.h>
#include "tscdf.h"
#include <future.h>

uint pcport;
int num_streams, work_queue_depth, time_window, output_time;
int stream_proc_futures(int nargs, char *args[]);
void stream_consumer_future(int32 id, future_t *f);

int32 stream_proc_futures(int nargs, char *args[])
{
    ulong secs, msecs, time;

    secs = clktime;

    msecs = clkticks;
    // TODO: Parse arguments
    char usage[] = "run tscdf_fq -s <num_streams> -w <work_queue_depth> -t <time_window> -o <output_time>\n";

    /* Parse arguments out of flags */
    /* if not even # args, print error and exit */
    char *ch, c;
    if (!(nargs % 2))
    {
        printf("%s", usage);
        return (-1);
    }
    else
    {
        int i = nargs - 1;
        while (i > 0)
        {
            ch = args[i - 1];
            c = *(++ch);

            switch (c)
            {
            case 's':
                num_streams = atoi(args[i]);
                break;

            case 'w':
                work_queue_depth = atoi(args[i]);
                break;

            case 't':
                time_window = atoi(args[i]);
                break;

            case 'o':
                output_time = atoi(args[i]);
                break;

            default:
                printf("%s", usage);
                return (-1);
            }

            i -= 2;
        }
    }

    if ((pcport = ptcreate(num_streams)) == SYSERR)
    {
        printf("ptcreate failed\n");
        return (-1);
    }
    // futures creation
    future_t **futures = (struct future_t **)getmem(num_streams * sizeof(future_t));

    int i = 0;
    for (i = 0; i < num_streams; i++)
    {
        futures[i] =  future_alloc(FUTURE_QUEUE, sizeof(de), work_queue_depth);
        resume(create((void *)stream_consumer_future, 2048, 20, "stream_consumer_future", 2, i, futures[i]));
    }

    // TODO: Parse input header file data and populate work queue
    char *a;
    for (int i = 0; i < n_input; i++)
    {
        a = (char *)stream_input[i];
        int st = atoi(a);
        while (*a++ != '\t')
            ;
        int ts = atoi(a);
        while (*a++ != '\t')
            ;
        int v = atoi(a);

        de *prod = (de *)getmem(sizeof(de));
        prod->time = ts;
        prod->value = v;

        // calling future set for producer
        future_set(futures[st], prod);
    }

    // TODO: Join all launched consumer processes
    for (i = 0; i < num_streams; i++)
    {
        uint32 pm = ptrecv(pcport);
        printf("process %d exited\n", pm);
    }

    ptdelete(pcport, 0);

    // TODO: Free all futures
    for (int i = 0; i < num_streams; i++)
    {
        future_free(futures[i]);
    }

    time = (((clktime * 1000) + clkticks) - ((secs * 1000) + msecs));
    kprintf("time in ms: %u\n", time);

    return (0);
}

void stream_consumer_future(int32 id, future_t *f)
{

    pid32 proc_id = getpid();
    kprintf("stream_consumer_future id:%d (pid:%d)\n", id, proc_id);
    struct tscdf *tcd = tscdf_init(time_window); //

    int count = 0;
    char *output;
    int *qarray = (int32 *)getmem(6 * sizeof(int32));
    while (1)
    {
        de *data;

        // fetching future data
        future_get(f, data);

        int upd = tscdf_update(tcd, data->time, data->value);

        if (data->time == 0)
        {
            break;
        }
        count = count + 1;

        if (upd == SYSERR)
        {
            return SYSERR;
        }

        if (count == output_time)
        {
            count = 0;
            output = "";
            qarray = tscdf_quartiles(tcd);

            if (qarray == NULL)
            {
                kprintf("tscdf_quartiles returned NULL\n");
                continue;
            }
            sprintf(output, "s%d: %d %d %d %d %d", id, qarray[0], qarray[1], qarray[2], qarray[3], qarray[4]);
            kprintf("%s\n", output);
            freemem((char *)qarray, (6 * sizeof(int32)));
        }
    }
    tscdf_free(tcd);
    future_free(f);
    kprintf("stream_consumer_future exiting\n");
    ptsend(pcport, getpid());
    return;
}