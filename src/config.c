/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015, David Zero
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
struct htable* rc_network;
struct htable* rc_network_sock;

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
    logmsg(LOG_DEBUG, "Loaded configuration file %s\n", path);
    if(root == NULL){
        logmsg(LOG_ERR, "Configuration: %s at line %d, column %d\n", error.text, error.line, error.column);
        _exit(-1);
    }

    initconfig(rc_praetor);
    json_t* daemon_section = NULL, *networks_section = NULL, *plugins_section = NULL;
    
    //Unpack and validate root object
    if(json_unpack_ex(root, &error, JSON_STRICT, "{s?O, s?O}", "daemon", &daemon_section, "networks", &networks_section)){
        logmsg(LOG_ERR, "Configuration: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
        _exit(-1);
    }

    //Unpack and validate daemon configuration section
    if(daemon_section == NULL){
        logmsg(LOG_WARNING, "Configuration: no daemon section, using default settings\n");
    }
    else{
        if(json_unpack_ex(daemon_section, &error, JSON_STRICT, "{s?s, s?s, s?s, s?s}", "user", &rc_praetor->user, "group", &rc_praetor->group, "workdir", &rc_praetor->workdir, "plugins", &rc_praetor->plugin_path) == -1){
            logmsg(LOG_ERR, "Configuration: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
            _exit(-1);
        }
    }

    //Unpack and validate networks configuration section
    if(networks_section == NULL){
        logmsg(LOG_WARNING, "Configuration: no networks section, praetor will not connect to any IRC networks\n");
    }
    else if(!json_is_array(networks_section)){
        logmsg(LOG_ERR, "Configuration: networks section must be an array\n");
        _exit(-1);
    }
    else{
        struct networkinfo* networkinfo_this = calloc(1, sizeof(struct networkinfo));
        char* name;
        size_t name_len;

        json_t* value, *channels, *admins, *plugins;
        size_t index;
        json_array_foreach(networks_section, index, value){
            if(json_unpack_ex(value, &error, JSON_STRICT, "{s:s%, s:s, s?b, s:s, s:s, s:s, s:s, s?O, s?O, s?O}", "name", &name, &name_len, "host", &networkinfo_this->host, "ssl", &networkinfo_this->ssl, "nick", &networkinfo_this->nick, "alt_nick", &networkinfo_this->alt_nick, "user", &networkinfo_this->user, "real_name", &networkinfo_this->real_name, "channels", &channels, "admins", &admins, "plugins", &plugins) == -1){
                logmsg(LOG_ERR, "Configuration: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
                _exit(-1);
            }
            htable_add(rc_network, name, name_len, networkinfo_this);
            logmsg(LOG_DEBUG, "Configuration: added configuration for network %s:%zd\n", name, name_len);
        }
    }

    //Unpack and validate plugins configuration section
    if(plugins_section == NULL){
        logmsg(LOG_WARNING, "Configuration: no plugins section, praetor will not load any plugins\n");
    }
    else if(!json_is_array(plugins_section)){
        logmsg(LOG_ERR, "Configuration: networks section must be an array\n");
        _exit(-1);
    }
    else{
        
    }
}
