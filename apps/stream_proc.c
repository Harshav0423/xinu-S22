#include <xinu.h>
#include <stream.h>
#include <stdio.h>
#include "tscdf.h"

uint pcport;
void stream_consumer(int32 id, struct stream *str);
int q_len, time_intv, out_tm_def; //
int32 stream_proc(int nargs, char *args[])
{
    ulong secs, msecs, time;
    int32 num_streams, work_queue_depth, time_window, output_time;
    secs = clktime;
    int st, ts, v, head;
    char *ch, c, *a;
    msecs = clkticks;
    char usage[] = "Usage: -s num_streams -w work_queue_depth -t time_window -o output_time\n";

    /* Parse arguments out of flags */
    /* if not even # args, print error and exit */
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
    // streams creation
    struct stream **new_stream;
    q_len = work_queue_depth;
    time_intv = time_window;
    out_tm_def = output_time;

    struct stream **total_size;
    new_stream = (struct stream **)getmem(sizeof(struct stream *) * (num_streams));
    if (new_stream == (struct stream **)SYSERR)
    {
        printf("ERROR: GETMEM FAILED\n");
    }

    int i = 0;
    while (i < num_streams)
    {
        new_stream[i] = (struct stream *)getmem(sizeof(struct stream) + (sizeof(de) * work_queue_depth));
        if (new_stream[i] == (struct stream *)SYSERR)
        {
            printf("ALLOCATION ERROR");
            return;
        }
        
        new_stream[i]->items = semcreate(work_queue_depth);
        new_stream[i]->mutex = semcreate(1);
        new_stream[i]->spaces = semcreate(0);
        new_stream[i]->head = 0;
        new_stream[i]->tail = 0;
        new_stream[i]->queue = sizeof(struct stream) + (char *)new_stream[i];
        i++;
    }

    // initiating consume
    for (int j = 0; j < num_streams; j++)
    {
        resume(create(stream_consumer, 4096, 20, "stream_consumer", 2, j, new_stream[j]));
    }

    for (int i = 0; i < n_input; i++)
    {
        a = (char *)stream_input[i];
        st = atoi(a);
        while (*a++ != '\t')
            ;
        ts = atoi(a);
        while (*a++ != '\t')
            ;
        v = atoi(a);

        // stopping stream cons
        wait(new_stream[st]->mutex);
        wait(new_stream[st]->items);
        head = new_stream[st]->head;

        new_stream[st]->queue[head].time = ts;
        new_stream[st]->queue[head].value = v;

        head = ++head % work_queue_depth;
        new_stream[st]->head = head;
        signal(new_stream[st]->mutex);
        signal(new_stream[st]->spaces);
    }

    int pid = 0;
    while (pid < num_streams)
    {
        uint32 pm;
        pm = ptrecv(pcport);
        printf("process %d exited\n", pm);
        pid++;
    }

    ptdelete(pcport, 0);

    time = (((clktime * 1000) + clkticks) - ((secs * 1000) + msecs));
    kprintf("time in ms: %u\n", time);

    return (0);
}

void stream_consumer(int32 id, struct stream *str)
{
    int tail, lef = 0;
    struct tscdf *tcd;
    pid32 proc_id = getpid();
    kprintf("stream_consumer id:%d (pid:%d)\n", id, proc_id);
    tcd = tscdf_init(time_intv); //
    while (1)
    {
        wait(str->spaces);
        wait(str->mutex);
        tail = str->tail;
        if (str->queue[str->tail].time == 0 && str->queue[str->tail].value == 0)
        {
            kprintf("stream_consumer exiting\n");
            break;
        }
        tscdf_update(tcd, str->queue[tail].time, str->queue[tail].value);

        if (lef++ == (out_tm_def - 1))
        {
            char output[10];
            int *qarray;
            qarray = tscdf_quartiles(tcd);
            if (qarray == NULL)
            {
                kprintf("tscdf_quartiles returned NULL\n");
                continue;
            }
            sprintf(output, "s%d: %d %d %d %d %d \n", id, qarray[0], qarray[1], qarray[2], qarray[3], qarray[4]);
            kprintf("%s", output);
            freemem((char *)qarray, (6 * sizeof(int32)));
            lef = 0;
        }
        tail = (++tail) % q_len;

        str->tail = tail;
        signal(str->items);
        signal(str->mutex);
    }
    tscdf_free(tcd);
    ptsend(pcport, getpid());
}