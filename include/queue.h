/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015-2017 David Zero
* All rights reserved.
*
* The following code is licensed for use, modification, and redistribution
* according to the terms of the Revised BSD License. The text of this license
* can be found in the "LICENSE" file bundled with this source distribution.
*/

#ifndef PRAETOR_QUEUE
#define PRAETOR_QUEUE

#include <stdint.h>

struct queue;

/**
 * A struct representing an item in the queue.
 */
struct item{
    /**
     * This field is not used by a caller, and will always be NULL.
     */
    struct item* next;
    /**
     * The size, in bytes, of \c value.
     */
    size_t size;
    /**
     * The variably-sized value.
     */
    uint8_t value[];
};

/**
 * Creates a queue.
 *
 * This function will fail only if the system is out of memory.
 *
 * \return On success, returns a pointer to a dynamically allocated queue.
 * \return On failure, returns NULL.
 */
struct queue* queue_create();

/**
 * Destroys the given queue and frees all associated memory.
 *
 * \param queue The queue to destroy.
 */
void queue_destroy(struct queue* q);

/**
 * Adds an item to the back of the queue.
 *
 * \c size bytes of \c value are copied into an item which will be added to the
 * queue.
 *
 * This function will fail only if the system is out of memory.
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
int queue_enqueue(struct queue* q, const void* value, size_t size);

/**
 * Removes an item from the front of the queue. The caller is responsible for
 * freeing the returned item.
 *
 * This function will fail if the system is out of memory, or if there are no
 * items in the queue.
 *
 * \return On success, returns a pointer to the dequeued item.
 * \return On failure, returns NULL.
 */
struct item* queue_dequeue(struct queue* q);

/**
 * Returns a copy of the item at the front of the queue. The caller is
 * responsible for freeing the returned item.
 *
 * This function will fail if the system is out of memory, or if there are no
 * items in the queue.
 *
 * \return On success, returns an item.
 * \return On failure, returns NULL.
 */
struct item* queue_peek(struct queue* q);

/**
 * Returns the number of items present in the given queue.
 */
size_t queue_get_size(struct queue* q);

#endif
