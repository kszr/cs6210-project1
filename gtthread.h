#ifndef GTTHREAD_H
#define GTTHREAD_H

#ifdef __APPLE__
    #define _XOPEN_SOURCE 600   // Have to do this on OS X, apparently.
#endif

#include "steque.h"
#include <ucontext.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

/* Define gtthread_t and gtthread_mutex_t types here */

typedef struct gtthread_s {
  unsigned long id;
  ucontext_t *context;
  int is_finished; // 1 if the thread has finished executing. For aesthetic purposes.
  void *retval;	// Stores the return value of the function this thread was executing.
  int is_joined; // -1 = this thread is not waiting on anything; 0 = this thread is waiting on something; 1 = the thing this thread is waiting on has joined.
  unsigned long wait_tid; // Thread id of the thread this thread is waiting on. -1, if this thread is not waiting on anything.
  struct gtthread_s *joinee; // The thread this thread is waiting on.
} gtthread_t;

typedef struct gtthread_mutex_s {
  steque_t *waiting_steque; // Queue contains waiting threads.
  long locker_id; // Id of the thread currently holding the lock. -1 if lock is free.
} gtthread_mutex_t;

/* Function prototypes below. */

void gtthread_init(long period);
int  gtthread_create(gtthread_t *thread,
                     void *(*start_routine)(void *),
                     void *arg);
int  gtthread_join(gtthread_t thread, void **status);
void gtthread_exit(void *retval);
void gtthread_yield(void);
int  gtthread_equal(gtthread_t t1, gtthread_t t2);
int  gtthread_cancel(gtthread_t thread);
gtthread_t gtthread_self(void);


int  gtthread_mutex_init(gtthread_mutex_t *mutex);
int  gtthread_mutex_lock(gtthread_mutex_t *mutex);
int  gtthread_mutex_unlock(gtthread_mutex_t *mutex);
int  gtthread_mutex_destroy(gtthread_mutex_t *mutex);
#endif
