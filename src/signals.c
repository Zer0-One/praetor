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

void signal_handle_sigchld(int sig){
    //NOTE: multiple children may have terminated, so handle *all* of them
    //log the exit status of the child.
    /**
     * run clean-up. remove fds from watch-list, remove entry from
     * rc_plugin_sock, and close UNIX domain sockets for the plugin.
     */
    //should probably not touch global state here. set a flag, handle all of this somewhere along the original thread of execution
}

void signal_handle_sighup(int sig){
    //handle re-initialization of the daemon    
}

void signal_handle_sigpipe(int sig){
    //handle remote end of a socket hanging up unexpectedly (for both plugins and networks)
    //    - clean up (kill the plugin process) and attempt to re-connect (re-start the plugin)
    //    - if reconnect fails, give up (admin can attempt to initiate another connection via IRC)
}

void signal_handle_sigterm(int sig){
    //allow the user to specify a quit message?
    irc_disconnect_all(NULL, 0);
    _exit(0);
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
