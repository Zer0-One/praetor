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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "config.h"
#include "ircmsg.h"
#include "log.h"
#include "queue.h"

struct ircmsg* ircmsg_parse(const char* network, const char* msg, size_t len){
    char* sender = NULL;
    char* user = NULL;
    char* host = NULL;
    char* cmd = NULL;
    
    //Argument vector for the command args
    size_t argc = 0;
    char* argv[IRCMSG_CMD_PARAMS_MAX] = {NULL};

    char* tok;
    char* tok2;

    char* saveptr = NULL;
    char* saveptr2 = NULL;

    //Duplicate the string, since strtok will modify it
    char* msg_dup = malloc(len);
    if(msg_dup == NULL){
        goto fail_oom;
    }
    strncpy(msg_dup, msg, len);

    //Begin parsing
    tok = strtok_r(msg_dup, " ", &saveptr);
    if(tok == NULL){
        logmsg(LOG_WARNING, "ircmsg: Parsing error, expected prefix or command, but got nothing\n");
        goto fail;
    }

    //Parse message prefix
    if(tok[0] == ':'){
        //Ensure the prefix is not a bare ':' 
        if(strlen(tok) == 1){
            logmsg(LOG_WARNING, "ircmsg: Parsing error, malformed/empty prefix\n");
            goto fail;
        }

        //Advance the pointer forward to exclude the ':'
        tok++;

        //If the prefix specifies a username:
        if(strchr(tok, '!') != NULL){
            tok2 = strtok_r(tok, "!", &saveptr2);
            sender = malloc(strlen(tok2) + 1);
            if(sender == NULL){
                goto fail_oom;
            }
            strcpy(sender, tok2);

            tok2 = strtok_r(NULL, "@", &saveptr2);
            if(tok2 == NULL){
                logmsg(LOG_WARNING, "ircmsg: Parsing error, expected username in prefix, but got nothing\n");
                goto fail;
            }

            user = malloc(strlen(tok2) + 1);
            if(user == NULL){
                goto fail_oom;
            }
            strcpy(user, tok2);
            
            tok2 = strtok_r(NULL, "@", &saveptr2);
            if(tok2 == NULL){
                logmsg(LOG_WARNING, "ircmsg: Parsing error, expected hostname in prefix, but got nothing\n");
                goto fail;
            }
            
            host = malloc(strlen(tok2) + 1);
            if(host == NULL){
                goto fail_oom;
            }
            strcpy(host, tok2);
        }
        //Otherwise, if the prefix specifies a hostname with no user:
        else if(strchr(tok, '@') != NULL){
            tok2 = strtok_r(tok, "@", &saveptr2);
            sender = malloc(strlen(tok2) + 1);
            if(sender == NULL){
                goto fail_oom;
            }
            strcpy(sender, tok2);

            tok2 = strtok_r(NULL, "@", &saveptr2);
            if(tok2 == NULL){
                logmsg(LOG_WARNING, "ircmsg: Parsing error, expected hostname in prefix, but got nothing\n");
                goto fail;
            }

            host = malloc(strlen(tok2) + 1);
            if(host == NULL){
                goto fail_oom;
            }
            strcpy(host, tok2);
        }
        //The prefix only contains a sender nick or server
        else{
            sender = malloc(strlen(tok) + 1);
            if(sender == NULL){
                goto fail_oom;
            }

            strcpy(sender, tok);
        }

        tok2 = NULL;
        saveptr2 = NULL;
       
        //Parse the command after the prefix
        tok = strtok_r(NULL, " ", &saveptr);
        if(tok == NULL){
            logmsg(LOG_WARNING, "ircmsg: Parsing error, expected command, but got nothing\n");
            goto fail;
        }
    }
    
    //Parse the command, there was no prefix
    cmd = malloc(strlen(tok) + 1);
    if(cmd == NULL){
        goto fail_oom;
    }
    strcpy(cmd, tok);

    struct ircmsg* ret = malloc(sizeof(struct ircmsg));
    if(ret == NULL){
        goto fail_oom;
    }

