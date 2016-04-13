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
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <tls.h>

#include "config.h"
#include "hashtable.h"
#include "log.h"

int inet_connect(const char* host, const char* service){
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(host, service, &hints, &result);
    if(s != 0){
        printf("irc: Could not get address info for host %s, %s", host, gai_strerror(s));
        return -1;
    }

    int sock = 0;
    for(; result != NULL; result = result->ai_next){
        sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if(sock == -1){
            logmsg(LOG_DEBUG, "irc: Could not open socket for host %s, %s", host, strerror(errno));
            continue;
        }
        if(connect(sock, result->ai_addr, result->ai_addrlen) != -1){
            break;
        }
        logmsg(LOG_DEBUG, "irc: Could not connect to host %s, %s", host, strerror(errno));
        close(sock);
    }

    //put the socket fd into non-blocking mode
    int flags;
    if((flags = fcntl(sock, F_GETFL, 0)) == -1){
        logmsg(LOG_WARNING, "irc: Could not get socket file descriptor flags for host %s, %s", host, strerror(errno));
        return -1;
    }

    if(fcntl(sock, F_SETFL, flags | O_NONBLOCK)){
        logmsg(LOG_WARNING, "irc: Could not set socket file descriptor flags for host %s, %s", host, strerror(errno));
        return -1;
    }

    freeaddrinfo(result);
    return (result == NULL) ? -1 : sock;
}

int irc_connect(const char* network){
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network))) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network %s", network);
        return -1;
    }
    
    const char* host, *port;
    if(strstr(n->host, ":") != NULL){
        //strtok modifies its argument, so make a copy
        char* tmp = calloc(1, strlen(n->host)+1);
        strcpy(tmp, n->host);
        host = strtok(tmp, ":");
        port = strtok(NULL, ":");
        if((n->sock = inet_connect(host, port)) == -1){
            logmsg(LOG_WARNING, "irc: Could not connect to IRC network %s", network);
            return -1;
        }
    }
    else{
        host = n->host;
        if((n->sock = inet_connect(host, "6667")) == -1){
            logmsg(LOG_WARNING, "irc: Could not connect to IRC network %s", network);
            return -1;
        }
    }

    //if the "ssl" option is enabled for this network, establish a TLS connection
    if(n->ssl){
        tls_init();
        struct tls_config* cfg;
        if((n->ctx = tls_client()) == NULL){
            logmsg(LOG_WARNING, "irc: Could not establish TLS connection to network %s, system out of memory", network);
            return -1;
        }
        if((cfg = tls_config_new()) == NULL){
            logmsg(LOG_WARNING, "irc: Could not establish TLS connection to network %s, system out of memory", network);
            tls_free(n->ctx);
            return -1;
        }
        if(!tls_configure(n->ctx, cfg)){
            logmsg(LOG_WARNING, "irc: Could not establish TLS connection to network %s, %s", network, tls_error(n->ctx));
            tls_config_free(cfg);
            tls_free(n->ctx);
            return -1;
        }

        tls_config_free(cfg);
        
        if(!tls_connect_socket(n->ctx, n->sock, host)){
            logmsg(LOG_WARNING, "irc: Could not establish TLS connection to network %s, %s", network, tls_error(n->ctx));
            tls_free(n->ctx);
            return -1;
        }
    }

    //map the socket to the network config
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
