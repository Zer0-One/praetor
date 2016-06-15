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

#include <stdlib.h>
#include <tls.h>

#include "config.h"
#include "hashtable.h"
#include "inet.h"
#include "log.h"
#include "nexus.h"

#define MAX_CONNECT_RETRY 5
#define IRC_DEFAULT_PORT "6667"

int irc_connect(const char* network){
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }
    
    const char* host = n->host, *port = IRC_DEFAULT_PORT;
    if(strstr(n->host, ":") != NULL){
        //strtok modifies its argument, so make a copy
        char* tmp; 
        if((tmp = calloc(1, strlen(n->host)+1)) == NULL){
            logmsg(LOG_WARNING, "irc: Out of memory, could not connect to IRC network '%s'\n", network);
            return -1;
        }
        strcpy(tmp, n->host);
        host = strtok(tmp, ":");
        port = strtok(NULL, ":");
    }
    
    for(int i = 0; i < MAX_CONNECT_RETRY; i++){
        if((n->sock = inet_connect(host, port)) != -1){
            break;
        }
        logmsg(LOG_WARNING, "irc: Could not connect to IRC network '%s', connection attempt #%d\n", network, i);
    }
    if(n->sock == -1){
        logmsg(LOG_WARNING, "irc: Failed to connect to IRC network '%s'\n", network);
        return -1;
    }
    

    //if the "ssl" option is enabled for this network, establish a TLS connection
    if(n->ssl){
        if((n->ctx = inet_tls_connect(n->sock, host)) == NULL){
            return -1;
        }
    }
    
    //map the socket to the irc network config
    htable_add(rc_network_sock, &n->sock, sizeof(n->sock), n);
    return n->sock;
}

int irc_disconnect(const char* network){
    return 0;
}

int irc_channel_join(const char* network, const char* channel){
    return 0;
}

int irc_channel_part(const char* network, const char* channel){
    return 0;
}
