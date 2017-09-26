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

/** \file ircmsg.h
 * The functions in this module implement the IRC message types described in
 * RFC 2812 \<<https://tools.ietf.org/html/rfc2812>\>.
 *
 * These functions automatically queue the generated message for sending.
 *
 * On success, they return 0, and on failure they return -1. Currently, these
 * functions can only fail if the system is out of memory.
 */

#ifndef PRAETOR_IRCMSG
#define PRAETOR_IRCMSG

/**
 * \param channels As described in RFC 2812, a comma-delimited list of
 *                 channels.
 * \param keys     As described in RFC 2812, a comma-delimited list of keys. If
 *                 no keys are necessary, this parameter may be set to NULL.
 */
int irc_join(const struct network* n, const char* channels, const char* keys);
int irc_nick(const struct network* n, const char* nick);
int irc_pass(const struct network* n, const char* pass);
int irc_pong(const struct network* n, const char* sender);
int irc_user(const struct network* n, const char* user, const char* real_name);

#endif
