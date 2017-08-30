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

#ifndef PRAETOR_IRC
#define PRAETOR_IRC

#include <jansson.h>

/**
 * The maximum size of an IRC message, including the terminating
 * carriage-return and newline characters.
 */
#define MSG_SIZE_MAX 512
/**
 * The maximum size of an IRC message, not including the terminating
 * carriage-return and newline characters.
 */
#define MSG_BODY_MAX 510

/**
 * The number of times a message should be re-sent (after failing to be sent)
 * before giving up.
 */
#define MSG_RETRY 3

/**
 * Transforms a JSON message generated by a plugin into one or more IRC
 * messages, suitable to be sent as-is via IRC.
 *
 * \param json_msg A pointer to the JSON message to be converted.
 *
 * \return The number of messages in the returned array.
 * \return 0 on failure.
 */
//size_t irc_msg_from_json(json_t* json_msg, char*** irc_msg);

/**
 * Transforms one or more messages received from an IRC network into a JSON
 * message, suitable to be sent as-is to a plugin.
 *
 * \param json_msg Something
 * \param irc_msg  Something
 * \param len      Something
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
//int irc_json_from_msg(json_t* json_msg, char*** irc_msg, size_t len);

/**
 * Builds an IRC message from the given format string.
 *
 * This function returns dynamically-allocated memory that should be freed by
 * the caller.
 *
 * \param fmt A printf() format string describing an IRC message.
 *
 * \return On success, returns a pointer to dynamically-allocated memory which
 *         should be freed when it is no longer needed.
 * \return On failure, returns NULL.
 */
char* irc_msg_build(const char* fmt, ...);

/**
 * Scans the receive queue belonging to the given network for a complete IRC
 * message. If a message is found, it is copied to \c buf .
 *
 * \param network A string indexing a struct networkinfo in the rc_network hash
 *                table.
 * \param buf     A buffer in which the next complete IRC message will be
 *                stored.
 * \param len     The length of \c buf.
 *
 * \return 0 on success.
 * \return On failure to find a complete message, or if the given buffer is too
 * small to hold the next message, this function returns -1.
 */
int irc_msg_recv(const char* network, char* buf, size_t len);

/**
 * Registers a connection with an IRC server by performing a PASS/NICK/USER
 * sequence (See RFC 2812, Section 3.1 for details).
 *
 * This function does not check to see if the server responded with an error.
 * It only ensures that the message was queued for sending.
 * 
 * \param network A string indexing a struct networkinfo in the rc_network hash
 *                table.
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
int irc_register_connection(const char* network);

/**
 * Joins the given set of channels using the given keys.
 *
 * This function only fails if the system is out of memory.
 *
 * \param channels As described in RFC 2812, a comma-delimited list of
 *                 channels.
 * \param keys     As described in RFC 2812, a comma-delimited list of keys. If
 *                 no keys are necessary, this parameter may be set to NULL.
 *
 * \return 0 on success.
 * \return -1 on failure.
 */
int irc_join(const char* network, const char* channels, const char* keys);

/**
 * Scans a message to determine if it is a PING message, and if so, sends the
 * appropriate PONG response back to the originating server. This function
 * expects \c buf to be null-terminated.
 *
 * This function can fail if the socket connection dies before the PONG
 * response can be sent, or if the system is out of memory.
 *
 * \param network A string indexing a networkinfo struct in the rc_network hash
 *                table.
 * \param buf     A buffer containing a null-terminated IRC message to parse.
 * \param len     The number of characters in the message contained in \c buf,
 *                including the terminating newline character.
 *
 * \return 0 on success.
 * \return -1 if \buf did not contain a PING message.
 * \return -2 if the system is out of memory.
 */
int irc_handle_ping(const char* network, const char* buf);

#endif
