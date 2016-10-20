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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "log.h"

/**
 * A struct representing a hash table entry. An instance of this struct is what
 * is stored in the "bucket array". When collisions occur, this struct is able
 * to act as a link in a linked list via its \b next pointer.
 */
struct htable_data{
    /**
     * A pointer to a copy of the caller-supplied key for this mapping. The
     * data pointed to is maintained by the hash table, and not directly
     * modifiable by the caller.
     */
    void* key;
    /**
     * The length, in char-sized units, of the stored key.
     */
    size_t key_len;
    /**
     * A pointer to the value mapped by the stored key. The data pointed to is
     * caller-managed, and not stored or modified by the hash table.
     */
    void* value;
    /**
     * If a hash collision has occurred in the bucket that this htable_data
     * entry belongs to, next will be a pointer to the next key-value mapping
     * in this bucket.
     */
    struct htable_data* next;
};

struct htable{
    /**
     * An array of buckets (htable_data structs) containing the stored
     * key-value pairs
     */
    struct htable_data* bucket_array;
    /**
     * The number of key-value pairs mapped within the hash table.
     */
    size_t size;
    /**
     * The number of buckets contained within the hash table.
     */
    size_t bucket_count;
    /** 
     * The maximum load factor of the hash table, given by \f$(n/k)\f$, where
     * \f$n\f$ is the number of key-value mappings, and \f$k\f$ is the number
     * of buckets.  If the table exceeds this threshold, it will be
     * automatically doubled in size. This value is set to .75 by default, and
     * may be modified by the caller to change the automatic resizing behavior
     * of the table.
     */
    double load_threshold;
};

struct htable* htable_create(size_t size){
    if(size == 0){
        return NULL;
    }
    struct htable* table = calloc(1, sizeof(struct htable));
    if(table == NULL){
        logmsg(LOG_ERR, "htable: Could not allocate memory for a new hash table\n");
        return NULL;
    }
    table->bucket_array = calloc(size, sizeof(struct htable_data));
    if(table->bucket_array == NULL){
        free(table);
        logmsg(LOG_ERR, "htable: Could not allocate memory for a new hash table\n");
        return NULL;
    }
    table->bucket_count = size;
    table->load_threshold = .75;
    logmsg(LOG_DEBUG, "htable %p: New hash table created\n", (void*)table);
    return table;
}

/**
 * An implementation of Bob Jenkins's one-at-a-time hash function
 */
