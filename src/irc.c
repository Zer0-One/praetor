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
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }
   
    char* tmp = NULL;
    const char* host = n->host, *port = IRC_DEFAULT_PORT;
    if(strstr(n->host, ":") != NULL){
        //strtok modifies its argument, so make a copy
        if((tmp = calloc(1, strlen(n->host)+1)) == NULL){
            logmsg(LOG_WARNING, "irc: Could not connect to IRC network '%s', out of memory\n", network);
            return -3;
        }
        strcpy(tmp, n->host);
        host = strtok(tmp, ":");
        port = strtok(NULL, ":");
    }
    
    for(size_t i = 0; i < MAX_CONNECT_RETRY; i++){
        if((n->sock = inet_connect(host, port)) != -1){
            break;
        }
        logmsg(LOG_WARNING, "irc: Could not connect to IRC network '%s', connection attempt #%zd\n", network, i);
    }
    if(n->sock == -1){
        logmsg(LOG_WARNING, "irc: Failed to connect to IRC network '%s'\n", network);
        free(tmp);
        return -1;
    }
    

    //if the "ssl" option is enabled for this network, establish a TLS connection
    if(n->ssl){
        if((n->ctx = inet_tls_upgrade(n->sock, host)) == NULL){
            free(tmp);
            return -1;
        }
    }

    free(tmp);
  
    //beyond this point, we will need a message buffer for this network
    if((n->msgbuf = calloc(MSG_SIZE_MAX, sizeof(char))) == NULL){
        irc_disconnect(network);
        logmsg(LOG_WARNING, "irc: Could not connect to IRC network '%s', the system is out of memory\n", network);
        return -1;
    }
    n->msgbuf_idx = 0;
    n->msgbuf_size = MSG_SIZE_MAX;

    //map the socket to the irc network config
    int ret = htable_add(rc_network_sock, &n->sock, sizeof(n->sock), n);
    if(ret == -1){
        logmsg(LOG_WARNING, "irc: Could not connect to IRC network '%s', the system is out of memory\n", network);
        return -3;
    }
    if(ret == -2){
        logmsg(LOG_WARNING, "irc: Could not connect to IRC network '%s', a mapping already exists for this network\n", network);
        return -2;
    }

    //add the socket to the global watchlist
    if(watch_add(n->sock) == -1){
        if(close(n->sock) == -1){
            logmsg(LOG_WARNING, "irc: Could not close socket connection to network %s\n", n->name);
        }
        return -2;
    }

    //register an IRC connection
    if(irc_register_connection(network) == -1){
        irc_disconnect(network);
        return -3;
    }

    //connect to all channels
    struct list* channels = htable_get_keys(n->channels, false);
    if(channels == NULL){
        logmsg(LOG_WARNING, "irc: Failed to load list of configured channels for network: %s\n", n->name);
        logmsg(LOG_WARNING, "irc: Network: %s has no configured channels, or the system is out of memory\n", n->name);
        return n->sock;
    }
    for(struct list* this = channels; this != 0; this = this->next){
        irc_channel_join(network, this->key);
    }
    htable_key_list_free(channels, false);

    return n->sock;
}

int irc_connect_all(){
    struct list* networks = htable_get_keys(rc_network, false);
    if(networks == NULL){
        logmsg(LOG_WARNING, "irc: Failed to load list of configured networks\n");
        logmsg(LOG_WARNING, "irc: There are no configured networks, or the system is out of memory\n");
        return -1;
    }
    int ret = 0;
    for(struct list* this = networks; this != 0; this = this->next){
        if(irc_connect(this->key) < 0){
            ret = -1;
        }
    }
    htable_key_list_free(networks, false);

    return ret;
}

int irc_register_connection(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char buf[MSG_SIZE_MAX];

    int count;
    count = snprintf(buf, MSG_SIZE_MAX, "NICK %s\r\n", n->nick);
    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", n->name);
        return -1;
    }
    //handle_numeric()
    count = snprintf(buf, MSG_SIZE_MAX, "USER %s 8 * :%s\r\n", n->user, n->real_name);
    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", n->name);
        return -1;
    }
    //handle_numeric()

    return 0;
}

int irc_disconnect(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char buf[MSG_SIZE_MAX];
    if(n->quit_msg != NULL){
        int count = snprintf(buf, MSG_SIZE_MAX, "QUIT :%s\r\n", n->quit_msg);
        if(irc_msg_send(network, buf, count) == -1){
            logmsg(LOG_WARNING, "irc: Could not quit from network '%s'\n", n->name);
        }
    }
    else{
        if(irc_msg_send(network, "QUIT", 4) == -1){
            logmsg(LOG_WARNING, "irc: Could not quit from network '%s'\n", n->name);
        }
    }

    if(close(n->sock) == -1){
        logmsg(LOG_WARNING, "irc: Could not disconnect from network '%s'\n", n->name);
    }
    watch_remove(n->sock);
    htable_remove(rc_network_sock, &n->sock, sizeof(n->sock));
    
    return 0;
}

