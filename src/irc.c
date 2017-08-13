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
#include <stdarg.h>
#include <stdio.h>
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

//int irc_register_connection(const char* network){
//    struct network* n;
//    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
//        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
//        return -1;
//    }
//
//    char buf[MSG_SIZE_MAX];
//
//    int count;
//    count = snprintf(buf, MSG_SIZE_MAX, "NICK %s\r\n", n->nick);
//    if(irc_msg_send(network, buf, count) == -1){
//        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", n->name);
//        return -1;
//    }
//    //handle_numeric()
//    count = snprintf(buf, MSG_SIZE_MAX, "USER %s 8 * :%s\r\n", n->user, n->real_name);
//    if(irc_msg_send(network, buf, count) == -1){
//        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", n->name);
//        return -1;
//    }
//    //handle_numeric()
//
//    return 0;
//}

//int irc_disconnect(const char* network){
//    struct network* n;
//    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
//        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
//        return -1;
//    }
//
//    char buf[MSG_SIZE_MAX];
//    if(n->quit_msg != NULL){
//        int count = snprintf(buf, MSG_SIZE_MAX, "QUIT :%s\r\n", n->quit_msg);
//        if(irc_msg_send(network, buf, count) == -1){
//            logmsg(LOG_WARNING, "irc: Could not quit from network '%s'\n", n->name);
//        }
//    }
//    else{
//        if(irc_msg_send(network, "QUIT", 4) == -1){
//            logmsg(LOG_WARNING, "irc: Could not quit from network '%s'\n", n->name);
//        }
//    }
//
//    if(close(n->sock) == -1){
//        logmsg(LOG_WARNING, "irc: Could not disconnect from network '%s'\n", n->name);
//    }
//    watch_remove(n->sock);
//    htable_remove(rc_network_sock, &n->sock, sizeof(n->sock));
//    
//    return 0;
//}

//int irc_disconnect_all(){
//    struct list* networks = htable_get_keys(rc_network_sock, false);
//
//    if(networks == NULL){
//        logmsg(LOG_WARNING, "irc: Failed to load list of configured networks\n");
//        logmsg(LOG_WARNING, "irc: There are no configured networks, or the system is out of memory\n");
//        return -1;
//    }
//
//    int ret = 0;
//    for(struct list* this = networks; this != 0; this = this->next){
//        struct network* n = htable_lookup(rc_network_sock, this->key, this->size);
//        if(irc_disconnect(n->name) < 0){
//            ret = -1;
//        }
//    }
//
//    htable_key_list_free(networks, false);
//
//    return ret;
//}
//
//int irc_channel_join(const char* network, const char* channel){
//    struct network* n;
//    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
//        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
//        return -1;
//    }
//
//    char buf[MSG_SIZE_MAX];
//    int count;
//    
//    struct channel* c;
//    if((c = (struct channel*)htable_lookup(n->channels, channel, strlen(channel)+1)) != NULL){
//        if(c->password != 0){
//            count = snprintf(buf, MSG_SIZE_MAX, "JOIN %s %s\r\n", c->name, c->password);
//        }
//        else{
//            count = snprintf(buf, MSG_SIZE_MAX, "JOIN %s\r\n", channel);
//        }
//    }
//    else{
//        count = snprintf(buf, MSG_SIZE_MAX, "JOIN %s\r\n", channel);
//    }
//
//    if(irc_msg_send(network, buf, count) == -1){
//        logmsg(LOG_WARNING, "irc: Could not join channel '%s' on network '%s'\n", channel, n->name);
//        return -1;
//    }
//    
//    return 0;
//}
//
//int irc_channel_part(const char* network, const char* channel){
//    struct network* n;
//    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
//        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
//        return -1;
//    }
//
//    char buf[MSG_SIZE_MAX];
//    int count = snprintf(buf, MSG_SIZE_MAX, "PART %s\r\n", channel);
//    if(irc_msg_send(network, buf, count) == -1){
//        logmsg(LOG_WARNING, "irc: Could not part channel '%s' on network '%s'\n", channel, n->name);
//        return -1;
//    }
//    
//    return 0;
//}

