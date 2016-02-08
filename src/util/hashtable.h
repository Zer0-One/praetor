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

/** \struct htable_data
 * A struct representing a hash table entry. An instance of this struct is what
 * is stored in the "bucket array". When collisions occur, this struct is able
 * to act as a link in a linked list via its \b next pointer.
 */
struct htable_data;

/** \struct htable
 * The htable struct represents a chaining hash table, in which the buckets are
 * htable_data structs. Keys are hashed using
 * <a href="http://www.burtleburtle.net/bob/hash/doobs.html">Bob Jenkins's one-at-a-time hash algorithm</a>.
 */
struct htable;

/**
 * Creates a hash table with a user-specified initial size. The returned \b
 * htable will maintain its initial size until its load factor reaches a
 * particular threshold (either user-defined, or the default of .75), at which
 * point the table will be doubled in size to accommodate additional mappings.
 *
 * \param size The initial size of the hash table. Space for \b size number of
 * \b htable_data structs will be allocated. If \b size is set to 0, this
 * function is guaranteed to return NULL.
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
 * into the hash table for use in resolving hash collisions.
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
void* htable_lookup(struct htable* table, void* key, size_t key_len);

/**
 * Calculates the current load factor of the specified hash table. The load
 * factor is given by \b (n/k), where \b n is the number of key-value mappings,
 * and \b k is the number of buckets.
 *
 * \param table The table whose load factor will be calculated.
 * \return The calculated load factor of the specified hash table.
 */
double htable_get_loadfactor(struct htable* table);

/**
 * \return The current load factor threshold.
 */
double htable_get_loadfactor_threshold(struct htable* table);

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
size_t htable_get_mapping_count(struct htable* table);

/**
 * \return The number of buckets stored within the given hash table.
 */
size_t htable_get_bucket_count(struct htable* table);

#endif
