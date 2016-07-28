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
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <tls.h>
#include <unistd.h>

#include "config.h"
#include "hashtable.h"
#include "inet.h"
#include "irc.h"
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
   
    char* tmp = NULL;
    const char* host = n->host, *port = IRC_DEFAULT_PORT;
    if(strstr(n->host, ":") != NULL){
        //strtok modifies its argument, so make a copy
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
        free(tmp);
        return -1;
    }
    

    //if the "ssl" option is enabled for this network, establish a TLS connection
    if(n->ssl){
        if((n->ctx = inet_tls_connect(n->sock, host)) == NULL){
            free(tmp);
            return -1;
        }
    }

    free(tmp);
  
    //beyond this point, we will need a message buffer for this network
    if((n->msg = calloc(512, sizeof(char))) == NULL){
        irc_disconnect(network, n->quit_msg);
        logmsg(LOG_WARNING, "irc: Out of memory, could not connect to IRC network '%s'\n", network);
        return -1;
    }
    n->msg_pos = 0;

    //map the socket to the irc network config
    htable_add(rc_network_sock, &n->sock, sizeof(n->sock), n);
    return n->sock;
}

int irc_register_connection(const char* network){
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char buf[MSG_SIZE];

    int count;
    count = snprintf(buf, MSG_SIZE, "NICK %s\r\n", n->nick);
    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not register connection with network %s\n", n->name);
        return -1;
    }
    //handle_numeric()
    count = snprintf(buf, MSG_SIZE, "USER %s 8 * :%s\r\n", n->user, n->real_name);
    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not register connection with network %s\n", n->name);
        return -1;
    }
    //handle_numeric()

    return 0;
}

int irc_disconnect(const char* network, const char* msg){
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char buf[MSG_SIZE];
    int count = snprintf(buf, MSG_SIZE, "QUIT :%s\r\n", msg);
    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not quit from network %s\n", n->name);
    }

    if(close(n->sock) == -1){
        logmsg(LOG_WARNING, "irc: Could not disconnect from network %s\n", n->name);
    }
    watch_remove(n->sock);
    
    return 0;
}

int irc_channel_join(const char* network, const char* channel){
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char buf[MSG_SIZE];
    int count;
    
    struct channel* c;
    if((c = (struct channel*)htable_lookup(n->channels, channel, strlen(channel)+1)) != NULL){
        if(c->password != 0){
            count = snprintf(buf, MSG_SIZE, "JOIN %s %s\r\n", c->name, c->password);
        }
        else{
            count = snprintf(buf, MSG_SIZE, "JOIN %s\r\n", channel);
        }
    }
    else{
        count = snprintf(buf, MSG_SIZE, "JOIN %s\r\n", channel);
    }

    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not join channel %s on network %s\n", channel, n->name);
        return -1;
    }
    
    return 0;
}

int irc_connect_all(const char* network){
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }
    
    if(irc_connect(network) == -1){
        return -1;
    }
    if(watch_add(n->sock) == -1){
        if(close(n->sock) == -1){
            logmsg(LOG_WARNING, "irc: Could not close socket connection to network %s\n", n->name);
        }
        return -1;
    }
    if(irc_register_connection(network) == -1){
        irc_disconnect(network, n->quit_msg);
        return -1;
    }

    struct list* channels = htable_get_keys(n->channels, false);
    if(channels == NULL){
        logmsg(LOG_WARNING, "irc: No channels configured for network %s\n", n->name);
        return 0;
    }
    for(struct list* this = channels; this != 0; this = this->next){
        irc_channel_join(network, this->key);
    }
    htable_key_list_free(channels, false);

    return 0;
}

int irc_channel_part(const char* network, const char* channel){
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char buf[MSG_SIZE];
    int count = snprintf(buf, MSG_SIZE, "PART %s\r\n", channel);
    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not part channel %s on network %s\n", channel, n->name);
        return -1;
    }
    
    return 0;
}

int irc_msg_send(const char* network, const char* buf, size_t len){
    if(len > MSG_SIZE){
        logmsg(LOG_WARNING, "irc: Message too large, could not send");
        return -1;
    }
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }
   
    ssize_t count;
    if(n->ssl){
        if((count = tls_write(n->ctx, buf, len)) == -1){
            logmsg(LOG_WARNING, "irc: Could not send message on %s, %s\n", n->name, tls_error(n->ctx));
            return -1;
        }
        return count;
    }

    if((count = send(n->sock, buf, len, 0)) == -1){
        logmsg(LOG_WARNING, "irc: Could not send message on %s, %s\n", n->name, strerror(errno));
        return -1;
    }

    return count;
}

ssize_t irc_msg_recv(const char* network, char* buf, size_t len){
    if(len < MSG_SIZE){
        logmsg(LOG_DEBUG, "irc: Message buffer too small to receive IRC message\n");
        return -1;
    }
    
    struct networkinfo* n;
    if((n = (struct networkinfo*)htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char* line_end;
    if((line_end = memchr(n->msg, '\n', n->msg_pos)) == NULL){
        return -1;
    }
    
    size_t char_count = line_end - n->msg + 1;
    strncpy(buf, n->msg, char_count);
    memmove(n->msg, line_end + 1, MSG_SIZE - char_count);
    n->msg_pos = (n->msg + n->msg_pos) - line_end - 1;

    return char_count;
}

int irc_handle_ping(const char* network, const char* buf, size_t len){
    if(strncmp(buf, "PING", 4) == 0){
        char msg[len];
        memcpy(msg, buf, len);
        msg[1] = 'O';
        if(irc_msg_send(network, msg, len) == -1){
            return -1;
        }
        logmsg(LOG_DEBUG, "RESPONSE: %.*s", (int)len, msg);
    }

    return 0;
}
