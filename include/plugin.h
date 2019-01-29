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

#ifndef PRAETOR_PLUGIN
#define PRAETOR_PLUGIN

#include <jansson.h>

#include "config.h"
#include "ircmsg.h"

/**
 * Loads an executable plugin from its configured path. The executable will be
 * forked, and its standard streams will be directed over an anonymous UNIX
 * domain socket toward praetor.
 *
 * \return 0 on success.
 * \return -1 if the plugin could not be loaded.
 */
int plugin_load(struct plugin* p);

/**
 * Calls plugin_load() for every plugin defined in praetor's configuration.
 * 
 * \return 0 on success.
 * \return -1 if plugin_load() fails for any plugin.
 */
int plugin_load_all();

/**
 * Unloads a currently-loaded executable plugin by removing its mappings in the
 * rc_plugin and rc_plugin_sock hash tables, closing its socket connection, and
 * then terminating its process.
 *
 * \return 0 on success.
 * \return -1 if the plugin could not be terminated successfully.
 */
int plugin_unload(struct plugin* p);

/**
 * Calls plugin_unload() for every plugin currently loaded and indexed by the
 * rc_plugin hash table.
 *
 * \return 0 on success.
 * \return -1 if any plugin could not be terminated successfully.
 */
int plugin_unload_all();

/**
 * Calls plugin_unload() and plugin_load() for the specified plugin.
 *
 * \return 0 on success.
 * \return -1 if the plugin could not be terminated/started successfully.
 */
int plugin_reload(struct plugin* p);

/**
 * Calls plugin_reload() for every plugin currently loaded and indexed by the
 * rc_plugin hash table.
 *
 * \return 0 on success.
 * \return -1 if any plugin could not be terminated/started successfully.
 */
int plugin_reload_all();

/**
 * Converts the given IRC message into a JSON message, and sends it to the
 * given plugin.
 *
 * If the message could not be sent, the plugin will be reloaded via
 * plugin_reload().
 *
 * \return 0 on success.
 * \return -1 on failure to send the JSON message.
 * \return -2 on failure to convert the given IRC message into a JSON message.
 */
int plugin_send(struct plugin* p, struct ircmsg* msg);

/**
 * Reads a JSON message from the specified plugin.
 *
 * If the message could not be loaded or parsed, the plugin will be reloaded
 * via plugin_reload().
 *
 * \return A pointer to the received JSON object on success.
 * \return NULL if input could not be read successfully.
 */
json_t* plugin_recv(struct plugin* p);

/**
 * Returns a pointer to the name of the plugin author(s).
 */
const char* plugin_get_author(const struct plugin* p);

/**
 * Returns a pointer to a description of the plugin.
 */
const char* plugin_get_description(const struct plugin* p);

#endif
