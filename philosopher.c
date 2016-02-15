#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <pthread.h>

#include "philosopher.h"

/* Global variables */
pthread_mutex_t g_stick_mutex[5];  // Mutexes for each stick.

/* Prototypes for helper functions */
static void putdown_left_chopstick_helper(int phil_id);
static void putdown_right_chopstick_helper(int phil_id);
static int left_stick_id(int phil_id);
static int right_stick_id(int phil_id);
static int left_phil_id(int phil_id);
static int right_phil_id(int phil_id);

/*
 * Performs necessary initialization of mutexes.
 */
void chopsticks_init() {
    int i;
    for(i=0; i<5; i++)
        pthread_mutex_init(&g_stick_mutex[i], NULL);
}

/*
 * Cleans up mutex resources.
 */
void chopsticks_destroy() {
    int i;
    for(i=0; i<5; i++)
        pthread_mutex_destroy(&g_stick_mutex[i]);
}

/*
 * Uses pickup_left_chopstick and pickup_right_chopstick
 * to pick up the chopsticks
 *
 * Uses numbered resources and a strategy of first picking up the smaller numbered chopstick.
 * If the other chopstick is unavailable, the philosopher puts down the first chopstick in order
 * to avoid a hold-and-wait situation.
 */   
void pickup_chopsticks(int phil_id) {
    int left = left_stick_id(phil_id);
    int right = right_stick_id(phil_id);

    int min = left < right ? left : right;
    int max = min == left ? right : left;

    while(1) {
        // First pick up the smaller numbered chopstick.
        pthread_mutex_lock(&g_stick_mutex[min]);

        if(min == left)
            pickup_left_chopstick(phil_id);
        else pickup_right_chopstick(phil_id);
        
        if(pthread_mutex_trylock(&g_stick_mutex[max])) {
            // If the second chopstick is unavailable, put down the first chopstick and try again.
            if(min == left)
                putdown_left_chopstick_helper(phil_id);
            else putdown_right_chopstick_helper(phil_id);
        } else  {
            // Otherwise pick up the second chopstick.
            if(max == right)
                pickup_right_chopstick(phil_id);
            else pickup_left_chopstick(phil_id);
            break;
        }
    }
}

/*
 * Uses putdown_left_chopstick and putdown_right_chopstick
 * to pick up the chopsticks
 */   
void putdown_chopsticks(int phil_id) {
    putdown_left_chopstick_helper(phil_id);
    putdown_right_chopstick_helper(phil_id);
}

/* Helper functions implemented below. */

/**
 * Puts down the left chopstick and releases the mutex on it.
 */
static void putdown_left_chopstick_helper(int phil_id) {
    int left = left_stick_id(phil_id);
    putdown_left_chopstick(phil_id);
    pthread_mutex_unlock(&g_stick_mutex[left]);
}

/**
 * Puts down the right chopstick and releases the mutex on it.
 */
static void putdown_right_chopstick_helper(int phil_id) {
    int right = right_stick_id(phil_id);
    putdown_right_chopstick(phil_id);
    pthread_mutex_unlock(&g_stick_mutex[right]);
}

/**
 * Returns the id of the stick to the left of a philosopher with id phil_id.
 */
static int left_stick_id(int phil_id) {
    return (phil_id+4) % 5;
}

/**
 * Returns the id of the stick to the right of a philosopher with id phil_id.
 */
static int right_stick_id(int phil_id) {
    return phil_id;
}

/**
 * Returns the id of the philosopher to the left of a philosopher with id phil_id.
 */
static int left_phil_id(int phil_id) {
    return (phil_id+4) % 5;
}

/**
 * Returns the id of the philosopher to the left of a philosopher with id phil_id.
 */
static int right_phil_id(int phil_id) {
    return (phil_id+1) % 5;
}
