#include <stdint.h>
#include <stdlib.h>
#include "hashtable.h"
#include "../log.h"

struct htable* htable_create(size_t size){
    if(size == 0){
        return NULL;
    }
    struct htable* table = calloc(1, sizeof(struct htable));
    table->bucket_array = calloc(size, sizeof(struct htable_data));
    if(table->bucket_array == NULL || table == NULL){
        logmsg(LOG_ERR, "htable: could not allocate memory for a new hash table\n");
        return NULL;
    }
    table->bucket_count = size;
    table->load_threshold = .75;
    return table;
}

double htable_loadfactor(struct htable* table){
    return (double)(table->size) / table->bucket_count;
}

/**
 * An implementation of Bob Jenkins's one-at-a-time hash function
 */
uint32_t hash(char* key, size_t len){
    uint32_t hash, i;
    for(hash = i = 0; i < len; ++i)
    {
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

int htable_add(struct htable* table, void* key, size_t key_len, void* value){
    size_t index = (hash(key, key_len) % table->bucket_count);
    //if you find an empty link within the chain, add your mapping there
    struct htable_data* entry = &table->bucket_array[index];
    do{
        if(entry->key == 0){
            table->bucket_array[index].key = key;
            table->bucket_array[index].key_len = key_len;
            table->bucket_array[index].value = value;
            goto load_check;
        }
        entry = entry->next;
    } while(entry->next != 0);
    //otherwise, create a new link at the end of the chain
    entry->next = calloc(1, sizeof(struct htable_data));
    if(entry->next == NULL){
        logmsg(LOG_ERR, "htable: could not allocate enough memory to store mapping for key at: %p\n", key);
        return -1;
    }
    entry->next->key = key;
    entry->next->key_len = key_len;
    entry->next->value = value;
    load_check:
    table->size++;
    //if loadfactor threshold is exceeded, double the table size and rehash
    if(htable_loadfactor(table) > table->load_threshold){
        table->bucket_count *= 2;
        if(htable_rehash(table) == -1){
            logmsg(LOG_ERR, "htable: could not allocate enough memory for resize/rehash\n");
            table->bucket_count /= 2;
        }
    }
    return 0;
}


void htable_destroy(struct htable* table){
    free_buckets(table->bucket_array, table->bucket_count);
    free(table);
}

void* htable_lookup(struct htable* table, void* key, size_t key_len){
    size_t index = (hash(key, key_len) % table->bucket_count);
    struct htable_data* entry = &table->bucket_array[index];
    while(entry != 0){
        if(entry->key == key){
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

int htable_remove(struct htable* table, void* key, size_t key_len){
    size_t index = (hash(key, key_len) % table->bucket_count);
    struct htable_data* entry = &table->bucket_array[index];
    while(entry != 0){
        if(entry->key == key){
            entry->key = 0;
            entry->key_len = 0;
            entry->value = 0;
            return 0;
        }
        entry = entry->next;
    }
    return -1;
}
