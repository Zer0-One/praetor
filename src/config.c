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

#include <errno.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "hashtable.h"

struct praetorinfo* rc_praetor;
struct htable* rc_network, * rc_network_sock;
struct htable* rc_plugin, * rc_plugin_sock;

void initconfig(struct praetorinfo* rc_praetor){
    rc_praetor->user = "praetor";
    rc_praetor->group = "praetor";
    rc_praetor->workdir = "/var/lib/praetor";
    rc_praetor->plugin_path = "/usr/share/praetor/plugins";
}

void loadconfig(char* path){
    srandom(time(NULL));
    json_error_t error;
    json_t* root = json_load_file(path, 0, &error);
    logmsg(LOG_DEBUG, "config: Loaded configuration file %s\n", path);
    if(root == NULL){
        logmsg(LOG_ERR, "config: %s at line %d, column %d\n", error.text, error.line, error.column);
        _exit(-1);
    }

    initconfig(rc_praetor);
    json_t* daemon_section = NULL, *networks_section = NULL, *plugins_section;
    
    //Unpack and validate root object
    if(json_unpack_ex(root, &error, JSON_STRICT, "{s?O, s?O, s?O}", "daemon", &daemon_section, "networks", &networks_section, "plugins", &plugins_section)){
        logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
        _exit(-1);
    }

    //Unpack and validate daemon configuration section
    if(daemon_section == NULL){
        logmsg(LOG_WARNING, "config: No daemon section, using default settings\n");
    }
    else{
        if(json_unpack_ex(daemon_section, &error, JSON_STRICT, "{s?s, s?s, s?s, s?s}", "user", &rc_praetor->user, "group", &rc_praetor->group, "workdir", &rc_praetor->workdir, "plugins", &rc_praetor->plugin_path) == -1){
            logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
            _exit(-1);
        }
    }

    //Unpack and validate plugins configuration section
    if(plugins_section == NULL){
        logmsg(LOG_WARNING, "config: No plugins configured\n");
    }
    else if(!json_is_array(plugins_section)){
        logmsg(LOG_ERR, "config: plugins section must be an array\n");
        _exit(-1);
    }
    else{
        json_t* value;
        size_t index;
        json_array_foreach(plugins_section, index, value){
            struct plugin* plugin_this = calloc(1, sizeof(struct plugin));
            if(plugin_this == NULL){
                logmsg(LOG_ERR, "config: Cannot allocate memory for plugin configuration\n");
                _exit(-1);
            }
            if(json_unpack_ex(value, &error, JSON_STRICT, "{s:s, s:s}", "name", &plugin_this->name, "path", &plugin_this->path) == -1){
                logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
                _exit(-1);
            }
            htable_add(rc_plugin, plugin_this->name, strlen(plugin_this->name)+1, plugin_this);
            logmsg(LOG_DEBUG, "config: Added configuration for plugin %s\n", plugin_this->name);
        }
    }

    //Unpack and validate networks configuration section
    if(networks_section == NULL){
        logmsg(LOG_WARNING, "config: No networks section, praetor will not connect to any IRC networks\n");
    }
    else if(!json_is_array(networks_section)){
        logmsg(LOG_ERR, "config: networks section must be an array\n");
        _exit(-1);
    }
    else{
        json_t* value, *channel_value, *channels, *admins, *plugins;
        size_t index, channel_index;
        json_array_foreach(networks_section, index, value){
            //create a fresh networkinfo struct for each network
            struct networkinfo* networkinfo_this = calloc(1, sizeof(struct networkinfo));
            if(networkinfo_this == NULL){
                logmsg(LOG_ERR, "config: Cannot allocate memory for network configuration\n");
                _exit(-1);
            }
            //instantiate hash tables for this network
            networkinfo_this->channels = htable_create(10);
            networkinfo_this->plugins = htable_create(10);
            if(json_unpack_ex(value, &error, JSON_STRICT, "{s:s, s:s, s?b, s:s, s:s, s:s, s:s, s:s, s?O, s?O, s?O}", "name", &networkinfo_this->name, "host", &networkinfo_this->host, "ssl", &networkinfo_this->ssl, "nick", &networkinfo_this->nick, "alt_nick", &networkinfo_this->alt_nick, "user", &networkinfo_this->user, "real_name", &networkinfo_this->real_name, "quit_msg", &networkinfo_this->quit_msg, "channels", &channels, "admins", &admins, "plugins", &plugins) == -1){
                logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
                _exit(-1);
            }
            //add this networkinfo to the global hash table, indexed by its name
            htable_add(rc_network, networkinfo_this->name, strlen(networkinfo_this->name)+1, networkinfo_this);
            logmsg(LOG_DEBUG, "config: Added configuration for network %s\n", networkinfo_this->name);
        

            //Unpack and validate plugins configuration section for this network
            if(plugins == NULL){
                logmsg(LOG_WARNING, "config: No plugins section, praetor will not load any plugins\n");
            }
            else if(!json_is_array(plugins)){
                logmsg(LOG_ERR, "config: networks section must be an array\n");
                _exit(-1);
            }
            else{
                //This is where I'd be parsing the plugins section for the network
            }

            //Unpack and validate admins configuration section for this network
            if(admins == NULL){
                logmsg(LOG_WARNING, "config: No admins section, praetor will not recognize any admins\n");
            }
            else if(!json_is_array(admins)){
                logmsg(LOG_ERR, "config: admins section must be an array\n");
                _exit(-1);
            }
            else{
                //This is where I'd be parsing the admins section for the network
            }
            
            //Unpack and validate channels configuration section for this network
            if(channels == NULL){
                logmsg(LOG_WARNING, "config: No channels section, praetor will not join any channels\n");
            }
            else if(!json_is_array(channels)){
                logmsg(LOG_ERR, "config: channels section must be an array\n");
                _exit(-1);
            }
            else{
                json_array_foreach(channels, channel_index, channel_value){
                    struct channel* channel_this = calloc(1, sizeof(struct channel));
                    if(channel_this == NULL){
                        logmsg(LOG_ERR, "config: Cannot allocate memory for network configuration\n");
                        _exit(-1);
                    }
                    if(json_unpack_ex(channel_value, &error, JSON_STRICT, "{s:s, s?s}", "name", &channel_this->name, "password", &channel_this->password) == -1){
                        logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
                        _exit(-1);
                    }
                    htable_add(networkinfo_this->channels, channel_this->name, strlen(channel_this->name)+1, channel_this);
                    logmsg(LOG_DEBUG, "config: Added channel %s to network %s\n", channel_this->name, networkinfo_this->name);
                }
            }
        }
    }
}
