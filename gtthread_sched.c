/**********************************************************************
gtthread_sched.c.

This file contains the implementation of the scheduling subset of the
gtthreads library.  A simple round-robin queue should be used.
**********************************************************************/
/*
Include as needed
*/

#include "gtthread.h"

/*
Students should define global variables and helper functions as
they see fit.
*/

/* Define variables below */

// Steques
static steque_t g_threads_steque; // Keeps track of threads.
static steque_t g_dead_threads_steque; // Keeps track of threads that have finished running.
static steque_t g_join_steque;  // A steque where threads that are waiting on other threads sit and wait.
static steque_t g_cancelatorium;  // Stores the id of the thread set up to be canceled.

// Other global variables
static int global_period; // The time quantum.
static long g_thread_id = 0;  // At any given time stores a thread_id that can be used for a new thread.
static struct itimerval T;
static struct sigaction act;
static sigset_t vtalrm;

/* Prototypes for helper functions */

// Helper functions for alarms and signals.
static int set_up_alarm();
static void alarm_handler(int sig);

// Helper function for wrapping other function calls
static void apply(void *(*func) (void *), void *arg, gtthread_t *runner_thread);

// Other helper functions
static void alarm_safe_yield();
static void yield_helper(int is_alarm_safe);
static void joininator(gtthread_t *joinee);

/*
The gtthread_init() function does not have a corresponding pthread equivalent.
It must be called from the main thread before any other GTThreads
functions are called. It allows the caller to specify the scheduling
period (quantum in micro second), and may also perform any other
necessary initialization.  If period is zero, then thread switching should
occur only on calls to gtthread_yield().

Recall that the initial thread of the program (i.e. the one running
main() ) is a thread like any other. It should have a
gtthread_t that clients can retrieve by calling gtthread_self()
from the initial thread, and they should be able to specify it as an
argument to other GTThreads functions. The only difference in the
initial thread is how it behaves when it executes a return
instruction. You can find details on this difference in the man page
for pthread_create.
*/
void gtthread_init(long period){
  steque_init(&g_threads_steque);
  steque_init(&g_dead_threads_steque);
  steque_init(&g_cancelatorium);
  steque_init(&g_join_steque);
  
  global_period = period;
  
  // Setting up the signal mask
  if(set_up_alarm()) {
    perror("Error setting up alarm");
    exit(EXIT_FAILURE);
  }
  
  gtthread_t *main_thread = malloc(sizeof(gtthread_t));
  
  // Sets the thread id and other attributes.
  main_thread->id = g_thread_id++;
  main_thread->is_finished = 0;
  main_thread->is_joined = -1;
  main_thread->wait_tid = -1L;
  main_thread->retval = NULL;
  
  main_thread->context = (ucontext_t *) malloc(sizeof(ucontext_t));
  
  if(getcontext(main_thread->context) == -1) {
    perror("Error calling getcontext()");
    exit(EXIT_FAILURE);
  }
  
  // Creating a stack for the context.
  main_thread->context->uc_stack.ss_sp = (char *) malloc(SIGSTKSZ);
  main_thread->context->uc_stack.ss_size = SIGSTKSZ;
  
  steque_enqueue(&g_threads_steque, main_thread);
}

/*
 The gtthread_create() function mirrors the pthread_create() function,
 only default attributes are always assumed.
*/
int gtthread_create(gtthread_t *thread,
void *(*start_routine)(void *),
void *arg) {
  // Sets the thread id and other attributes.
  thread->id = g_thread_id++;
  thread->is_finished = 0;
  thread->is_joined = -1;
  thread->wait_tid = -1L;
  thread->retval = NULL;
  
  thread->context = (ucontext_t *) malloc(sizeof(ucontext_t));
  ucontext_t curr_context;
  
  if(getcontext(thread->context) == -1) {
    perror("Error calling getcontext()");
    exit(EXIT_FAILURE);
  }
  
  // Creating a stack for the context.
  thread->context->uc_stack.ss_sp = (char *) malloc(SIGSTKSZ);
  thread->context->uc_stack.ss_size = SIGSTKSZ;
  
  // Sets the caller thread to be the new thread's successor.
  thread->context->uc_link = &curr_context;
  
  // Uses the apply() helper function with start_routine and arg as arguments.
  makecontext(thread->context, (void *) apply, 3, (void *) start_routine, arg, thread);
  
  steque_enqueue(&g_threads_steque, thread);
  
  return 0;
}

