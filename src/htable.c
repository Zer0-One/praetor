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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "htable.h"
#include "log.h"

#define LOAD_THRESHOLD .75

struct htable_entry{
    struct htable_entry* next;
    void* value;
    size_t key_size;
    uint8_t key[];
};

struct htable{
    struct htable_entry** buckets;
    size_t mapping_count;
    size_t bucket_count;
    double load_threshold;
};

uint32_t hash(const uint8_t* key, size_t len){
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

struct htable* htable_create(size_t size){
    if(size == 0){
        return NULL;
    }

    struct htable* table = calloc(1, sizeof(struct htable));
    if(table == NULL){
        logmsg(LOG_DEBUG, "htable: Could not allocate memory for a new table\n");
        return NULL;
    }

    table->buckets = malloc(size * sizeof(struct htable_entry*));
    if(table->buckets == NULL){
        logmsg(LOG_DEBUG, "htable %p: Could not allocate memory for a new table\n", (void*)table);
        free(table);
        return NULL;
    }
    //Initialize the buckets to NULL
    for(size_t i = 0; i < size; i++){
        table->buckets[i] = NULL;
    }

    table->bucket_count = size;
    table->load_threshold = LOAD_THRESHOLD;
    logmsg(LOG_DEBUG, "htable %p: Created new hash table\n", (void*)table);
    return table;
}

void htable_destroy(struct htable* table){
    if(table == NULL){
        logmsg(LOG_DEBUG, "htable: Cannot destroy NULL table\n");
        return;
    }

    for(size_t i = 0; i < table->bucket_count; i++){
        struct htable_entry* initial_entry = table->buckets[i];
        //If this bucket is empty, continue
        if(initial_entry == NULL){
            continue;
        }

        //If there are chained elements, free them
        struct htable_entry* this = initial_entry->next;
        while(this != NULL){
            struct htable_entry* tmp = this->next;
            free(this);
            this = tmp;
        }
        //Free the initial element
        free(initial_entry);
    }

    free(table->buckets);
    free(table);
}

//Rehashing is done this way to avoid invalidating the user's pointer after a call to htable_add()
int htable_rehash(struct htable* table, size_t scale){
    //Create a temporary table
    struct htable* tmp_table = htable_create(table->bucket_count * scale);
    if(tmp_table == NULL){
        return -1;
    }

    //Enumerate keys from the table that needs to be rehashed
    size_t size = 0;
    struct htable_key** key_list = htable_get_keys(table, &size);
    if(key_list == NULL){
        htable_destroy(tmp_table);
        return -1;
    }

    //Add all entries into the temporary table
    for(size_t i = 0; i < size; i++){
        void* value = htable_lookup(table, key_list[i]->key, key_list[i]->key_size);
        if(htable_add(tmp_table, key_list[i]->key, key_list[i]->key_size, value) == -2){
            htable_destroy(tmp_table);
            return -1;
        }
    }

    //Swap the bucket arrays between tables
    struct htable_entry** tmp_buckets = table->buckets;
    table->buckets = tmp_table->buckets;
    tmp_table->buckets = tmp_buckets;

    //Update bucket counts
    table->bucket_count *= scale;
    tmp_table->bucket_count /= scale;

    //Clean-up
    htable_key_list_free(key_list, size);
    htable_destroy(tmp_table);

    return 0;
}

int htable_add(struct htable* table, const uint8_t* key, size_t key_size, void* value){
    if(table == NULL){
        logmsg(LOG_DEBUG, "Cannot add mapping to NULL table\n");
        return -1;
    }
    if(key == NULL || key_size == 0){
        logmsg(LOG_DEBUG, "htable %p: Cannot map NULL or 0-length key\n", (void*)table);
        return -1;
    }
    if(value == NULL){
        logmsg(LOG_DEBUG, "htable %p: Cannot map NULL value\n", (void*)table);
        return -1;
    }

    if(htable_lookup(table, key, key_size) != NULL){
        logmsg(LOG_DEBUG, "htable %p: Cannot add mapping, key already present within table\n", (void*)table);
        return -1;
    }
    
    size_t index = hash(key, key_size) % table->bucket_count;

    struct htable_entry* this = table->buckets[index];
    //If this bucket is empty, add the first entry
    if(this == NULL){
        this = calloc(1, sizeof(struct htable_entry) + (sizeof(uint8_t) * key_size));
        if(this == NULL){
            logmsg(LOG_DEBUG, "htable %p: Cannot add mapping, the system is out of memory\n", (void*)table);
            return -2;
        }
        //Update the table to point to the new entry
        table->buckets[index] = this;
    }
    //Otherwise, add an element to the end of the list
    else{
        while(this->next != NULL){
            this = this->next;
        }

        this->next = calloc(1, sizeof(struct htable_entry) + (sizeof(uint8_t) * key_size));
        if(this->next == NULL){
            logmsg(LOG_DEBUG, "htable %p: Cannot add mapping, the system is out of memory\n", (void*)table);
            return -2;
        }
        this = this->next;
    }

    //Copy the values into the entry
    memcpy(this->key, key, key_size);
    this->value = value;
    this->key_size = key_size;
    this->next = NULL;

    //Increment the mapping counter, and check if we need to rehash
    table->mapping_count++;
    if(htable_get_load_factor(table) > table->load_threshold){
        logmsg(LOG_DEBUG, "htable %p: Load factor threshold exceeded, initiating rehash\n", (void*)table);
        if(htable_rehash(table, 2) == -1){
            logmsg(LOG_DEBUG, "htable %p: Could not allocate enough memory to complete a rehash\n", (void*)table);
        }
        else{
            logmsg(LOG_DEBUG, "htable %p: Resized and rehashed: %zd buckets, %zd mappings, %.2f load-factor\n", (void*)table, table->bucket_count, table->mapping_count, htable_get_load_factor(table));
        }
    }

    return 0;
}

int htable_remove(struct htable* table, const uint8_t* key, size_t key_size){
    if(table == NULL){
        logmsg(LOG_DEBUG, "htable: Cannot remove mapping from NULL table\n");
        return -1;
    }
    if(key == NULL || key_size == 0){
        logmsg(LOG_DEBUG, "htable %p: Cannot remove mapping for NULL or 0-length key\n", (void*)table);
        return -1;
    }
    
    size_t index = hash(key, key_size) % table->bucket_count;

    struct htable_entry* this = table->buckets[index];
    struct htable_entry* prev_entry = this;

    for(; this != NULL; this = this->next){
        //The sizes must match, or undefined behavior will occur
        if(this->key_size == key_size && memcmp(this->key, key, key_size) == 0){
            //If this was the first entry in the bucket:
            if(table->buckets[index] == this){
                //If there are entries below this, move them up
                if(this->next != NULL){
                    table->buckets[index] = this->next;
                }
                else{
                    table->buckets[index] = NULL;
                }
            }
            //If this was a chained entry:
            else{
                //Get a pointer to the entry before this one
                while(prev_entry->next != this){
                    prev_entry = prev_entry->next;
                }

                //If there are entries below this, move them up
                prev_entry->next = this->next;
            }

            free(this);
            return 0;
        }
    }

    return -1;
}

void* htable_lookup(const struct htable* table, const uint8_t* key, size_t key_size){
    if(table == NULL){
        logmsg(LOG_DEBUG, "htable: Cannot lookup mapping from NULL table\n");
        return NULL;
    }
    if(key == NULL || key_size == 0){
        logmsg(LOG_DEBUG, "htable %p: Cannot lookup mapping for NULL or 0-length key\n", (void*)table);
        return NULL;
    }
    
    size_t index = hash(key, key_size) % table->bucket_count;

    struct htable_entry* this = table->buckets[index];
    for(; this != NULL; this = this->next){
        if(this->key_size == key_size && memcmp(this->key, key, key_size) == 0){
            return this->value;
        }
    }

    return NULL;
}

struct htable_key** htable_get_keys(const struct htable* table, size_t* size){
    if(table == NULL){
        logmsg(LOG_DEBUG, "htable: Cannot get keys for NULL table\n");
        return NULL;
    }
    if(table->mapping_count == 0){
        logmsg(LOG_DEBUG, "htable %p: Cannot get keys for table with no entries\n", (void*)table);
        return NULL;
    }

    //Create the key list
    struct htable_key** keys = malloc(table->mapping_count * sizeof(struct htable_key*));
    if(keys == NULL){
        logmsg(LOG_DEBUG, "htable %p: Could not allocate enough memory to get keys\n", (void*)table);
        return NULL;
    }

    //Populate the array
    size_t keys_idx = 0;
    for(size_t i = 0; i < table->bucket_count; i++){
        for(struct htable_entry* entry = table->buckets[i]; entry != NULL; entry = entry->next){
            keys[keys_idx] = malloc(sizeof(struct htable_key) + entry->key_size);
            if(keys[keys_idx] == NULL){
                logmsg(LOG_DEBUG, "htable %p: Could not allocate enough memory to get keys\n", (void*)table);
                goto fail;
            }

            memcpy(keys[keys_idx]->key, entry->key, entry->key_size);
            keys[keys_idx]->key_size = entry->key_size;
            
            keys_idx++;
        }
    }

    //This should never happen.
    if(keys_idx != table->mapping_count){
        logmsg(LOG_ERR, "htable %p: htable_get_keys() returned more/less keys than exist in the table\n", (void*)table);
        _exit(-1);
    }

    *size = keys_idx;

    return keys;

    fail:
        //Free as much of the array as we managed to created
        for(size_t i = 0; i < keys_idx; i++){
            free(keys[i]);
        }
        free(keys);
        return NULL;
}

void htable_key_list_free(struct htable_key** keys, size_t size){
    if(keys == NULL){
        logmsg(LOG_DEBUG, "htable: Cannot free NULL key list\n");
        return;
    }
    for(size_t i = 0; i < size; i++){
        free(keys[i]);
    }
    free(keys);
}

double htable_get_load_factor(const struct htable* table){
    return (double)table->mapping_count / (double)table->bucket_count;
}

double htable_get_load_threshold(const struct htable* table){
    return table->load_threshold;
}

void htable_set_load_threshold(struct htable* table, double threshold){
    table->load_threshold = threshold;
}

size_t htable_get_mapping_count(const struct htable* table){
    return table->mapping_count;
}
