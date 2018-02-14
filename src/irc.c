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

int irc_recv(struct network* n, char* buf, size_t len){
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
    if(n->pass != NULL){
        char* pass = ircmsg_pass(n->pass);
        if(pass == NULL){
            goto fail_pass;
        }
        if(queue_enqueue(n->send_queue, pass, strlen(pass)) == -1){
            goto fail_pass;
        }
    }

    char* nick = ircmsg_nick(n->nick);
    if(nick == NULL){
        goto fail_nick;
    }
    if(queue_enqueue(n->send_queue, nick, strlen(nick)) == -1){
        goto fail_nick;
    }

    //mode is hard-coded here because we haven't made this a user-configurable option yet
    char* user = ircmsg_user(n->user, "0", n->real_name);
    if(user == NULL){
        goto fail_user;
    }
    if(queue_enqueue(n->send_queue, user, strlen(user)) == -1){
        goto fail_user;
    }

    return 0;

    fail_pass:
            logmsg(LOG_WARNING, "irc: Could not register connection with network %s, unable to set connection password\n", n->name);
            return -1;
    fail_nick:
            if(n->pass != NULL){
                free(queue_dequeue(n->send_queue));
            }
            logmsg(LOG_WARNING, "irc: Could not register connection with network %s, unable to set nickname\n", n->name);
            return -1;
    fail_user:
            if(n->pass != NULL){
                free(queue_dequeue(n->send_queue));
            }
            free(queue_dequeue(n->send_queue));
            logmsg(LOG_WARNING, "irc: Could not register connection with network %s, unable to set username/hostname/realname\n", n->name);
            return -1;
}

int irc_join_all(const struct network* n){
    size_t size = 0;
    struct htable_key** channels = htable_get_keys(n->channels, &size);
    if(channels == NULL){
        return -1;
    }

    size_t i;
    for(i = 0; i < size; i++){
        struct channel* c = htable_lookup(n->channels, channels[i]->key, strlen((char*)channels[i]->key)+1);

        char* join = ircmsg_join(c->name, c->key);
        if(join == NULL){
            logmsg(LOG_WARNING, "irc: Could not queue JOIN messages for network '%s', the system is out of memory\n", n->name);
            goto fail;
        }

        if(queue_enqueue(n->send_queue, join, strlen(join)) == -1){
            logmsg(LOG_WARNING, "irc: Could not join channel '%s' on network '%s' because a JOIN message could not be queued, the system is out of memory\n", c->name, n->name);
            goto fail;
        }
    }

    htable_key_list_free(channels, size);

    return 0;

    fail:
        //If we don't have enough memory to queue all of the join messages, then don't send any at all
        for(size_t j = 0; j < i; j++){
            free(queue_dequeue(n->send_queue));
        }
        htable_key_list_free(channels, size);
        return -1;
}

int irc_handle_ping(const struct network* n, const struct ircmsg* msg){
    //If this isn't actually PING, we could segfault
    if(msg->type != PING){
        logmsg(LOG_ERR, "irc: Attempted to handle PING, but the message type was incorrect\n");
        _exit(-1);
    }

    char* pong = ircmsg_pong(n->user, msg->ping->server);
    if(pong == NULL){
        logmsg(LOG_WARNING, "irc: Unable to handle PING message, could not craft response message\n");
        return -1;
    }

    if(queue_enqueue(n->send_queue, pong, strlen(pong)) == -1){
        logmsg(LOG_WARNING, "irc: Unable to handle PING message, could not queue response\n");
        free(pong);
        return -1;
    }

    free(pong);
    return 0;
}