/*
 The gtthread_join() function is analogous to pthread_join.
 All gtthreads are joinable.
*/
int gtthread_join(gtthread_t thread, void **status) {
  sigprocmask(SIG_BLOCK, &vtalrm, NULL);
  gtthread_t *self = steque_front(&g_threads_steque);
  sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
  
  /* The range [0, g_thread_id) indicates the range of thread ids that have ever belonged to valid threads.
  Also can't join with self. */
  if(thread.id >= g_thread_id || thread.id == self->id)
    return 1;
  
  sigprocmask(SIG_BLOCK, &vtalrm, NULL);
  self->is_joined = 0;
  self->wait_tid = thread.id;
  sigprocmask(SIG_BLOCK, &vtalrm, NULL);
  int found_among_dead = 0;
  
  /* First look for joinee among threads that have already terminated. */
  int i;
  for(i=0; i<steque_size(&g_dead_threads_steque); i++) {
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
    gtthread_t *curr = steque_front(&g_dead_threads_steque);
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
    
    if(curr->id == self->wait_tid) {
      found_among_dead = 1;
      sigprocmask(SIG_BLOCK, &vtalrm, NULL);
      self->joinee = curr;
      self->is_joined = 1;
      sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
      break;
    }
    
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
    steque_cycle(&g_dead_threads_steque);
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
  }
  
  /* If we haven't found it, wait until it terminates. */
  if(!found_among_dead) {
    // First check to see that the thread I am waiting on is not already waiting on me.
    for(i=0; i<steque_size(&g_join_steque); i++) {
      sigprocmask(SIG_BLOCK, &vtalrm, NULL);
      gtthread_t *curr = steque_front(&g_join_steque);
      sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
      
      if(curr->wait_tid == self->id && curr->id == self->wait_tid) {
        sigprocmask(SIG_BLOCK, &vtalrm, NULL);
        self->is_joined = -1;
        self->wait_tid = -1L;
        sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
        return 1;
      }
      sigprocmask(SIG_BLOCK, &vtalrm, NULL);
      steque_cycle(&g_join_steque);
      sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
    }
    
    // If joinee isn't already waiting on me, enqueue myself in the join queue...
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
    steque_enqueue(&g_join_steque, self);
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
    
    // ... and wait until joinee terminates.
    while(!self->is_joined)
    alarm_safe_yield();
  }
  
  if(status)
    *status = self->joinee->retval;
  
  sigprocmask(SIG_BLOCK, &vtalrm, NULL);
  self->joinee = NULL;
  self->wait_tid = -1L;
  self->is_joined = -1;
  sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
  
  /* Remove yourself from the join steque, if you put yourself there. */
  if(!found_among_dead) {
    for(i=0; i<steque_size(&g_join_steque); i++) {
      sigprocmask(SIG_BLOCK, &vtalrm, NULL);
      gtthread_t *curr = steque_front(&g_join_steque);
      sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
      
      if(gtthread_equal(*self, *curr)) {
        steque_pop(&g_join_steque);
        break;
      }
      if(steque_size(&g_join_steque) > 0) {
        sigprocmask(SIG_BLOCK, &vtalrm, NULL);
        steque_cycle(&g_join_steque);
        sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
      }
    }
  }
  
  return 0;
}

/*
 The gtthread_exit() function is analogous to pthread_exit.
*/
void gtthread_exit(void* retval) {
  sigprocmask(SIG_BLOCK, &vtalrm, NULL);
  gtthread_t *thread = steque_front(&g_threads_steque);
  thread->is_finished = 1;
  
  if(retval != NULL)
    thread->retval = retval;
  
  /* If this was the last thread in the queue, clean up and exit */
  if(steque_size(&g_threads_steque) == 1) {
    gtthread_t *dead_thread;
    int num = steque_size(&g_dead_threads_steque);

    while(num > 0) {
      dead_thread = steque_front(&g_dead_threads_steque);
      if(dead_thread->context && dead_thread->context->uc_stack.ss_sp)
        free(dead_thread->context->uc_stack.ss_sp);
      if(dead_thread->context)
        free(dead_thread->context);
      if(dead_thread->id == 0)
        free(dead_thread);

      if(steque_size(&g_dead_threads_steque) > 1)
        steque_cycle(&g_dead_threads_steque);
      num--;
    }
	
    if(thread->context)
      free(thread->context); 

    // Main thread was the only one that was dynamically allocated.
    if(thread->id == 0)
      free(thread);

    steque_destroy(&g_threads_steque);
    steque_destroy(&g_dead_threads_steque);
    steque_destroy(&g_cancelatorium);
    steque_destroy(&g_join_steque);


    /*
    So apparently we can't free the stack of the thread that is running.
    */
    // if(thread->context && thread->context->uc_stack.ss_sp)
      //  free(thread->context->uc_stack.ss_sp);
    exit(0);
    }
  
  sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
  alarm_safe_yield();
}


/*
 The gtthread_yield() function is analogous to pthread_yield, causing
 the calling thread to relinquish the cpu and place itself at the
 back of the schedule queue.
*/
void gtthread_yield(void) {
  alarm_safe_yield();
}

/*
 The gtthread_yield() function is analogous to pthread_equal,
 returning zero if the threads are the same and non-zero otherwise.

 TYPOS:
  - gtthread_yield() should be gtthread_equal()
  - Should be "non-zero if the threads are the same and zero otherwise".
*/
int gtthread_equal(gtthread_t t1, gtthread_t t2) {
  return t1.id == t2.id;
}

