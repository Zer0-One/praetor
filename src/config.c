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

void initconfig(struct praetorinfo* rc_praetor){
    rc_praetor->user = "praetor";
    rc_praetor->group = "praetor";
    rc_praetor->workdir = "/var/lib/praetor";
    rc_praetor->plugin_path = "/usr/share/praetor/plugins";
}

void loadconfig(char* path, struct praetorinfo* rc_praetor, struct networkinfo* rc_networks){
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
        json_t* value, *channels, *admins, *plugins;
        size_t index;
        struct networkinfo* rc_networks_this = rc_networks;
        json_array_foreach(networks_section, index, value){
            if(json_unpack_ex(value, &error, JSON_STRICT, "{s:s, s:s, s?b, s:s, s:s, s:s, s:s, s?O, s?O}", "name", &rc_networks_this->name, "host", &rc_networks_this->host, "ssl", &rc_networks_this->ssl, "nick", &rc_networks_this->nick, "alt_nick", &rc_networks_this->alt_nick, "user", &rc_networks_this->user, "real_name", &rc_networks_this->real_name, "channels", &channels, "admins", &admins) == -1){
                logmsg(LOG_ERR, "Configuration: %s at line %d, column %d. Source: %s\n", error.text, error.line, error.column, error.source);
                _exit(-1);
            }
            if(index == (json_array_size(networks_section) - 1)){
                rc_networks_this->next = NULL;
                break;
            }
            rc_networks_this->next = malloc(sizeof(struct networkinfo));
            rc_networks_this = rc_networks_this->next;
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
