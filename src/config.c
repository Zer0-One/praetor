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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <jansson.h>

#include "config.h"
#include "log.h"
#include "htable.h"

#define SCHEMA_CHANNELS "{s:s, s?s}"
#define SCHEMA_DAEMON "{s?s, s?s, s?s}"
#define SCHEMA_NETWORKS "{s?o, s:s, s?o, s:s, s:s, s:s, s?s, s?o, s?s, s:s, s?b, s:s}"
#define SCHEMA_PLUGINS "{s:s, s:s}"
#define SCHEMA_ROOT "{s?o, s?o, s?o}"

struct praetor* rc_praetor;
struct htable* rc_network, * rc_network_sock;
struct htable* rc_plugin, * rc_plugin_sock;

json_t* root = NULL;

void config_init(struct praetor* rc_praetor){
    rc_praetor->user = "praetor";
    rc_praetor->group = "praetor";
    rc_praetor->workdir = "/var/lib/praetor";
}

int config_load(char* path){
    srandom(time(NULL));
    json_error_t error;
    //if we're reloading the config, free the old one
    if(root != NULL){
        json_decref(root);
    }
    root = json_load_file(path, 0, &error);
    logmsg(LOG_DEBUG, "config: Loaded configuration file %s\n", path);
    if(root == NULL){
        logmsg(LOG_ERR, "config: %s at line %d, column %d\n", error.text, error.line, error.column);
        return -1;
    }

    config_init(rc_praetor);
    json_t* praetor_section = NULL, *networks_section = NULL, *plugins_section;
    
    //Unpack and validate root object
    int ret = json_unpack_ex(
        root,
        &error,
        JSON_STRICT,
        SCHEMA_ROOT,
        "praetor", &praetor_section,
        "networks", &networks_section,
        "plugins", &plugins_section
    );
    if(ret == -1){
        logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
        return -1;
    }

    //Unpack and validate praetor configuration section
    if(praetor_section == NULL){
        logmsg(LOG_WARNING, "config: No praetor section, using default settings\n");
    }
    else{
        int ret = json_unpack_ex(
            praetor_section,
            &error,
            JSON_STRICT,
            SCHEMA_DAEMON,
            "user", &rc_praetor->user,
            "group", &rc_praetor->group,
            "workdir", &rc_praetor->workdir
        );
        if(ret == -1){
            logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
            return -1;
        }
    }

    //Unpack and validate plugins configuration section
    if(plugins_section == NULL){
        logmsg(LOG_WARNING, "config: No plugins configured\n");
    }
    else if(!json_is_array(plugins_section)){
        logmsg(LOG_ERR, "config: plugins section must be an array\n");
        return -1;
    }
    else{
        json_t* value;
        size_t index;
        json_array_foreach(plugins_section, index, value){
            struct plugin* plugin_this = calloc(1, sizeof(struct plugin));
            if(plugin_this == NULL){
                logmsg(LOG_ERR, "config: Cannot allocate memory for plugin configuration\n");
                return -1;
            }
            if(json_unpack_ex(value, &error, JSON_STRICT, SCHEMA_PLUGINS, "name", &plugin_this->name, "path", &plugin_this->path) == -1){
                logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
                return -1;
            }
            
            int ret = htable_add(rc_plugin, (uint8_t*)plugin_this->name, strlen(plugin_this->name)+1, plugin_this);
            if(ret == -1){
                logmsg(LOG_ERR, "config: Could not add configuration for plugin %s, plugin already exists\n", plugin_this->name);
                _exit(-1);
            }
            else if(ret == -2){
                logmsg(LOG_ERR, "config: Could not add configuration for plugin %s, the system is out of memory\n", plugin_this->name);
                _exit(-1);
            }

            plugin_this->sock = -1;

            logmsg(LOG_DEBUG, "config: Added configuration for plugin %s\n", plugin_this->name);
        }
    }

    //Unpack and validate networks configuration section
    if(networks_section == NULL){
        logmsg(LOG_WARNING, "config: No networks section, praetor will not connect to any IRC networks\n");
    }
    else if(!json_is_array(networks_section)){
        logmsg(LOG_ERR, "config: networks section must be an array\n");
        return -1;
    }
    else{
        json_t* value, *channel_value, *channels, *admins, *plugins;
        size_t index, channel_index;
        json_array_foreach(networks_section, index, value){
            //create a fresh networkinfo struct for each network
            struct network* network_this = calloc(1, sizeof(struct network));
            if(network_this == NULL){
                logmsg(LOG_ERR, "config: Cannot allocate memory for network configuration\n");
                return -1;
            }
            //instantiate hash tables for this network
            network_this->channels = htable_create(10);
            network_this->plugins = htable_create(10);
            if(network_this->channels == NULL || network_this->plugins == NULL){
                logmsg(LOG_ERR, "config: Could not create network configuration, the system is out of memory\n");
                _exit(-1);
            }

            int ret = json_unpack_ex(
                value,
                &error,
                JSON_STRICT,
                SCHEMA_NETWORKS,
                "admins", &admins,
                "alt_nick", &network_this->alt_nick,
                "channels", &channels,
                "host", &network_this->host,
                "name", &network_this->name,
                "nick", &network_this->nick,
                "pass", &network_this->pass,
                "plugins", &plugins,
                "quit_msg", &network_this->quit_msg,
                "real_name", &network_this->real_name,
                "ssl", &network_this->ssl,
                "user", &network_this->user
            );
            if(ret == -1){
                logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
                return -1;
            }
            //add this networkinfo to the global hash table, indexed by its name
            ret = htable_add(rc_network, (uint8_t*)network_this->name, strlen(network_this->name)+1, network_this);
            if(ret == -1){
                logmsg(LOG_ERR, "config: Could not add configuration for network %s, network already exists\n", network_this->name);
                _exit(-1);
            }
            else if(ret == -2){
                logmsg(LOG_ERR, "config: Could not add configuration for network %s, the system is out of memory\n", network_this->name);
                _exit(-1);
            }
            logmsg(LOG_DEBUG, "config: Added configuration for network %s\n", network_this->name);
        

            //Unpack and validate plugins configuration section for this network
            if(plugins == NULL){
                logmsg(LOG_WARNING, "config: No plugins section, praetor will not load any plugins\n");
            }
            else if(!json_is_array(plugins)){
                logmsg(LOG_ERR, "config: networks section must be an array\n");
                return -1;
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
                return -1;
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
                return -1;
            }
            else{
                json_array_foreach(channels, channel_index, channel_value){
                    struct channel* channel_this = calloc(1, sizeof(struct channel));
                    if(channel_this == NULL){
                        logmsg(LOG_ERR, "config: Cannot allocate memory for network configuration\n");
                        return -1;
                    }
                    if(json_unpack_ex(channel_value, &error, JSON_STRICT, SCHEMA_CHANNELS, "name", &channel_this->name, "key", &channel_this->key) == -1){
                        logmsg(LOG_ERR, "config: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
                        return -1;
                    }
                    ret = htable_add(network_this->channels, (uint8_t*)channel_this->name, strlen(channel_this->name)+1, channel_this);
                    if(ret == -1){
                        logmsg(LOG_ERR, "config: Could not add configuration for channel %s, channel already exists\n", channel_this->name);
                        _exit(-1);
                    }
                    else if(ret == -2){
                        logmsg(LOG_ERR, "config: Could not add configuration for channel %s, the system is out of memory\n", channel_this->name);
                        _exit(-1);
                    }
                    logmsg(LOG_DEBUG, "config: Added channel %s to network %s\n", channel_this->name, network_this->name);
                }
            }
        }
    }

    return 0;
}