/*
 The gtthread_cancel() function is analogous to pthread_cancel,
 allowing one thread to terminate another asynchronously.
*/
int gtthread_cancel(gtthread_t thread) {
  steque_enqueue(&g_cancelatorium, (void *) thread.id);
  if(gtthread_equal(thread, gtthread_self()))
    alarm_safe_yield();
  return 0;
}

/*
 Returns calling thread.
*/
gtthread_t gtthread_self(void) {
  /* Well, in order for the calling thread to be calling anything,
  it has to be at the front of the queue. */
  gtthread_t *self = steque_front(&g_threads_steque);
  return *self;
}

/* Helper functions defined below */

/**
 * Sets up the signal mask and alarm, as well as any other
 * data structures connected with alarms and signal handling.
 */
static int set_up_alarm() {
  sigemptyset(&vtalrm);
  sigaddset(&vtalrm, SIGVTALRM);
  sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
  
  // Setting up the alarm.
  T.it_value.tv_sec = T.it_interval.tv_sec = 0;
  T.it_value.tv_usec = T.it_interval.tv_usec = global_period;
  
  setitimer(ITIMER_VIRTUAL, &T, NULL);
  
  // Setting up the alarm handler.
  act.sa_handler = &alarm_handler;
  if (sigaction(SIGVTALRM, &act, NULL) < 0) {
    perror ("sigaction");
    return 1;
  }
  return 0;
}

/**
 * The alarm handler.
 */
static void alarm_handler(int sig) {
  yield_helper(0);
}

/**
 * Moves the thread to the back of the queue and resets the alarm.
 */
static void alarm_safe_yield() {
  yield_helper(1);
}

/**
 * Helper function for yield.
 */
static void yield_helper(int is_alarm_safe) {
  if(is_alarm_safe)
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
  
  // Don't need to do anything if there's just one thread in the queue.
  if(steque_size(&g_threads_steque) == 1)
    return;
  
  gtthread_t *old_thread = steque_pop(&g_threads_steque);
  gtthread_t *new_thread = NULL;
  
  /* Find an eligible new thread - i.e., a thread that isn't queued for cancelation. */
  if(!is_alarm_safe)
    sigprocmask(SIG_BLOCK, &vtalrm, NULL);
  
  while(steque_size(&g_threads_steque) > 0) {
    new_thread = steque_front(&g_threads_steque);
    
    /* Cancels threads when it's their turn to run */
    int i;
    int canceled=0;
    for(i=0; i < steque_size(&g_cancelatorium); i++) {
      if((long) steque_front(&g_cancelatorium) == new_thread->id) {
        new_thread->is_finished = 1;
        new_thread->retval = (void *) -1;
        steque_pop(&g_cancelatorium);
        steque_pop(&g_threads_steque);
        steque_enqueue(&g_dead_threads_steque, new_thread);
        
        canceled=1;

        joininator(new_thread); // Attempt to join the thread you just canceled.
        break;
      }
      if(steque_size(&g_cancelatorium) > 0)
        steque_cycle(&g_cancelatorium);
    }
    
    if(!canceled)
      break;
  }
  
  /* If the thread that yielded finsihed executing, put it in the finished steque. */
  if(old_thread->is_finished) {
    steque_enqueue(&g_dead_threads_steque, old_thread);
    joininator(old_thread);
  } else {
    steque_enqueue(&g_threads_steque, old_thread);
  }
  
  if(!is_alarm_safe)
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
  
  // All threads have finished running. Is this necessary?
  if(steque_size(&g_threads_steque) == 0)
    exit(0);
  
  // Don't context switch if the original thread is the only one left in the queue.
  if(gtthread_equal(*((gtthread_t *) steque_front(&g_threads_steque)), *old_thread))
    return;
  
  if(is_alarm_safe) {
    T.it_value.tv_usec = global_period; // Reset timer so that the next period can start immediately.
    sigprocmask(SIG_UNBLOCK, &vtalrm, NULL);
  }
  
  swapcontext(old_thread->context, new_thread->context);
}

/**
 * Cycles through the join_steque to update the waiting status of any threads that may
 * be waiting on a thread to join. 
 */
static void joininator(gtthread_t *joinee) {
  int i;
  for(i=0; i<steque_size(&g_join_steque); i++) {
    gtthread_t *curr = steque_front(&g_join_steque);
    
    if(curr->wait_tid == joinee->id) {
      curr->is_joined = 1;
      curr->joinee = joinee;
    }
    
    steque_cycle(&g_join_steque);
  }
}

/**
 * Applies the function pointed to by func to arg, and stores the return
 * value in retval. Inspired by the apply() method in Python.
 */
static void apply(void *(*func) (void *), void *arg, gtthread_t *runner_thread) {
  runner_thread->retval = (void *) func(arg);
  gtthread_exit(NULL);
}
