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

#define FORMAT_JOIN "JOIN %s %s"
#define FORMAT_NICK "NICK %s"
#define FORMAT_PASS "PASS %s"
#define FORMAT_PING "PONG %s"
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

int irc_msg_recv(const char* network, char* buf, size_t len){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char* eom = strstr(n->recv_queue, "\r\n");
    if(eom == NULL){
        return -1;
    }

    eom += 1;
    size_t bytes_to_read = eom - n->recv_queue + 1;

    if(len < bytes_to_read){
        logmsg(LOG_ERR, "irc: Could not read IRC message from network '%s', buffer too small\n", network);
        _exit(-1);
    }

    //The number of characters left over in the buffer
    size_t remainder = strlen(n->recv_queue + bytes_to_read);

    //Copy message to the buffer, and null-terminate it
    strncpy(buf, n->recv_queue, bytes_to_read);
    buf[bytes_to_read] = '\0';
    logmsg(LOG_DEBUG, "%s", buf);

    //Shift the remaining text to the front of the receive queue
    memmove(n->recv_queue, eom + 1, remainder);
    memset(n->recv_queue + remainder, '\0', n->recv_queue_size - remainder);
    n->recv_queue_idx = remainder;

    return 0;
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

    //if(queue_enqueue(n->send_queue, pass, strlen(nick)) == -1){
    //    goto fail;
    //}
    if(queue_enqueue(n->send_queue, nick, strlen(nick)) == -1){
        //queue_dequeue(n->send_queue);
        goto fail;
    }
    if(queue_enqueue(n->send_queue, user, strlen(user)) == -1){
        //queue_dequeue(n->send_queue);
        queue_dequeue(n->send_queue);
        goto fail;
    }

    //if(inet_send(network, nick, strlen(nick)) == -1 || inet_send(network, user, strlen(user)) == -1){
    //    free(pass); free(nick); free(user);
    //    logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", network);
    //    return -1;
    //}
    
    //logmsg(LOG_DEBUG, "irc: Connection registered with network '%s'\n", network);
    free(pass); free(nick); free(user);
    return 0;

    fail:
        logmsg(LOG_WARNING, "irc: Could not register connection with network '%s'\n", network);
        free(pass); free(nick); free(user);
        return -1;
}

int irc_join(const char* network, const char* channels, const char* keys){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    char* join = irc_msg_build(FORMAT_JOIN, channels, keys);
    if(join == NULL){
        logmsg(LOG_WARNING, "irc: Could not join channel(s) '%s' with keys '%s' on network '%s'\n", channels, keys, network);
        return -1;
    }

    if(queue_enqueue(n->send_queue, join, strlen(join)) == -1){
        logmsg(LOG_WARNING, "irc: Could not join channel(s) '%s' with keys '%s' on network '%s'\n", channels, keys, network);
        free(join);
        return -1;
    }

    free(join);

    return 0;
}

int irc_handle_ping(const char* network, const char* buf){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "irc: No configuration found for network '%s'\n", network);
        return -1;
    }

    if(strstr(buf, "PING") != buf){
        return -1;
    }

    char* tmp;
    const char* ptr;
    
    //If there's a message prefix, skip it
    if(buf[0] != ':'){
        ptr = buf;
    }
    else{
        ptr = strchr(buf, ' ') + 1;
    }

    tmp = calloc(1, strlen(ptr));
    if(tmp == NULL){
        return -2;
    }

    strncpy(tmp, ptr, strlen(ptr));
    //Do you like this
    tmp[1] = 'O';
    
    if(queue_enqueue(n->send_queue, tmp, strlen(buf))){
        logmsg(LOG_WARNING, "irc: Could not reply to PING message from network '%s'\n", network);
        return -2;
    }

    free(tmp);
    return 0;
}
