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

#ifndef PRAETOR_PLUGIN
#define PRAETOR_PLUGIN

#include <jansson.h>

/**
 * Loads an executable plugin from the given path. The executable will be
 * forked, and its standard streams will be directed over an anonymous domain
 * socket toward praetor.
 *
 * \param path The filesystem path to the executable to be loaded. Relative
 *             paths will be based in praetor's configured working directory; absolute
 *             paths are used verbatim.
 *
 * \return 0 on success.
 * \return -1 if the plugin could not be loaded.
 */
int plugin_load(const char* path);

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
 * \param name The handle indexing the plugin in the rc_plugin hash table.
 *
 * \return 0 on success.
 * \return -1 if the plugin could not be terminated successfully.
 */
int plugin_unload(const char* name);

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
 * \param name The handle indexing the plugin in the rc_plugin hash table.
 *
 * \return 0 on success.
 * \return -1 if the plugin could not be terminated/started successfully.
 */
int plugin_reload(const char* name);

/**
 * Calls plugin_reload() for every plugin currently loaded and indexed by the
 * rc_plugin hash table.
 *
 * \return 0 on success.
 * \return -1 if any plugin could not be terminated/started successfully.
 */
int plugin_reload_all();

/**
 * Reads a JSON message from the specified plugin.
 *
 * \param name The handle indexing the plugin in the rc_plugin hash table.
 *
 * \return A pointer to the received JSON object on success.
 * \return NULL if input could not be read successfully.
 */
json_t* plugin_msg_recv(const char* name);

/**
 * Returns a pointer to the name of the plugin author(s).
 */
const char* plugin_get_author(const char* name);

/**
 * Returns a pointer to a description of the plugin.
 */
const char* plugin_get_description(const char* name);

#endif
