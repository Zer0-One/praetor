/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015, David Zero
* All rights reserved.
*
* The following code is licensed for use, modification, and redistribution
* according to the terms of the Revised BSD License. The text of this license
* can be found in the "LICENSE" file bundled with this source distribution.
*/

#ifndef PRAETOR_HASHTABLE
#define PRAETOR_HASHTABLE

#include <stdbool.h>

/** \struct htable
 * The htable struct represents a chaining hash table. Keys are hashed using
 * <a href="http://www.burtleburtle.net/bob/hash/doobs.html">Bob Jenkins's one-at-a-time hash algorithm</a>.
 */
struct htable;

/**
 * A linked list used to enumerate the keys stored in a hashmap when
 * htable_get_keys() is called.
 */
struct list{
    /**
     * A pointer to a key stored in the hash table passed to htable_get_keys().
     * If htable_get_keys() was called with \b deep set to \i true, \b key
     * points to dynamically-allocated memory holding a copy of the key,
     * instead of pointing directly to the key stored within the table.
     */
    void* key;
    /**
     * The size, in char-sized units, of \b key.
     */
    size_t size;
    /**
     * A pointer to the next link in the linked list. If this is the last list
     * in the chain, \b next will be 0.
     */
    struct list* next;
};

/**
 * Creates a hash table with a caller-specified initial size. The returned \b
 * htable will maintain its initial size until its load factor reaches a
 * particular threshold (either caller-defined, or the default of .75), at
 * which point the table will be doubled in size to accommodate additional
 * mappings.
 *
 * \param size The initial size of the hash table. Space for \b size number of
 * \b buckets will be allocated. If \b size is set to 0, this function is
 * guaranteed to return NULL.
 * \return A pointer to the newly created hash table, and NULL if either \b
 * size is 0, or the system is out of memory.
 */
struct htable* htable_create(size_t size);

/**
 * Destroys the specified hash table, freeing all associated memory.
 *
 * \param table The hash table to destroy.
 */
void htable_destroy(struct htable* table);

/**
 * Adds a key-value pair to the specified hash table. If the system is out of
 * memory at the time this function is called, the function will return \b -1,
 * but the table will remain in a consistent state, and the function may be
 * called again when more memory is available. If a mapping for the given key
 * already exists within the table, the function will return 1, and leave the
 * existing mapping unmodified.
 *
 * \param table The hash table to which the given key and value will be added.
 * \param key The key to index the specified value. This key will be copied
 * into the hash table via memcpy() for use in resolving hash collisions.
 * \param key_len The size, in char-sized units, of the \b key.
 * \param value The value to store in the specified hash table.
 * \return 0 on success, -1 if the system is out of memory, and 1 if a mapping
 * for the given key already exists.
 */
int htable_add(struct htable* table, void* key, size_t key_len, void* value);

/**
 * Removes a key-value pair from the specified hash table
 *
 * \param table The hash table from which the given key and value will be
 * removed.
 * \param key The key to index the specified value.
 * \param key_len The size, in char-sized units, of the \b key.
 * \return 0 on success, and -1 if the given key did not index any values.
 */
int htable_remove(struct htable* table, void* key, size_t key_len);

/**
 * Performs a lookup for the value mapped to the given key in the specified
 * hash table. When a bucket stores multiple mappings, the correct entry is
 * found by comparing the given key and the stored key with memcmp. The given
 * key must match the stored key exactly in both size and content.
 *
 * \param table The hash table to be searched for the specified key.
 * \param key The key indexing the value being looked up.
 * \param key_len The size, in char-sized units, of the \b key.
 * \return A pointer to the mapped value on success, or NULL if no mapping for
 * \b key was found in the given hash table.
 */
void* htable_lookup(const struct htable* table, void* key, size_t key_len);

/**
 * Generates a linked list of all keys contained within the given hash table.
 *
 * \param deep If set to true, the returned linked list will contain copies of
 * the keys stored within this table instead of pointers to the keys stored
 * within the table.
 * \return A linked list of keys stored within the given hash table.
 */
struct list* htable_get_keys(const struct htable* table, bool deep);

/**
 * Frees a linked list generated via a call to htable_get_keys().
 *
 * \param key_list The linked list of keys whose memory will be freed.
 * \param deep This value must match the value of \b deep passed to
 * htable_get_keys() to generate the linked list. If htable_get_keys() was
 * called with \b deep set to true, and this function is called with \b deep
 * set to false, memory will be leaked. If the opposite is true, behavior is
 * undefined.
 */
void htable_key_list_free(struct list* key_list, bool deep);

/**
 * Calculates the current load factor of the specified hash table. The load
 * factor is given by \b (n/k), where \b n is the number of key-value mappings,
 * and \b k is the number of buckets.
 *
 * \param table The table whose load factor will be calculated.
 * \return The calculated load factor of the specified hash table.
 */
double htable_get_loadfactor(const struct htable* table);

/**
 * \return The current load factor threshold.
 */
double htable_get_loadfactor_threshold(const struct htable* table);

/**
 * Sets the load factor value at which the table will automatically resize
 * itself. If the given threshold is either 0 or a negative value, the table
 * will double in size with every addition. If the given threshold is larger
 * than 1, performance of the hash table may be severely degraded.
 *
 * \param table The table whose load factor threshold will be set.
 */
void htable_set_loadfactor_threshold(struct htable* table, double threshold);

/**
 * \return The number of key-value mappings stored within the given hash table.
 */
size_t htable_get_mapping_count(const struct htable* table);

/**
 * \return The number of buckets stored within the given hash table.
 */
size_t htable_get_bucket_count(const struct htable* table);

#endif