uint32_t hash(const char* key, size_t len){
    uint32_t hash, i;
    for(hash = i = 0; i < len; ++i){
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

void free_buckets(struct htable_data* bucket_array, size_t bucket_count){
    for(int i = 0; i < bucket_count; i++){
        if(bucket_array[i].next != 0){
            struct htable_data* entry = bucket_array[i].next;
            do{
                struct htable_data* tmp = entry->next;
                free(entry);
                entry = tmp;
            }
            while(entry != 0);
        }
    }
    free(bucket_array);
}

int htable_rehash(struct htable* table){
    struct htable_data* tmp_buckets = table->bucket_array;
    table->bucket_array = calloc(table->bucket_count, sizeof(struct htable_data));
    if(table->bucket_array == NULL){
        return -1;
    }
    for(int i = 0; i < table->bucket_count/2; i++){
        struct htable_data* entry = &tmp_buckets[i];
        while(entry != 0){
            //we skip empty links, and add filled ones to the new bucket array
            if(entry->key != 0){
                if(htable_add(table, entry->key, entry->key_len, entry->value) == -1){
                    free_buckets(table->bucket_array, table->bucket_count);
                    table->bucket_array = tmp_buckets;
                    return -1;
                }
            }
            entry = entry->next;
        }
    }
    free_buckets(tmp_buckets, table->bucket_count/2);
    return 0;
}

int htable_data_write(struct htable* table, struct htable_data* entry, const void* key, size_t key_len, void* value){
    entry->key = calloc(key_len, sizeof(char));
    if(entry->key == NULL){
        entry->key = 0;
        logmsg(LOG_ERR, "htable: Could not allocate enough memory to store mapping\n");
        return -1;
    }
    memcpy(entry->key, key, key_len);
    entry->key_len = key_len;\
    entry->value = value;
    return 0;
}

int htable_add(struct htable* table, const void* key, size_t key_len, void* value){
    size_t index = (hash(key, key_len) % table->bucket_count);
    if(htable_lookup(table, key, key_len) != NULL){
        logmsg(LOG_DEBUG, "htable: Failed to add mapping, key already exists\n");
        return -2;
    }
    //if you find an empty link within the chain, add your mapping there
    struct htable_data tmp = {.next = &table->bucket_array[index]};
    struct htable_data* entry = &tmp;
    while(entry->next != 0){
        entry = entry->next;
        if(entry->key == 0){
            if(htable_data_write(table, entry, key, key_len, value) == -1){
                return -1;
            }
            goto load_check;
        }
    }
    //otherwise, create a new link at the end of the chain
    entry->next = calloc(1, sizeof(struct htable_data));
    if(entry->next == NULL){
        entry->next = 0;
        logmsg(LOG_ERR, "htable: Could not allocate enough memory to store mapping\n");
        return -1;
    }
    if(htable_data_write(table, entry->next, key, key_len, value) == -1){
        return -1;
    }
    
    load_check:
    table->size++;
    //if loadfactor threshold is exceeded, double the table size and rehash
    if(htable_get_loadfactor(table) > table->load_threshold){
        size_t tmp_size = table->size;
        table->size = 0;
        table->bucket_count *= 2;
        if(htable_rehash(table) == -1){
            logmsg(LOG_ERR, "htable: Could not allocate enough memory for resize/rehash\n");
            table->bucket_count /= 2;
            table->size = tmp_size;
        }
        logmsg(LOG_DEBUG, "htable %p: Table resized and rehashed from %zu to %zu buckets\n", (void*)table, table->bucket_count/2, table->bucket_count);
    }
    logmsg(LOG_DEBUG, "htable %p: Added new mapping for key:%p - value:%p, table size: %zu\n", (void*)table, key, value, table->size);
    return 0;
}

void htable_destroy(struct htable* table){
    free_buckets(table->bucket_array, table->bucket_count);
    free(table);
}

void* htable_lookup(const struct htable* table, const void* key, size_t key_len){
    if(table == NULL){
        logmsg(LOG_DEBUG, "htable: NULL htable provided for lookup\n");
        return NULL;
    }
    size_t index = (hash(key, key_len) % table->bucket_count);
    struct htable_data* entry = &table->bucket_array[index];
    while(entry != 0){
        if(key_len == entry->key_len){
            if(memcmp(entry->key, key, key_len) == 0){
                return entry->value;
            }
        }
        entry = entry->next;
    }
    return NULL;
}

int htable_remove(struct htable* table, const void* key, size_t key_len){
    size_t index = (hash(key, key_len) % table->bucket_count);
    struct htable_data* entry = &table->bucket_array[index];
    while(entry != 0){
        if(key_len == entry->key_len){
            if(memcmp(entry->key, key, key_len) == 0){
                free(entry->key);
                entry->key = 0;
                entry->key_len = 0;
                entry->value = 0;
                table->size--;
                logmsg(LOG_DEBUG, "htable %p: Removed mapping for key:%p, table size: %zu\n", (void*)table, key, table->size);
                return 0;
            }
        }
        entry = entry->next;
    }
    return -1;
}

void htable_key_list_free(struct list* key_list, bool deep){
    if(key_list == NULL){
        return;
    }
    struct list* list_this = key_list;
    while(list_this != 0){
        if(deep){
            free(list_this->key);
        }
        struct list* tmp = list_this->next;
        free(list_this);
        list_this = tmp;
    }
}

struct list* htable_get_keys(const struct htable* table, bool deep){
    if(table->size == 0){
        logmsg(LOG_DEBUG, "htable %p: Table has no entries\n", (void*)table);
        return NULL;
    }
    struct list* key_list = calloc(1, sizeof(struct list));
    struct list* list_this = key_list;
    if(key_list == NULL){
        logmsg(LOG_ERR, "htable: Could not allocate memory for a new key list\n");
        return NULL;
    }
    for(int i = 0; i < table->bucket_count; i++){
        for(struct htable_data* entry = &table->bucket_array[i]; entry != 0; entry = entry->next){
            if(entry->key != 0){
                list_this->next = calloc(1, sizeof(struct list));
                list_this = list_this->next;
                if(list_this == NULL){
                    goto fail;
                }
                if(deep){
                    list_this->key = calloc(1, entry->key_len);
                    if(list_this->key == NULL){
                        goto fail;
                    }
                    memcpy(list_this->key, entry->key, entry->key_len);
                }
                else{
                    list_this->key = entry->key;
                }
                list_this->size = entry->key_len;
            }
        }
    }
    //The first link was there to make iteration easier, free it now
    struct list* tmp = key_list->next;
    free(key_list);
    return tmp;

    fail:
        logmsg(LOG_ERR, "htable: Could not allocate memory for a new key list\n");
        struct list* temp = key_list->next;
        free(key_list);
        htable_key_list_free(temp, deep);
        return NULL;
}

double htable_get_loadfactor(const struct htable* table){
    return (double)(table->size) / table->bucket_count;
}

void htable_set_loadfactor_threshold(struct htable* table, double threshold){
    table->load_threshold = threshold;
}

size_t htable_get_mapping_count(const struct htable* table){
    return table->size;
}

size_t htable_get_bucket_count(const struct htable* table){
    return table->bucket_count;
}
