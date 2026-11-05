#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "BENSCHILLIBOWL.h"

/* Override these at compile time to test small queues and many threads. */
#ifndef BENSCHILLIBOWL_SIZE
#define BENSCHILLIBOWL_SIZE 100
#endif
#ifndef NUM_CUSTOMERS
#define NUM_CUSTOMERS 90
#endif
#ifndef NUM_COOKS
#define NUM_COOKS 10
#endif
#ifndef ORDERS_PER_CUSTOMER
#define ORDERS_PER_CUSTOMER 3
#endif
#define EXPECTED_NUM_ORDERS (NUM_CUSTOMERS * ORDERS_PER_CUSTOMER)
BENSCHILLIBOWL *bcb;

static void CheckThread(int error) {
    if (error) {
        fprintf(stderr, "pthread: %s\n", strerror(error));
        exit(EXIT_FAILURE);
    }
}

void* BENSCHILLIBOWLCustomer(void* tid) {
    int customer_id = (int)(intptr_t)tid;
    for (int i = 0; i < ORDERS_PER_CUSTOMER; ++i) {
        Order *order = calloc(1, sizeof(*order));
        if (!order) { perror("calloc order"); exit(EXIT_FAILURE); }
        order->customer_id = customer_id;
        order->menu_item = PickRandomMenuItem();
        /* Ownership passes to a cook through the monitor. */
        AddOrder(bcb, order);
    }
    return NULL;
}

void* BENSCHILLIBOWLCook(void* tid) {
    int cook_id = (int)(intptr_t)tid;
    int orders_fulfilled = 0;
    Order *order;
    while ((order = GetOrder(bcb)) != NULL) {
        printf("Cook #%d fulfilled order #%d for customer #%d: %s\n",
               cook_id, order->order_number, order->customer_id, order->menu_item);
        ++orders_fulfilled;
        /* Menu strings are static; only the order itself is allocated. */
        free(order);
    }
    printf("Cook #%d fulfilled %d orders\n", cook_id, orders_fulfilled);
    return NULL;
}

int main(void) {
    if (NUM_COOKS < 1 || NUM_CUSTOMERS < 0 || ORDERS_PER_CUSTOMER < 0)
        return EXIT_FAILURE;
    srand((unsigned)time(NULL));
    bcb = OpenRestaurant(BENSCHILLIBOWL_SIZE, EXPECTED_NUM_ORDERS);
    if (!bcb) { fputs("Cannot open restaurant\n", stderr); return EXIT_FAILURE; }
    pthread_t cooks[NUM_COOKS];
    pthread_t customers[NUM_CUSTOMERS > 0 ? NUM_CUSTOMERS : 1];
    /* Start consumers first so even a capacity-one queue can make progress. */
    for (int i = 0; i < NUM_COOKS; ++i)
        CheckThread(pthread_create(&cooks[i], NULL, BENSCHILLIBOWLCook, (void*)(intptr_t)i));
    for (int i = 0; i < NUM_CUSTOMERS; ++i)
        CheckThread(pthread_create(&customers[i], NULL, BENSCHILLIBOWLCustomer, (void*)(intptr_t)i));
    for (int i = 0; i < NUM_CUSTOMERS; ++i)
        CheckThread(pthread_join(customers[i], NULL));
    for (int i = 0; i < NUM_COOKS; ++i)
        CheckThread(pthread_join(cooks[i], NULL));
    CloseRestaurant(bcb);
    return EXIT_SUCCESS;
}
