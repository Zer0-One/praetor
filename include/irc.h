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
 * The maximum size of an IRC message
 */
#define MSG_SIZE 512

/**
 * Establishes a socket connection to an IRC server. This function modifies the
 * \c networkinfo struct indexed by the \c network argument by setting its \c
 * sock and \c ctx pointers to their appropriate values.
 * 
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \return On success, this function returns a socket file descriptor. If the
 * given network was configured for SSL, you should not attempt to write or
 * read data via this descriptor directly; use the libtls API instead. On
 * failure, this function returns -1.
 */
int irc_connect(const char* network);

/**
 * Registers a connection with an IRC server by performing a PASS/NICK/USER
 * sequence (See RFC 2812, Section 3.1 for details).
 *
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \return 0 upon successfully registering the connection, and -1 on error.
 */
int irc_register_connection(const char* network);

/**
 * Performs a graceful disconnect from an IRC network. This function is
 * guaranteed to disconnect praetor from the given IRC network by performing an
 * IRC QUIT, and terminating the socket connection; it does so gracefully if
 * possible, and ungracefully if necessary. This function also removes the
 * socket file descriptor belonging to the given network from the global
 * watchlist.
 * 
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \return 0 on a successful disconnect (graceful or not), and -1 if no
 * configuration could be found for the given network handle.
 */
int irc_disconnect(const char* network, const char* msg);

/**
 * Joins a channel on the given IRC network. If \c channel is a valid handle in
 * the channels hash table for the given network, praetor will connect
 * according to the information for the corresponding channel struct (channel
 * password, etc.).  Otherwise, praetor simply attempts to perform a JOIN using
 * \c channel as the channel name.
 * 
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \param channel One of either: a string indexing a channel struct in the
 * channels hash table, or the name of a valid IRC channel.
 * \return 0 on a successful channel join, -1 on an unsuccessful join, and 1 if
 * the socket connection to the server was closed.
 */
int irc_channel_join(const char* network, const char* channel);

/**
 * Establishes a socket connection to an IRC server, adds the connected socket
 * to the global watchlist, registers an IRC connection, and joins every
 * channel configured for the given network. If any one of these steps fail,
 * (with the exception of failure to successfully join a channel) this function
 * fails.
 *
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \return 0 on success, and -1 on failure.
 */
int irc_connect_all(const char* network);

/**
 * Parts a channel on the given IRC network.
 * 
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \param channel A string indexing a channel struct in the channels hash
 * table.
 * \return 
 */
int irc_channel_part(const char* network, const char* channel);

/**
 * Sends a message of length \c len, read from buffer \c buf, to the given IRC
 * network. If the entire message cannot be sent, 
 *
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \param buf The buffer that the message to send will be read from.
 * \param len The length of the message to send.
 * \return On success, returns the number of characters sent from the supplied
 * buffer. On failure, returns -1.
 */
int irc_msg_send(const char* network, const char* buf, size_t len);

/**
 * Reads and removes a complete line, including terminating newline character,
 * from the message buffer for the given network.
 *
 * Note: \c buf must be at least \c MSG_SIZE chars in length, otherwise this
 * function may cause a buffer overflow.
 *
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \param buf The buffer that the read line will be stored in.
 * \param len The size of \buf in char-sized units.
 * \return On success, returns the number of characters in the line read into
 * \c buf, including the terminating newline character. On failure, returns -1.
 */
ssize_t irc_msg_recv(const char* network, char* buf, size_t len);

/**
 * Processes the given message for a numeric code, and takes action
 * accordingly.
 */
void irc_handle_numeric(const char* msg);

/**
 * Processes an IRC message for the presence of a PING, and sends the
 * appropriate PONG response back to the originating server. Only the first \c
 * len characters of the message are processed.
 *
 * This function will only fail if the socket connection dies before the PONG
 * response can be sent.
 *
 * \param network A string indexing a networkinfo struct in the rc_network hash
 * table.
 * \param buf A buffer containing a complete (newline-terminated) IRC message
 * to parse.
 * \param len The number of characters in the message contained in \c buf,
 * including the terminating newline character.
 * \return On success, returns 0. On failure, returns -1.
 */
int irc_handle_ping(const char* network, const char* buf, size_t len);

#endif
