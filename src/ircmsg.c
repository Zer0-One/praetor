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
#include <unistd.h>

#include "config.h"
#include "irc.h"
#include "log.h"
#include "queue.h"

#define FORMAT_JOIN "JOIN %s %s"
#define FORMAT_NICK "NICK %s"
#define FORMAT_PASS "PASS %s"
#define FORMAT_PONG "PONG %s"
#define FORMAT_USER "USER %s 0 * :%s"

/**
 * Builds an IRC message from the given format string.
 *
 * This function returns dynamically-allocated memory that should be freed by
 * the caller.
 *
 * \param fmt A printf() format string describing an IRC message.
 *
 * \return On success, returns a pointer to dynamically-allocated memory which
 *         should be freed when it is no longer needed.
 * \return On failure, returns NULL.
 */
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
        free(msg);
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

int irc_join(const struct network* n, const char* channels, const char* keys){
    char* join_msg = irc_msg_build(FORMAT_JOIN, channels, keys);
    if(join_msg == NULL){
        return -1;
    }

    if(queue_enqueue(n->send_queue, join_msg, strlen(join_msg)) == -1){
        free(join_msg);
        return -1;
    }

    free(join_msg);

    return 0;
}

int irc_nick(const struct network* n, const char* nick){
    char* nick_msg = irc_msg_build(FORMAT_NICK, nick);
    if(nick_msg == NULL){
        return -1;
    }
    
    if(queue_enqueue(n->send_queue, nick_msg, strlen(nick_msg)) == -1){
        free(nick_msg);
        return -1;
    }

    free(nick_msg);

    return 0;
}

int irc_pass(const struct network* n, const char* pass){
    char* pass_msg = irc_msg_build(FORMAT_PASS, pass);
    if(pass_msg == NULL){
        return -1;
    }
    
    if(queue_enqueue(n->send_queue, pass_msg, strlen(pass_msg)) == -1){
        free(pass_msg);
        return -1;
    }

    free(pass_msg);

    return 0;
}

int irc_pong(const struct network* n, const char* pong){
    char* pong_msg = irc_msg_build(FORMAT_PONG, pong);
    if(pong_msg == NULL){
        return -1;
    }
    
    if(queue_enqueue(n->send_queue, pong_msg, strlen(pong_msg)) == -1){
        free(pong_msg);
        return -1;
    }

    free(pong_msg);

    return 0;
}

int irc_user(const struct network* n, const char* user, const char* real_name){
    char* user_msg = irc_msg_build(FORMAT_USER, user, real_name);
    if(user_msg == NULL){
        return -1;
    }
    
    if(queue_enqueue(n->send_queue, user_msg, strlen(user_msg)) == -1){
        free(user_msg);
        return -1;
    }

    free(user_msg);

    return 0;
}