    //Tokenize and save arguments
    for(size_t i = 0; i < sizeof(argv); i++){
        tok = strtok_r(NULL, " ", &saveptr);
        if(tok == NULL){
            break;
        }

        //If we encounter a trailing arg, read in the rest of the message as one arg
        //They have to be parsed this way, because a ':' may appear as part of "middle" argument
        if(tok[0] == ':'){
            tok2 = strtok_r(NULL, "", &saveptr);
            //If the trailing arg was a single word, save it and break
            if(tok2 == NULL){
                argv[i] = malloc(strlen(tok) + 1);
                //+1 to exclude the trailing argument delimiter, ':'
                strcpy(argv[i], tok + 1);
                
                argc++;
                break;
            }

            //Otherwise, combine the two tokens
            argv[i] = malloc(strlen(tok) + strlen(tok2) + 1);

            strncpy(argv[i], tok, strlen(tok));
            strncpy(argv[i] + strlen(tok), tok2, strlen(tok2) + 1);

            argc++;
            break;
        }

        argv[i] = malloc(strlen(tok) + 1);
        if(argv[i] == NULL){
            goto fail_oom;
        }
        strcpy(argv[i], tok);

        argc++;
    }

    //The number of args present will indicate which args are optional.
    //i.e If the format is "CMD [arg1] arg2", and you got 1 arg, it *must* be
    //the non-optional arg, "arg2". If you got two args, the first *must* be
    //the optional arg, "arg1".
    if(strcasecmp(cmd, "PING") == 0){
        if(argc > 2 || argc < 1){
            logmsg(LOG_WARNING, "ircmsg: Parsing error, expected 1-2 arguments for command '%s', got %zd\n", cmd, argc);
            goto fail;
        }

        struct ircmsg_ping* ping = malloc(sizeof(struct ircmsg_ping));
        if(ping == NULL){
            goto fail_oom;
        }

        ping->server = argv[0];
        ping->server2 = argv[1];

        ret->type = PING;
        ret->ping = ping;
    }
    else if(strcasecmp(cmd, "PRIVMSG") == 0){
        if(argc != 2){
            logmsg(LOG_WARNING, "ircmsg: Parsing error, expected 2 arguments for command '%s', got %zd\n", cmd, argc);
            goto fail;
        }
        
        struct ircmsg_privmsg* privmsg = malloc(sizeof(struct ircmsg_privmsg));
        if(privmsg == NULL){
            goto fail_oom;
        }

        privmsg->target = argv[0];
        privmsg->msg = argv[1];
        privmsg->is_hilight = false;
        privmsg->is_pm = false;

        ret->type = PRIVMSG;
        ret->privmsg = privmsg;
    }
    else if(strcasecmp(cmd, "JOIN") == 0){
        
    }
    else if(strcasecmp(cmd, "PART") == 0){
        
    }
    else if(strcasecmp(cmd, "QUIT") == 0){
        
    }
    else{
        ret->type = UNKNOWN;
    }

    char* n = malloc(strlen(network));
    if(n == NULL){
        goto fail_oom;
    }
    strcpy(n, network);

    ret->cmd = cmd;
    ret->host = host;
    ret->network = n;
    ret->sender = sender;
    ret->user = user;

    free(msg_dup);

    //logmsg(LOG_DEBUG, "network: %s\n", ret->network);
    //logmsg(LOG_DEBUG, "sender: %s\n", ret->sender);
    //logmsg(LOG_DEBUG, "user: %s\n", ret->user);
    //logmsg(LOG_DEBUG, "host: %s\n", ret->host);
    //logmsg(LOG_DEBUG, "cmd: %s\n", ret->cmd);

    return ret;

    fail_oom:
    logmsg(LOG_WARNING, "ircmsg: Parsing error, the system is out of memory\n");

    fail:
    logmsg(LOG_DEBUG, "ircmsg: Could not parse message: %s\n", msg);

    free(sender);
    free(user);
    free(host);
    free(cmd);
    free(msg_dup);
    for(size_t i = 0; i < argc; i++){
        free(argv[i]);
    }

    return NULL;
}

void ircmsg_free(struct ircmsg* msg){
    free(msg->network);
    free(msg->sender);
    free(msg->user);
    free(msg->host);
    free(msg->cmd);
    
    switch(msg->type){
        case PING:
            free(msg->ping->server);
            free(msg->ping->server2);
            break;
        case PONG:
            free(msg->pong->server);
            free(msg->pong->server2);
            break;
        case PRIVMSG:
            free(msg->privmsg->target);
            free(msg->privmsg->msg);
            break;
        default:
            break;
    }
}

char* ircmsg_join(const char* channels, const char* keys){
    char* msg = malloc(IRCMSG_SIZE_BUF);
    if(msg == NULL){
        logmsg(LOG_WARNING, "ircmsg: Could not allocate memory for JOIN message, the system is out of memory\n");
        return NULL;
    }

    int count = 0;
    if(keys == NULL){
        count = snprintf(msg, IRCMSG_SIZE_BUF, "JOIN %s\r\n", channels);
    }
    else{
        count = snprintf(msg, IRCMSG_SIZE_BUF, "JOIN %s %s\r\n", channels, keys);
    }

    if(count < 0){
        logmsg(LOG_WARNING, "ircmsg: Could not craft JOIN message, %s\n", strerror(errno));
        return NULL;
    }

    if(count >= IRCMSG_SIZE_BUF){
        logmsg(LOG_WARNING, "ircmsg: JOIN message truncated, size %d exceeded maximum message size\n", count);
    }

    return msg;
}

