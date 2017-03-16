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

#ifndef PRAETOR_CONFIG
#define PRAETOR_CONFIG

#include <stdbool.h>
#include <tls.h>
#include "hashtable.h"

/**
 * A pointer to the global struct containing praetor's daemon-specific
 * configuration.
 */
extern struct praetor* rc_praetor;
/**
 * Pointers to the global hash tables containing praetor's network-specific
 * configuration, indexed by the user-specified name of the network, and by
 * socket file descriptor, respectively.
 */
extern struct htable* rc_network, * rc_network_sock;
/**
 * Pointers to the global hash tables containing configuration for praetor's
 * loaded plugins, indexed by the user-specified name of the plugin, and by
 * socket file descriptor, respectively.
 */
extern struct htable* rc_plugin, * rc_plugin_sock;

/**
 * Contains the configuration options required for praetor to function as a
 * daemon.
 */
struct praetor{
    /**
     * The daemon will drop its user privileges from root to that of the
     * specified user.
     */
    const char* user;
    /**
     * The daemon will drop its group privileges from root to that of the
     * specified group.
     */
    const char* group;
    /**
     * The daemon's working directory. All non-absolute file paths are relative
     * to this directory.
     */
    const char* workdir;
};

/**
 * This struct represents the configuration of a loaded plugin
 */
struct plugin{
    /**
     * A handle for the plugin, to be used as an index in hash tables.
     */
    char* name;
    /**
     * The process ID of the child process spawned for this plugin.
     */
    pid_t pid;
    /**
     * Praetor's end of the socket pair for this plugin.
     */
    int sock;
    /**
     * The name of the plugin author, to be printed on calls to
     * plugin_get_author().
     */
    char* author;
    /**
     * The version of the plugin
     */
    char* version;
    /**
     * A description of the plugin, to be printed on calls to
     * plugin_get_description().
     */
    char* description;
    /**
     * The filesystem path at which the plugin binary is located.
     */
    char* path;
    /**
     * If set to true, this plugin will be allowed to send and receive private
     * messages.
     */
    bool private_messages;
    /**
     * The number of milliseconds that this plugin will be required to wait
     * before sending another message. If the plugin sends messages at a rate
     * that exceeds this limit, the messages will be queued and sent one-by-one
     * as the rate_limit timer cycles.
     */
    size_t rate_limit;
    /**
     * A linked list of channels that this plugin will be allowed to receive
     * input from.
     */
    struct channelinfo* input;
    /**
     * A linked list of channels that this plugin will be allowed to send
     * output to.
     */
    struct channelinfo* output;
};

/**
 * This struct describes an IRC channel.
 */
struct channel{
    const char* name;
    const char* password;
};

/**
 * A struct that contains configuration options for connections to an IRC
 * network.
 */
struct network{
    /**
     * A handle for the network, to be used as an index in hash tables.
     */
    const char* name;
    /**
     * The DNS name or IP address and TCP port of the IRC server to connect to.
     */
    const char* host;
    /**
     * If this is set to true, praetor will attempt to connect using SSL/TLS.
     */
    bool ssl;
    /**
     * praetor will attempt to use this nickname first when registering a
     * connection with the IRC server. See <a
     * href="https://tools.ietf.org/html/rfc2812#section-3.1.2">RFC2812</a> for
     * more information
     */
    const char* nick;
    /**
     * If the nickname specified in nick cannot be used, praetor attempts to
     * use this nickname. Any question marks in this nickname will be replaced
     * with random decimal digits.
     */
    const char* alt_nick;
    /**
     * The username that praetor will use when registering a connection with
     * the IRC server. See <a
     * href="https://tools.ietf.org/html/rfc2812#section-3.1.3">RFC2812</a> for
     * more information.
     */
    const char* user;
    /**
     * The realname that praetor will use when registering a connection with
     * the IRC server. See <a
     * href="https://tools.ietf.org/html/rfc2812#section-3.1.3">RFC2812</a> for
     * more information.
     */
    const char* real_name;
    /**
     * The message to be sent with a QUIT command.
     */
    const char* quit_msg;
    /**
     * A hash table containing the names of the channels on this network that
     * praetor will join upon registering a connection.
     */
    struct htable* channels;
    /**
     * A hash table containing the IRC nicknames of administrators of this bot.
     */
    struct htable* admins;
    /**
     * A hash table containing plugin configuration for this network. This hash
     * table maps plugin handles to plugin structs.
     */
    struct htable* plugins;
    /**
     * A socket file descriptor for the connection to this IRC server.
     */
    int sock;
    /**
     * A libtls connection context.
     */
    struct tls* ctx;
    /**
     * A buffer for the next message(s) from this network. At any given time,
     * this buffer may contain no full lines, or multiple lines.
     */
    char* msg;
    /**
     * An index one greater than the index of the last character in the message
     * buffer.
     */
    int msg_pos;
};

/**
 * Parses praetor's main configuration file, and returns the results into the
 * appropriate global objects. These are:
 *     - rc_praetor (struct praetor)
 *     - rc_network (struct htable)
 *     - rc_plugin (struct htable)
 * Memory for these structures must be allocated before calling this function.
 * All fields of the rc_praetor struct are given default values, and do not
 * need to be specified by the user unless the defaults need to be overridden.
 *
 * @param path The path to the main configuration file.
 * \return 0 on success.
 * \return -1 if the configuration could not be successfully loaded.
 */
int loadconfig(char* path);

#endif
