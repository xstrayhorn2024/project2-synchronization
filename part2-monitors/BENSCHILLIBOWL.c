#include "BENSCHILLIBOWL.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

bool IsEmpty(BENSCHILLIBOWL* bcb);
bool IsFull(BENSCHILLIBOWL* bcb);
void AddOrderToBack(Order **orders, Order *order);

MenuItem BENSCHILLIBOWLMenu[] = { 
    "BensChilli", 
    "BensHalfSmoke", 
    "BensHotDog", 
    "BensChilliCheeseFries", 
    "BensShake",
    "BensHotCakes",
    "BensCake",
    "BensHamburger",
    "BensVeggieBurger",
    "BensOnionRings",
};
int BENSCHILLIBOWLMenuLength = 10;

/* pthread functions return error numbers rather than setting errno. */
static void CheckThread(int error) {
    if (error) {
        fprintf(stderr, "pthread: %s\n", strerror(error));
        exit(EXIT_FAILURE);
    }
}

MenuItem PickRandomMenuItem(void) {
    static pthread_mutex_t menu_mutex = PTHREAD_MUTEX_INITIALIZER;
    CheckThread(pthread_mutex_lock(&menu_mutex));
    int index = rand() % BENSCHILLIBOWLMenuLength;
    CheckThread(pthread_mutex_unlock(&menu_mutex));
    return BENSCHILLIBOWLMenu[index];
}

BENSCHILLIBOWL* OpenRestaurant(int max_size, int expected_num_orders) {
    if (max_size <= 0 || expected_num_orders < 0) return NULL;
    BENSCHILLIBOWL *bcb = calloc(1, sizeof(*bcb));
    if (!bcb) return NULL;
    bcb->max_size = max_size;
    bcb->expected_num_orders = expected_num_orders;
    bcb->next_order_number = 1;
    CheckThread(pthread_mutex_init(&bcb->mutex, NULL));
    CheckThread(pthread_cond_init(&bcb->can_add_orders, NULL));
    CheckThread(pthread_cond_init(&bcb->can_get_orders, NULL));
    puts("Restaurant is open!");
    return bcb;
}

/* Called after all customer and cook threads have been joined. */
void CloseRestaurant(BENSCHILLIBOWL* bcb) {
    assert(bcb != NULL);
    assert(bcb->orders == NULL && bcb->current_size == 0);
    assert(bcb->orders_handled == bcb->expected_num_orders);
    assert(bcb->next_order_number - 1 == bcb->orders_handled);
    CheckThread(pthread_cond_destroy(&bcb->can_add_orders));
    CheckThread(pthread_cond_destroy(&bcb->can_get_orders));
    CheckThread(pthread_mutex_destroy(&bcb->mutex));
    free(bcb);
    puts("Restaurant is closed!");
}

/* Helpers that inspect or change the queue require the monitor lock. */
bool IsEmpty(BENSCHILLIBOWL* bcb) { return bcb->current_size == 0; }
bool IsFull(BENSCHILLIBOWL* bcb) { return bcb->current_size >= bcb->max_size; }
void AddOrderToBack(Order **orders, Order *order) {
    order->next = NULL;
    while (*orders) orders = &(*orders)->next;
    *orders = order;
}

int AddOrder(BENSCHILLIBOWL* bcb, Order* order) {
    assert(bcb != NULL && order != NULL);
    CheckThread(pthread_mutex_lock(&bcb->mutex));
    /* Recheck after every wakeup, including spurious wakeups. */
    while (IsFull(bcb))
        CheckThread(pthread_cond_wait(&bcb->can_add_orders, &bcb->mutex));
    int number = bcb->next_order_number++;
    order->order_number = number;
    AddOrderToBack(&bcb->orders, order);
    ++bcb->current_size;
    assert(bcb->current_size <= bcb->max_size);
    CheckThread(pthread_cond_signal(&bcb->can_get_orders));
    CheckThread(pthread_mutex_unlock(&bcb->mutex));
    /* A cook owns the order now; do not dereference it after unlocking. */
    return number;
}

Order *GetOrder(BENSCHILLIBOWL* bcb) {
    CheckThread(pthread_mutex_lock(&bcb->mutex));
    while (IsEmpty(bcb) && bcb->orders_handled < bcb->expected_num_orders)
        CheckThread(pthread_cond_wait(&bcb->can_get_orders, &bcb->mutex));
    if (IsEmpty(bcb)) {
        CheckThread(pthread_cond_broadcast(&bcb->can_get_orders));
        CheckThread(pthread_mutex_unlock(&bcb->mutex));
        puts("No more orders");
        return NULL;
    }
    Order *order = bcb->orders;
    bcb->orders = order->next;
    order->next = NULL;
    --bcb->current_size;
    ++bcb->orders_handled;
    CheckThread(pthread_cond_signal(&bcb->can_add_orders));
    /* The last dequeue must wake every cook waiting on an empty queue. */
    if (bcb->orders_handled == bcb->expected_num_orders)
        CheckThread(pthread_cond_broadcast(&bcb->can_get_orders));
    CheckThread(pthread_mutex_unlock(&bcb->mutex));
    return order;
}