//To-Do: Add debug logging to display sent messages
//int irc_msg_send(const char* network, const char* buf, size_t len){
//    struct network* n;
//    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
//        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
//        return -1;
//    }
//   
//    ssize_t count;
//    if(n->ssl){
//        if(len < MSG_SIZE_MAX){
//            if((count = tls_write(n->ctx, buf, len)) == -1){
//                logmsg(LOG_WARNING, "irc: Could not send message on network '%s', %s\n", n->name, tls_error(n->ctx));
//                return -1;
//            }
//        }
//        else{
//            if((count = tls_write(n->ctx, buf, MSG_SIZE_MAX)) == -1){
//                logmsg(LOG_WARNING, "irc: Could not send message on network '%s', %s\n", n->name, tls_error(n->ctx));
//                return -1;
//            }
//            logmsg(LOG_WARNING, "irc: Message to network '%s' truncated; size exceeded 512 characters.\n", n->name);
//        }
//        return count;
//    }
//
//    if(len < MSG_SIZE_MAX){
//        if((count = send(n->sock, buf, len, 0)) == -1){
//            logmsg(LOG_WARNING, "irc: Could not send message on network '%s', %s\n", n->name, strerror(errno));
//            return -1;
//        }
//    }
//    else{
//        if((count = send(n->sock, buf, MSG_SIZE_MAX, 0)) == -1){
//            logmsg(LOG_WARNING, "irc: Could not send message on network '%s', %s\n", n->name, strerror(errno));
//            return -1;
//        }
//        logmsg(LOG_WARNING, "irc: Message to network '%s' truncated; size exceeded 512 characters.\n", n->name);
//    }
//
//    return count;
//}
//
//int irc_handle_ping(const char* network, const char* buf, size_t len){
//    if(strncmp(buf, "PING", 4) == 0){
//        char msg[len];
//        memcpy(msg, buf, len);
//        msg[1] = 'O';
//        if(irc_msg_send(network, msg, len) == -1){
//            return -1;
//        }
//        logmsg(LOG_DEBUG, "RESPONSE: %.*s", (int)len, msg);
//    }
//
//    return 0;
//}

#define FORMAT_PASS "PASS %s"
#define FORMAT_NICK "NICK %s"
#define FORMAT_USER "USER %s %s %s * :%s"


char* irc_msg_build(const char* fmt, ...){
    char* msg = calloc(1, MSG_SIZE_MAX + 1);
    if(msg == NULL){
        return NULL;
    }

    va_list ap;
    va_start(ap, fmt);
    
    int s = vsnprintf(msg, MSG_BODY_MAX, fmt, ap);
    
    va_end(ap);

    if(s < 0){
        logmsg(LOG_DEBUG, "irc: Could not build message '%s'\n", fmt);
        logmsg(LOG_WARNING, "irc: Could not build message, %s\n", strerror(errno));
        if(errno == EINVAL || errno == EOVERFLOW){
            _exit(-1);
        }
        return NULL;
    }
    
    if(s >= MSG_SIZE_MAX){
        logmsg(LOG_DEBUG, "irc: Could not build message '%s'\n", fmt);
        logmsg(LOG_WARNING, "irc: Could not build message, message parameter too long\n");
        free(msg);
        return NULL;
    }

    msg[s] = '\r';
    msg[s + 1] = '\n';

    return msg;
}

int irc_register_connection(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    //Need to add a "password" field to the config...
    char* pass = irc_msg_build(FORMAT_PASS, "blah");
    if(pass == NULL){
        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", network);
        return -1;
    }
    
    char* nick = irc_msg_build(FORMAT_NICK, n->nick);
    if(nick == NULL){
        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", network);
        free(pass);
        return -1;
    }
    //Need to add a "user_mode" field to the config...
    char* user = irc_msg_build(FORMAT_USER, n->user, "8", n->real_name);
    if(user == NULL){
        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", network);
        free(pass); free(nick);
        return -1;
    }

    if( 
        //inet_send(network, pass, strlen(pass)) == -1 ||
        inet_send(network, nick, strlen(nick)) == -1 ||
        inet_send(network, user, strlen(user)) == -1
      )
    {
        free(pass); free(nick); free(user);
        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", network);
        return -1;
    }

    free(pass); free(nick); free(user);
    return 0;
}

int irc_register_connection_all(){
    struct list* networks = htable_get_keys(rc_network_sock, false);

    if(networks == NULL){
        logmsg(LOG_WARNING, "irc: Failed to load list of configured networks\n");
        logmsg(LOG_WARNING, "irc: There are no configured networks or the system is out of memory\n");
        return -1;
    }

    for(struct list* this = networks; this != 0; this = this->next){
        irc_register_connection(this->key);
    }

    htable_key_list_free(networks, false);
   
    return 0;
}

int irc_msg_recv(const char* network, char* buf, size_t len){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char* eom = strstr(n->msgbuf, "\r\n");
    if(eom == NULL){
        logmsg(LOG_DEBUG, "irc: Could not read a complete IRC message from '%s'\n", network);
        return -1;
    }

    eom += 1;
    size_t bytes_to_read = eom - n->msgbuf + 1;

    if(len < bytes_to_read){
        logmsg(LOG_ERR, "irc: Could not read IRC message from network '%s', buffer too small\n", network);
        _exit(-1);
    }

    //The number of characters left over in the buffer
    size_t remainder = strlen(n->msgbuf + bytes_to_read);

    //Copy message to the buffer, and null-terminate it
    strncpy(buf, n->msgbuf, bytes_to_read);
    buf[bytes_to_read] = '\0';

    //Shift the remaining text to the front of the msgbuf
    memmove(n->msgbuf, eom + 1, remainder);
    memset(n->msgbuf + remainder, '\0', n->msgbuf_size - remainder);
    n->msgbuf_idx = remainder;

    return 0;
}

//size_t irc_msg_from_json(json_t* json_msg, char*** irc_msg){
//    return 0;
//}
