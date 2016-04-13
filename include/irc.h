/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015,2016 David Zero
* All rights reserved.
*
* The following code is licensed for use, modification, and redistribution
* according to the terms of the Revised BSD License. The text of this license
* can be found in the "LICENSE" file bundled with this source distribution.
*/

#ifndef PRAETOR_IRC
#define PRAETOR_IRC

/**
 * Connects to an IRC network, and adds the resultant socket descriptor to the
 * list of monitored descriptors. This function modifies the networkinfo struct
 * indexed by the network argument by setting the sock and ctx pointers to
 * their appropriate values.
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \return A socket file descriptor. If the given network was configured for
 * SSL, you should not attempt to write or read data via this descriptor
 * directly; use the libtls API instead.
 */
int irc_connect(const char* network);

/**
 * Performs a graceful disconnect from an IRC network. This function is
 * guaranteed to disconnect praetor from the given IRC network by performing an
 * IRC QUIT, and terminating the socket connection; it does so gracefully if
 * possible, and ungracefully if necessary.
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \return 0 on a successful graceful disconnect, and -1 on an ungraceful
 * disconnect.
 */
int irc_disconnect(const char* network);

/**
 * Joins a channel on the given IRC network.
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \param channel A string indexing a channel struct in the channels hash
 * table.
 * \return 0 on a successful channel join, -1 on an unsuccessful join, and 1 if
 * the socket connection to the server was closed.
 */
int irc_channel_join(const char* network, const char* channel);

/**
 * Parts a channel on the given IRC network.
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \return 
 */
int irc_channel_part(const char* network, const char* channel);

#endif
