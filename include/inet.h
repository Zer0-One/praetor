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

#ifndef PRAETOR_INET
#define PRAETOR_INET

#include <tls.h>

/**
 * Establishes a socket connection using the given host:service combination.
 * 
 * \param host The address of the host to connect to, either as a DNS name, an
 * IPv4 address in dot-decimal notation, or an IPv6 address in
 * hexadecimal-string notation.
 * \param service The port on the remote host to connect to, either as a
 * service name (see 'man services' for more information) or decimal port
 * number.
 * \return A socket file descriptor
 */
int inet_connect(const char* host, const char* service);

/**
 * Reads a single line from the connection associated with the given socket
 * file descriptor, \c fd, into the buffer pointed to by \c buf. Reading stops
 * after \c num characters have been read, or when the first newline character
 * is encountered.
 *
 * \param fd A socket file descriptor whose associated connection will be read
 * from.
 * \param buf The buffer into which characters will be read.
 * \param num The maximum number of characters to read.
 * \return The number of characters read.
 */
int inet_readline(int sock, void* buf, size_t num);

/**
 * Upgrades an already-established socket connection to a TLS connection.
 *
 * \param sock The socket file descriptor whose associated socket connection
 * will be upgraded.
 * \param host A hostname which will be used to verify the remote host's
 * certificate against the local certificate store.
 * \return On success, a libtls context (see 'man tls_client' for more
 * information), and NULL on failure.
 */
struct tls* inet_tls_connect(int sock, const char* host);

#endif
