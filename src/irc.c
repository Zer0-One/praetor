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
#include "htable.h"
#include "ircmsg.h"
#include "log.h"
#include "nexus.h"

int irc_msg_recv(struct network* n, char* buf, size_t len){
    char* eom = strstr(n->recv_queue, "\r\n");
    if(eom == NULL){
        return -1;
    }

    eom += 1;
    size_t bytes_to_read = eom - n->recv_queue + 1;

    if(len < bytes_to_read){
        logmsg(LOG_ERR, "irc: Could not read IRC message from network '%s', buffer too small\n", n->name);
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

int irc_register_connection(const struct network* n){
    if(n->pass != 0){
        if(irc_pass(n, n->pass) == -1){
            logmsg(LOG_WARNING, "irc: Could not set connection password on network '%s'\n", n->name);
            goto fail;
        }
    }
    
    if(irc_nick(n, n->nick) == -1){
        logmsg(LOG_WARNING, "irc: Could not set nickname on network '%s'\n", n->name);
        if(n->pass != 0){
            free(queue_dequeue(n->send_queue));
        }
        goto fail;
    }
    //logmsg(LOG_DEBUG, "we crafted the nick msg\n");
    
    if(irc_user(n, n->user, n->real_name) == -1){
        logmsg(LOG_WARNING, "irc: Could not set username on network '%s'\n", n->name);
        if(n->pass != 0){
            free(queue_dequeue(n->send_queue));
        }
        free(queue_dequeue(n->send_queue));
        goto fail;
    }

    logmsg(LOG_DEBUG, "irc: Connection registered with network '%s'\n", n->name);
    return 0;

    fail:
        logmsg(LOG_WARNING, "irc: Failed to register connection with network '%s'\n", n->name);
        return -1;
}

int irc_join_all(const struct network* n){
    size_t size = 0;
    struct htable_key** channels = htable_get_keys(n->channels, &size);
    if(channels == NULL){
        return -1;
    }

    for(size_t i = 0; i < size; i++){
        struct channel* c = htable_lookup(n->channels, channels[i]->key, strlen((char*)channels[i]->key)+1);
        if(irc_join(n, c->name, c->key) == -1){
            //If we don't have enough memory to queue all of the join messages, then don't send any at all
            for(size_t j = 0; j <= i; j++){
                queue_dequeue(n->send_queue);
            }
            htable_key_list_free(channels, size);
            logmsg(LOG_WARNING, "irc: Could not join channel '%s' on network '%s'", c->name, n->name);
            return -1;
        }
    }

    htable_key_list_free(channels, size);

    return 0;
}

int irc_handle_ping(const struct network* n, const char* buf){
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
        logmsg(LOG_WARNING, "irc: Could not reply to PING message from network '%s'\n", n->name);
        free(tmp);
        return -2;
    }

    free(tmp);
    return 0;
}
