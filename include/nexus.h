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

#ifndef PRAETOR_NEXUS
#define PRAETOR_NEXUS

#include <stdbool.h>

/**
 * Adds a socket file descriptor to the global file descriptor monitor list by
 * wrapping it in a pollfd struct.
 *
 * \param fd         A valid socket file descriptor.
 * \param connection If the given socket file descriptor is meant to be monitored for
 *                   a completed connection (instead of monitored for messages
 *                   waiting in the queue), this parameter should be set \c true.
 *
 * \return 0 if the file descriptor was successfully added to the monitor list.
 * \return -1 on an out-of-memory condition.
 */
int watch_add(int fd, bool connection);

/**
 * Removes a file descriptor from the global file descriptor monitor list.
 *
 * Note that this function does not free memory; the pollfd struct created when
 * watch_add() was called will be cleared, and the memory will be re-used on
 * the next call to watch_add().
 *
 * \param fd A valid file descriptor.
 */
void watch_remove(int fd);

/**
 * This function monitors all open file descriptors for activity via poll(),
 * and dispatches incoming and outgoing messages.
 *
 * This function should be called in a loop as often as possible.
 */
void run();

#endif
