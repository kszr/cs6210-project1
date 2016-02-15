/**
 * Example execution for the Dining Philosophers problem. This is for debugging purposes.
 */

#include <unistd.h>
#include "philosopher.c"

philosopher_t g_phil[5];
int g_stix[5];

void init() {
    int i;
    for(i=0; i<5; i++) {
        // Initialize philosopher
        g_phil[i].holds_left = 0;
        g_phil[i].holds_right = 0;
        g_phil[i].is_eating = 0;

        // Initially nobody possesses the chopsticks.
        g_stix[i] = -1;
    }
}

void pickup_left_chopstick(int phil_id) {
    int left = left_stick_id(phil_id);
    if(g_stix[left] != -1)
        return;
    g_stix[left] = phil_id;
}

void putdown_left_chopstick(int phil_id) {
    int left = left_stick_id(phil_id);
    if(g_stix[left] != phil_id) {
        // printf("Trying to put down someone else's chopstick! (left)\n");
        return;
    }
    g_stix[left] = -1;
}

void pickup_right_chopstick(int phil_id) {
    int right = right_stick_id(phil_id);
    if(g_stix[right] != -1)
        return;
    g_stix[right] = phil_id;
}

void putdown_right_chopstick(int phil_id) {
    int right = right_stick_id(phil_id);
    if(g_stix[right] != phil_id) {
        // printf("Trying to put down someone else's chopstick! (right)\n");
        return;
    }
    g_stix[right] = -1;
}

void start_eating(int phil_id) {
    pickup_chopsticks(phil_id);
    g_phil[phil_id].is_eating = 1;
    sleep(1);
}

void stop_eating(int phil_id) {
    putdown_chopsticks(phil_id);
    g_phil[phil_id].is_eating = 0;
}

int count_meals_eaten(int phil_id) {
    return -1;
}

void *dine(void *in) {
    int id = (int) in;
    int i;
    for(i=0; i<10; i++) {
        start_eating(id);
        printf("Philosopher %d has started eating meal %d.\n", id, i);
        stop_eating(id);
        printf("Philosopher %d has stopped eating meal %d.\n", id, i);
    }
    return NULL;
}

int main() {
    chopsticks_init();
    pthread_t threads[5];

    int i;
    for(i=0; i<5; i++)
        pthread_create(&threads[i], NULL, dine, (void *) i);
    
    for(i=0; i<5; i++) {
        pthread_join(threads[i], NULL);
        printf("Thread %d has joined\n", i);
    }

    printf("Done eating!\n");
    chopsticks_destroy();
}