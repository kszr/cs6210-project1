/**********************************************************************
gtthread_mutex.c.

This file contains the implementation of the mutex subset of the
gtthreads library.  The locks can be implemented with a simple queue.
**********************************************************************/

/*
Include as needed
*/


#include "gtthread.h"

/*
The gtthread_mutex_init() function is analogous to
pthread_mutex_init with the default parameters enforced.
There is no need to create a static initializer analogous to
PTHREAD_MUTEX_INITIALIZER.
*/
int gtthread_mutex_init(gtthread_mutex_t* mutex) {
  mutex->waiting_steque = malloc(sizeof(steque_t));
  steque_init(mutex->waiting_steque);
  mutex->locker_id = -1;
  return 0;
}

/*
The gtthread_mutex_lock() is analogous to pthread_mutex_lock.
Returns zero on success.

A simple spinlock implementation.
*/
int gtthread_mutex_lock(gtthread_mutex_t* mutex) {
  /* gttthread_mutex_lock() returns an error if the mutex is invalid. We are not checking for potential
  deadlocks here, unlike pthread_mutex_lock(). */
  if(!mutex->waiting_steque)
    return 1;
  
  gtthread_t curr = gtthread_self();
  steque_enqueue(mutex->waiting_steque, (void *) curr.id);
  
  /* Spin until the lock becomes free and the current thread arrives at the front of the mutex's queue. */
  while(1) {
    while(mutex->locker_id != -1);
    if((long) steque_front(mutex->waiting_steque) == curr.id) {
      mutex->locker_id = curr.id;
      break;
    }
  }
  return 0;
}

/*
The gtthread_mutex_unlock() is analogous to pthread_mutex_unlock.
Returns zero on success.
*/
int gtthread_mutex_unlock(gtthread_mutex_t *mutex) {
  /* gtthread_mutex_unlock() returns an error if: a) the mutex is not valid; or b) the calling thread
  does not hold a lock on the mutex. */
  if(mutex->waiting_steque == NULL || steque_size(mutex->waiting_steque) == 0)
    return 1;
  
  if(mutex->locker_id == -1)
    return 1;
  
  gtthread_t curr = gtthread_self();
  if(curr.id != (long) steque_front(mutex->waiting_steque))
    return 1;
  
  steque_pop(mutex->waiting_steque);
  mutex->locker_id = -1;
  return 0;
}

/*
The gtthread_mutex_destroy() function is analogous to
pthread_mutex_destroy and frees any resourcs associated with the mutex.
*/
int gtthread_mutex_destroy(gtthread_mutex_t *mutex) {
  steque_destroy(mutex->waiting_steque);
  free(mutex->waiting_steque);
  return 0;
}
