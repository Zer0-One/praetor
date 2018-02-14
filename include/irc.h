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

#ifndef PRAETOR_IRC
#define PRAETOR_IRC

#include "ircmsg.h"

/**
 * Scans the receive queue belonging to the given network for a complete IRC
 * message. If a message is found, it is copied to \c buf .
 *
 * \param n   The network configuration that this function will apply to.
 * \param buf A buffer in which the next complete IRC message will be stored.
 * \param len The length of \c buf.
 *
 * \return 0 on success.
 * \return On failure to find a complete message, or if the given buffer is too
 * small to hold the next message, this function returns -1.
 */
int irc_recv(const struct network* n, char* buf, size_t len);

/**
 * Registers a connection with an IRC server by performing a PASS/NICK/USER
 * sequence (See RFC 2812, Section 3.1 for details).
 *
 * Note that if a user does not configure a connection password for the given
 * network, no PASS message will be queued.
 *
 * This function does not check to see if the server responded with an error,
 * it only ensures that the messages were queued for sending.
 * 
 * \param n The network configuration that this function will apply to.
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
int irc_register_connection(const struct network* n);

/**
 * Joins all channels configured for the given network. This function may not
 * partially succeed; if this function fails, no JOIN messages are queued, and
 * no channels are joined.
 *
 * This function only fails if the system is out of memory.
 *
 * \param n The network configuration that this function will apply to.
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
int irc_join_all(const struct network* n);

/**
 * This function queues a PONG message as a response to the given PING message
 * for the given network.
 *
 * \param n   The network configuration that this function will apply to.
 * \param msg An ircmsg struct of type PING for which to create and queue a
 *            PONG response message.
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
int irc_handle_ping(const struct network* n, const struct ircmsg* msg);

#endif
