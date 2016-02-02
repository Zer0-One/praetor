#ifndef PRAETOR_HASHTABLE
#define PRAETOR_HASHTABLE

/**
 * A struct representing a hash table entry. An instance of this struct is what
 * is stored in the "bucket array". When collisions occur, this struct is able
 * to act as a link in a linked list via the \b next pointer.
 */
struct htable_data{
    void* key;
    size_t key_len;
    void* value;
    struct htable_data* next;
};

/**
 * The htable struct represents a chaining hash table, in which the buckets are
 * \b htable_data structs. The fields of this struct, with the exception of \b
 * load_threshold, are all managed internally, and should *never* be modified
 * by the user.
 */
struct htable{
    /**
     * An array of buckets (htable_data structs) containing the stored
     * key-value pairs
     */
    struct htable_data* bucket_array;
    /**
     * The number of key-value pairs mapped within this hash table.
     */
    size_t size;
    /**
     * The number of buckets contained within this hash table.
     */
    size_t bucket_count;
    /**
     * The maximum load factor of this hash table. If the table exceeds this
     * threshold, it will be automatically doubled in size. This value is set
     * to .75 by default, and may be modified by the user to change the
     * automatic resizing behavior of this table.
     */
    double load_threshold;
};

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
 * Adds a key-value pair to the specified hash table. The given key is hashed
 * using Bob Jenkins's one-at-a-time hash algorithm. If the system is out of
 * memory at the time this function is called, the function will return \b -1,
 * but the table will remain in a consistent state, and the function may be
 * called again when more memory is available.
 *
 * \param table The hash table to which the given key and value will be added.
 * \param key The key to index the specified value.
 * \param key_len The size, in char-sized units, of the \b key.
 * \param value The value to store in the specified hash table.
 * \return 0 on success, and -1 if the system is out of memory and the mapping
 * could not be added to the table.
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
 * hash table.
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
double htable_loadfactor(struct htable* table);

#endif