int irc_disconnect_all(){
    struct list* networks = htable_get_keys(rc_network_sock, false);

    if(networks == NULL){
        logmsg(LOG_WARNING, "irc: Failed to load list of configured networks\n");
        logmsg(LOG_WARNING, "irc: There are no configured networks, or the system is out of memory\n");
        return -1;
    }

    int ret = 0;
    for(struct list* this = networks; this != 0; this = this->next){
        struct network* n = htable_lookup(rc_network_sock, this->key, this->size);
        if(irc_disconnect(n->name) < 0){
            ret = -1;
        }
    }

    htable_key_list_free(networks, false);

    return ret;
}

int irc_channel_join(const char* network, const char* channel){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char buf[MSG_SIZE_MAX];
    int count;
    
    struct channel* c;
    if((c = (struct channel*)htable_lookup(n->channels, channel, strlen(channel)+1)) != NULL){
        if(c->password != 0){
            count = snprintf(buf, MSG_SIZE_MAX, "JOIN %s %s\r\n", c->name, c->password);
        }
        else{
            count = snprintf(buf, MSG_SIZE_MAX, "JOIN %s\r\n", channel);
        }
    }
    else{
        count = snprintf(buf, MSG_SIZE_MAX, "JOIN %s\r\n", channel);
    }

    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not join channel '%s' on network '%s'\n", channel, n->name);
        return -1;
    }
    
    return 0;
}

int irc_channel_part(const char* network, const char* channel){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char buf[MSG_SIZE_MAX];
    int count = snprintf(buf, MSG_SIZE_MAX, "PART %s\r\n", channel);
    if(irc_msg_send(network, buf, count) == -1){
        logmsg(LOG_WARNING, "irc: Could not part channel '%s' on network '%s'\n", channel, n->name);
        return -1;
    }
    
    return 0;
}

//To-Do: Add debug logging to display sent messages
int irc_msg_send(const char* network, const char* buf, size_t len){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }
   
    ssize_t count;
    if(n->ssl){
        if(len < MSG_SIZE_MAX){
            if((count = tls_write(n->ctx, buf, len)) == -1){
                logmsg(LOG_WARNING, "irc: Could not send message on network '%s', %s\n", n->name, tls_error(n->ctx));
                return -1;
            }
        }
        else{
            if((count = tls_write(n->ctx, buf, MSG_SIZE_MAX)) == -1){
                logmsg(LOG_WARNING, "irc: Could not send message on network '%s', %s\n", n->name, tls_error(n->ctx));
                return -1;
            }
            logmsg(LOG_WARNING, "irc: Message to network '%s' truncated; size exceeded 512 characters.\n", n->name);
        }
        return count;
    }

    if(len < MSG_SIZE_MAX){
        if((count = send(n->sock, buf, len, 0)) == -1){
            logmsg(LOG_WARNING, "irc: Could not send message on network '%s', %s\n", n->name, strerror(errno));
            return -1;
        }
    }
    else{
        if((count = send(n->sock, buf, MSG_SIZE_MAX, 0)) == -1){
            logmsg(LOG_WARNING, "irc: Could not send message on network '%s', %s\n", n->name, strerror(errno));
            return -1;
        }
        logmsg(LOG_WARNING, "irc: Message to network '%s' truncated; size exceeded 512 characters.\n", n->name);
    }

    return count;
}

ssize_t irc_msg_recv(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return 0;
    }
    
    if(n->msgbuf_size < MSG_SIZE_MAX){
        logmsg(LOG_ERR, "irc: Message buffer for network '%s' too small to receive IRC messages\n", network);
        _exit(-1);
    }

    size_t bytes_to_read = n->msgbuf_size - n->msgbuf_idx;

    ssize_t ret;
    if(n->ssl){
        ret = tls_read(n->ctx, n->msgbuf + n->msgbuf_idx, bytes_to_read);
        if(ret == -1){
            logmsg(LOG_WARNING, "irc: Could not read from network '%s', %s\n", network, tls_error(n->ctx));
            //I do this check for a shutdown socket because idk how the fuck to get this from libtls
            char c;
            if(recv(n->sock, &c, 1, MSG_PEEK) == 0){
                goto server_reconnect;
            }
            return -1;
        }
    }
    else{
        ret = recv(n->sock, n->msgbuf + n->msgbuf_idx, bytes_to_read, 0);
        if(ret == -1){
            logmsg(LOG_WARNING, "irc: Could not read from network '%s', %s\n", network, strerror(errno));
            return -1;
        }
        if(ret == 0){
            goto server_reconnect;
        }
    }

    n->msgbuf_idx += ret;
    return 0;

    server_reconnect:
        watch_remove(n->sock);
        htable_remove(rc_network_sock, &n->sock, sizeof(n->sock));

        memset(n->msgbuf, '\0', n->msgbuf_size);
        n->msgbuf_idx = 0;

        irc_connect(network);
        return -2;
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

//size_t irc_msg_from_json(json_t* json_msg, char*** irc_msg){
//    return 0;
//}
