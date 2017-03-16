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
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "irc.h"
#include "log.h"
#include "nexus.h"

void signal_handle_sigchld(){
    sigchld = 1;
}

void signal_handle_sighup(){
    sighup = 1;
}

void signal_handle_sigpipe(){
    sigpipe = 1;
}

void signal_handle_sigterm(){
    sigterm = 1;
}

int signal_init(){
    sigset_t mask_set;
    sigfillset(&mask_set);

    struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = signal_handle_sigchld, .sa_mask = mask_set};
    if(sigaction(SIGCHLD, &sa, NULL) == -1){
       logmsg(LOG_ERR, "signals: Failed to install signal handler for SIGCHLD, %s", strerror(errno));
       return -1;
    }

    sa.sa_handler = signal_handle_sighup;
    if(sigaction(SIGHUP, &sa, NULL) == -1){
       logmsg(LOG_ERR, "signals: Failed to install signal handler for SIGHUP, %s", strerror(errno));
       return -1;
    }
    
    sa.sa_handler = signal_handle_sigpipe;
    if(sigaction(SIGPIPE, &sa, NULL) == -1){
       logmsg(LOG_ERR, "signals: Failed to install signal handler for SIGPIPE, %s", strerror(errno));
       return -1;
    }
    
    sa.sa_handler = signal_handle_sigterm;
    if(sigaction(SIGTERM, &sa, NULL) == -1){
       logmsg(LOG_ERR, "signals: Failed to install signal handler for SIGTERM, %s", strerror(errno));
       return -1;
    }

    return 0;
}
