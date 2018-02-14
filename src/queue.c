/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015-2018 David Zero
* All rights reserved.
*
* The following code is licensed for use, modification, and redistribution
* according to the terms of the Revised BSD License. The text of this license
* can be found in the "LICENSE" file bundled with this source distribution.
*/

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "queue.h"

struct queue{
    struct item* head;
    struct item* tail;
    size_t size;
};

struct queue* queue_create(){
    struct queue* q;
    if((q = malloc(sizeof(struct queue))) == NULL){
        logmsg(LOG_DEBUG, "queue: Could not allocate memory for a new queue\n");
        return NULL;
    }

    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

    return q;
}

void queue_destroy(struct queue* q){
    if(q->head == NULL){
        return;
    }

    struct item* this = q->head;
    while(this != NULL){
        struct item* tmp = this->next;
        free(this);
        this = tmp;
    }

    free(q);
}

int queue_enqueue(struct queue* q, const void* value, size_t size){
    struct item* itm = malloc(sizeof(struct item) + size);
    if(itm == NULL){
        logmsg(LOG_DEBUG, "queue: Could not allocate enough memory to enqueue new item\n");
        return -1;
    }
    
    memcpy(itm->value, value, size);
    itm->size = size;
    itm->next = NULL;

    if(q->head == NULL){
        q->head = itm;
        q->tail = itm;
    }
    else{
        q->tail->next = itm;
        q->tail = itm;
    }
    
    q->size++;

    return 0;
}

struct item* queue_dequeue(struct queue* q){
    if(q->head == NULL){
        return NULL;
    }

    struct item* itm = q->head;
    q->head = q->head->next;
    itm->next = NULL;

    return itm;
}

struct item* queue_peek(struct queue* q){
    if(q->head == NULL){
        return NULL;
    }

    struct item* itm = malloc(sizeof(struct item) + q->head->size);
    if(itm == NULL){
        logmsg(LOG_DEBUG, "queue: Could not allocate enough memory for a queue peek\n");
        return NULL;
    }

    memcpy(itm->value, q->head->value, q->head->size);
    itm->size = q->head->size;
    itm->next = NULL;

    return itm;
}

size_t queue_get_size(struct queue* q){
    return q->size;
}
