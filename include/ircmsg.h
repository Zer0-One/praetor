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

#ifndef PRAETOR_IRCMSG
#define PRAETOR_IRCMSG

#include <limits.h>
#include <stdbool.h>

#include <jansson.h>

/**
 * The maximum number of command parameters in an IRC message.
 */
#define IRCMSG_CMD_PARAMS_MAX 15
/**
 * The maximum size, in bytes, of an IRC message, including the terminating
 * carriage-return and newline characters.
 */
#define IRCMSG_SIZE_MAX 512
/**
 * The maximum size, in bytes, of an IRC message, not including the terminating
 * carriage-return and newline characters.
 */
#define IRCMSG_BODY_MAX 510
/**
 * The maximum size, in bytes, of a buffer required to hold a null-terminated
 * IRC message string.
 */
#define IRCMSG_SIZE_BUF 513

enum ircmsg_type{
    PRIVMSG = 0,
    PING = 1,
    PONG = 2,
    UNKNOWN = INT_MAX
};

struct ircmsg_ping{
    char* server;
    char* server2;
};

struct ircmsg_pong{
    char* server;
    char* server2;
};

struct ircmsg_privmsg{
    char* target;
    char* msg;
    bool is_hilight;
    bool is_pm;
};

struct ircmsg{
    //Common fields
    enum ircmsg_type type;
    char* network;
    char* sender;
    char* user;
    char* host;
    char* cmd;
    //Command-specific fields
    union {
        struct ircmsg_privmsg* privmsg;
        struct ircmsg_ping* ping;
        struct ircmsg_pong* pong;
    };
};

/**
 * Parses the given IRC message into an ircmsg struct.
 *
 * The ircmsg struct returned by this function is dynamically allocated and
 * must be freed by the caller.
 *
 * \param network The network that the given message was destined to, or was
 *                received from.
 * \param msg     The IRC message to parse, with or without a terminating
 *                carriage-return and newline.
 * \param len     The length of the given IRC message.
 *
 * \return An dynamically-allocated ircmsg struct on success.
 * \return NULL on failure.
 */
struct ircmsg* ircmsg_parse(const char* network, const char* msg, size_t len);

/**
 * Frees the memory associated with the given ircmsg struct.
 *
 * This function is meant to work with ircmsg structs that are allocated both
 * dynamically or on the stack, and does not free the struct itself; it only
 * frees memory associated with the fields contained within the object.
 */
void ircmsg_free(struct ircmsg* msg);

/**
 * Packs the fields of the given ircmsg struct into a json_t JSON object.
 *
 * \return A JSON object on success.
 * \return NULL if the system was out of memory.
 */
json_t* ircmsg_to_json(const struct ircmsg* msg);

/**
 * Unpacks the given JSON object into an ircmsg struct.
 *
 * \return An ircmsg struct on success.
 * \return NULL on failure.
 */
struct ircmsg* ircmsg_from_json(const json_t* obj);

/**
 * The functions below implement the IRC message types described in RFC 2812
 * \<<https://tools.ietf.org/html/rfc2812>\>.
 *
 * On success, these functions return a dynamically-allocated string, and on
 * failure they return NULL.
 */
char* ircmsg_join(const char* channels, const char* keys);
char* ircmsg_nick(const char* nick);
char* ircmsg_pass(const char* pass);
char* ircmsg_pong(const char* server, const char* server2);
char* ircmsg_privmsg(const char* msgtarget, const char* text);
char* ircmsg_user(const char* user, const char* mode, const char* real_name);

#endif
