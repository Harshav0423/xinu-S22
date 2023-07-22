#include <future.h>
future_t *future_alloc(future_mode_t mode, uint size, uint nelem)
{
  intmask mask;
  mask = disable();
  future_t *f;
  f = (future_t *)getmem((size * nelem) + sizeof(future_t));
  // printf("%d",size);
  f->size = size;
  f->data = sizeof(future_t) + (char *)f;
  f->mode = mode;
  f->state = FUTURE_EMPTY;

  f->get_queue = newqueue();
  // TODO: write your code here

  if (f->mode == FUTURE_QUEUE)
  {
    f->set_queue = newqueue();
    f->max_elems = nelem;
    f->count = 0;
    f->head = 0;
    f->tail = 0;
  }
  restore(mask);

  return f;
}

// TODO: write your code here for future_free, future_get and future_set
syscall future_free(future_t *f)
{
  intmask mask;
  mask = disable();

  if (f->mode == FUTURE_QUEUE)
  {
    while (!isempty(f->get_queue))
    {
      pid32 id = dequeue(f->get_queue);
      kill(id);
    }
    while (!isempty(f->set_queue))
    {
      pid32 id = dequeue(f->set_queue);
      kill(id);
    }
  }
  else if (f->mode == FUTURE_EXCLUSIVE)
  {
    kill(f->pid);
  }
  else
  {
    while (!isempty(f->get_queue))
    {
      pid32 id = dequeue(f->get_queue);
      kill(id);
    }
    kill(f->pid);
  }

  restore(mask);

  // printf("%d queue", sizeof(f->get_queue));
  return freemem((char *)f, f->size + sizeof(future_t));
}

syscall future_get(future_t *f, char *out)
{

  intmask mask;
  mask = disable();
  if (f->mode == FUTURE_QUEUE)
  {
    if (f->count == 0)
    {
      f->pid = getpid();
      enqueue(getpid(), f->get_queue);
      suspend(getpid());
    }

    // memory to copy
    char* headelemptr = f->data + (f->head * f->size);

    memcpy(out, headelemptr, f->size);

    f->head = (f->head + 1) % f->max_elems; //
    f->count -= 1;
    if (!isempty(f->set_queue))
    {
      resume(dequeue(f->set_queue));
    }
    restore(mask);
    return OK;
  }
  else if (f->mode == FUTURE_EXCLUSIVE)
  {

    if (f->state == FUTURE_EMPTY)
    {
      f->state = FUTURE_WAITING;
      f->pid = getpid();
      enqueue(f->pid, f->get_queue);
      suspend(f->pid);
      memcpy(out, f->data, sizeof(f->data));
    }
    else if (f->state == FUTURE_READY)
    {
      f->state = FUTURE_EMPTY;
      memcpy(out, f->data, sizeof(f->data));
    }
    else
    {
      return SYSERR;
    }
  }
  else if (f->mode == FUTURE_SHARED)
  {

    if (f->state == FUTURE_EMPTY)
    {
      f->state = FUTURE_WAITING;
      f->pid = getpid();
      enqueue(f->pid, f->get_queue);
      suspend(f->pid);
      memcpy(out, f->data, sizeof(f->data));
    }
    else if (f->state == FUTURE_WAITING)
    {

      f->pid = getpid();
      enqueue(f->pid, f->get_queue);
      suspend(f->pid);
      memcpy(out, f->data, f->size);
    }
    else if (f->state == FUTURE_READY)
    {
      memcpy(out, f->data, sizeof(f->data));
    }
  }

  restore(mask);
  return OK;
}

syscall future_set(future_t *f, char *in)
{
  intmask mask;
  mask = disable();
  if (f->mode == FUTURE_QUEUE)
  {

    if (f->count == f->max_elems)
    {
      pid32 id = getpid();
      enqueue(id, f->set_queue);
      suspend(id);
    }

    // copying future data
    char* tailelemptr = f->data + (f->tail * f->size);

    memcpy(tailelemptr, in, f->size);
    f->tail = (f->tail + 1) % f->max_elems;
    f->count += 1; //

    if (!isempty(f->get_queue))
    {
      resume(dequeue(f->get_queue));
    }
    restore(mask);
    return OK;
  }
  else if (f->mode == FUTURE_EXCLUSIVE)
  {

    if (f->state == FUTURE_EMPTY)
    {
      memcpy(f->data, in, sizeof(in));
      f->state = FUTURE_READY;
    }
    else if (f->state == FUTURE_READY)
    {
      return SYSERR;
    }
    else
    {

      memcpy(f->data, in, sizeof(in));
      f->state = FUTURE_EMPTY;
      while (!isempty(f->get_queue))
      {
        pid32 id = dequeue(f->get_queue);
        resume(id);
      }
    }
  }
  else if (f->mode == FUTURE_SHARED)
  {
    if (f->state == FUTURE_EMPTY)
    {
      memcpy(f->data, in, sizeof(in));
      f->state = FUTURE_READY;
    }
    else if (f->state == FUTURE_WAITING)
    {
      memcpy(f->data, in, sizeof(in));

      while (!isempty(f->get_queue))
      {
        pid32 id = dequeue(f->get_queue);
        resume(id);
      }
    }
    else
    {
      return SYSERR;
    }
  }

  restore(mask);
  return OK;
}