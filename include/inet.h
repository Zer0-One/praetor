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

#ifndef PRAETOR_INET
#define PRAETOR_INET

#include <tls.h>

/**
 * Performs DNS lookup for the host configured for the given network, and
 * stores the resulting list of struct addrinfo in the \c addr field.
 *
 * \param n The network configuration that this function will apply to.
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
int inet_getaddrinfo(struct network* n);

/**
 * Establishes a socket connection using the host:service combination
 * configured for the given network.
 *
 * This function performs the following steps, in order: 
 *  1. Populates \c addr with the list of addrinfo structs returned by a DNS lookup
 *  2. Creates a socket and sets \c sock to it.
 *  3. Allocates a send queue, and points \c send_queue to it.
 *  4. Alocates a receive queue, points \c recv_queue to it, and sets \c
 *     recv_queue_size to the size of the receive queue.
 *  5. Initiates a connection to the network. If this would block, skip to #7.
 *  6. Upgrades the connection to a TLS connection.
 *  7. Adds the socket to the global monitor list.
 *  8. Adds a mapping for the socket to rc_network_sock.
 *
 * Regarding step #6, if the connection could be established immediately, then
 * the socket will be monitored for readability. If establishing the connection
 * would have blocked, the socket will be monitored for writeability, and
 * inet_check_connection() should be called when the socket is writeable.
 *
 * On connection failure (and not any other failure reason), this function
 * increments the network's \c addr_idx counter to point to the next address to
 * be tried. If this function fails to connect using the last valid address in
 * the list, it frees and NULLs \c addr, sets \c addr_idx to INT_MAX, and will
 * fail immediately on every subsequent invocation until \c addr_idx is reset
 * to 0.
 *
 * \param n The network configuration that this function will apply to.
 *
 * \return 1 if the connection could not be established immediately.
 * \return 0 on immediate success.
 * \return -1 on failure to connect using the address currently selected by the
 *         network's \c addr_idx field.
 * \return -2 on failure to connect to all addresses associated with the
 *         network.
 */
int inet_connect(struct network* n);

/**
 * Calls \c inet_connect() for every network present in \c rc_network. This
 * function will only fail if the system is out memory.
 * 
 * \return 0 on success.
 * \return -1 on failure.
 */
int inet_connect_all();

/**
 * Verifies that a non-blocking connect() to the given network completed
 * successfully. On success, this function:
 *  1. Removes the socket from the global monitor list.
 *  2. Upgrades the connection to a TLS connection (if necessary)
 *  3. Re-adds the socket to the global monitor list to monitor for readability.
 *
 * On failure, this function:
 *  1. Removes the mapping for the socket from \c rc_network_sock .
 *  2. Removes the socket from the global monitor list.
 *  3. Increments \c addr_idx.
 *  4. Closes the socket.
 *
 * \param n The network configuration that this function will apply to.
 *
 * \return 0 on connection success.
 * \return -1 if the connection failed.
 */
int inet_check_connection(struct network* n);

/**
 * For the given network, this function:
 *  1. Closes its socket.
 *  2. Removes its socket from the global monitor list.
 *  3. Removes its \c rc_network_sock mapping.
 *  4. De-allocates its send and receive queues.
 *
 * \param n The network configuration that this function will apply to.
 *
 * \return 0 on success.
 * \return -1 if no configuration could be found for the given network.
 */
int inet_disconnect(struct network* n);

/**
 * Upgrades an already-established socket connection to a TLS connection.
 *
 * The hostname associated with the given network will be used to verify the
 * remote host's certificate against the local certificate store. It is not
 * necessary to check for whether or not the given network is configured for
 * TLS before calling this function; it may be called after any connection.
 *
 * On success, stores the resultant libtls context in the \c ctx field of the
 * given network.
 *
 * \param n The network configuration that this function will apply to.
 *
 * \return 0 on success, or if the given network was not configured for TLS.
 * \return -1 on failure.
 */
int inet_tls_upgrade(struct network* n);

/**
 * Reads from the socket belonging to the given network until its receive queue
 * is full, or until reading would block.
 *
 * If reading fails due to a connection issue, this function initiates a
 * restart of the connection via inet_disconnect() and inet_connect().
 *
 * \param n The network configuration that this function will apply to.
 *
 * \return 0 on success.
 * \return -1 on failure to read from the given socket.
 */
int inet_recv(struct network* n);

/**
 * Sends a message on the socket belonging to the given network.
 *
 * If the message could not be sent for any reason, it is discarded.
 *
 * If the message could not be sent due to a connection issue, this function
 * initiates a restart of the connection via inet_disconnect() and
 * inet_connect().
 *
 * \param n   The network configuration that this function will apply to.
 * \param buf A buffer containing the message to send.
 * \param len The number of characters from \c buf to send.
 *
 * \return 0 on success.
 * \return -1 on failure to send via the given socket.
 */
int inet_send_immediate(struct network* n, const char* buf, size_t len);

/**
 * Attempts to send all messages currently in the given network's send queue.
 *
 * This function calls inet_send_immediate() in order to send each message.
 *
 * \param n The network configuration that this function will apply to.
 *
 * \return 0 on success, or if the system was out of memory.
 * \return -1 on failure to send one or more messages.
 */
int inet_send(struct network* n);

/**
 * Calls inet_send() for every network that is actively connected.
 *
 * This function only fails if the system is out of memory.
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
int inet_send_all();

#endif