char* ircmsg_nick(const char* nick){
    char* msg = malloc(IRCMSG_SIZE_BUF);
    if(msg == NULL){
        logmsg(LOG_WARNING, "ircmsg: Could not allocate memory for NICK message, the system is out of memory\n");
        return NULL;
    }

    int count = snprintf(msg, IRCMSG_SIZE_BUF, "NICK %s\r\n", nick);
    if(count < 0){
        logmsg(LOG_WARNING, "ircmsg: Could not craft NICK message, %s\n", strerror(errno));
        return NULL;
    }

    if(count >= IRCMSG_SIZE_BUF){
        logmsg(LOG_WARNING, "ircmsg: NICK message truncated, size %d exceeded maximum message size\n", count);
    }

    return msg;
}

char* ircmsg_pass(const char* pass){
    char* msg = malloc(IRCMSG_SIZE_BUF);
    if(msg == NULL){
        logmsg(LOG_WARNING, "ircmsg: Could not allocate memory for PASS message, the system is out of memory\n");
        return NULL;
    }

    int count = snprintf(msg, IRCMSG_SIZE_BUF, "PASS :%s\r\n", pass);
    if(count < 0){
        logmsg(LOG_WARNING, "ircmsg: Could not craft PASS message, %s\n", strerror(errno));
        return NULL;
    }

    if(count >= IRCMSG_SIZE_BUF){
        logmsg(LOG_WARNING, "ircmsg: PASS message truncated, size %d exceeded maximum message size\n", count);
    }

    return msg;
}

char* ircmsg_pong(const char* server, const char* server2){
    char* msg = malloc(IRCMSG_SIZE_BUF);
    if(msg == NULL){
        logmsg(LOG_WARNING, "ircmsg: Could not allocate memory for PONG message, the system is out of memory\n");
        return NULL;
    }

    int count = 0;
    if(server2 == NULL){
        count = snprintf(msg, IRCMSG_SIZE_BUF, "PONG %s\r\n", server);
    }
    else{
        count = snprintf(msg, IRCMSG_SIZE_BUF, "PONG %s :%s\r\n", server, server2);
    }
    
    if(count < 0){
        logmsg(LOG_WARNING, "ircmsg: Could not craft PONG message, %s\n", strerror(errno));
        return NULL;
    }

    if(count >= IRCMSG_SIZE_BUF){
        logmsg(LOG_WARNING, "ircmsg: PONG message truncated, size %d exceeded maximum message size\n", count);
    }

    return msg;
}

//To-Do: Implement message-splitting here:
//  - Return an array of strings instead of a single string to accomodate text
//    that doesn't fit in a single message.
char* ircmsg_privmsg(const char* msgtarget, const char* text){
    char* msg = malloc(IRCMSG_SIZE_BUF);
    if(msg == NULL){
        logmsg(LOG_WARNING, "ircmsg: Could not allocate memory for PRIVMSG message, the system is out of memory\n");
        return NULL;
    }

    int count = snprintf(msg, IRCMSG_SIZE_BUF, "PRIVMSG %s :%s\r\n", msgtarget, text);
    if(count < 0){
        logmsg(LOG_WARNING, "ircmsg: Could not craft PRIVMSG message, %s\n", strerror(errno));
        return NULL;
    }

    if(count >= IRCMSG_SIZE_BUF){
        logmsg(LOG_WARNING, "ircmsg: PRIVMSG message truncated, size %d exceeded maximum message size\n", count);
    }

    return msg;
}

char* ircmsg_user(const char* user, const char* mode, const char* real_name){
    char* msg = malloc(IRCMSG_SIZE_BUF);
    if(msg == NULL){
        logmsg(LOG_WARNING, "ircmsg: Could not allocate memory for USER message, the system is out of memory\n");
        return NULL;
    }

    int count = snprintf(msg, IRCMSG_SIZE_BUF, "USER %s %s * :%s\r\n", user, mode, real_name);
    if(count < 0){
        logmsg(LOG_WARNING, "ircmsg: Could not craft USER message, %s\n", strerror(errno));
        return NULL;
    }

    if(count >= IRCMSG_SIZE_BUF){
        logmsg(LOG_WARNING, "ircmsg: USER message truncated, size %d exceeded maximum message size\n", count);
    }

    return msg;
}
