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
#include <sys/wait.h>

#include "config.h"
#include "hashtable.h"
#include "irc.h"
#include "log.h"
#include "nexus.h"

volatile sig_atomic_t sigchld = 0;
volatile sig_atomic_t sighup = 0;
volatile sig_atomic_t sigpipe = 0;
volatile sig_atomic_t sigterm = 0;

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

int sigchld_handler(){
    struct list* plugins = htable_get_keys(rc_plugin, false);
    if(plugins == NULL){
        logmsg(LOG_WARNING, "signals: Failed to load list of configured plugins\n");
        logmsg(LOG_WARNING, "signals: There are no configured plugins, or the system is out of memory\n");
        return -1;
    }

    int wstatus;
    pid_t pid;
    struct plugin* p;
    while((pid = waitpid(-1, &wstatus, WNOHANG)) > 0){
        for(struct list* this = plugins; this != 0; this = this->next){
            p = htable_lookup(rc_plugin, this->key, this->size);
            if(p->pid == pid){
                goto success;
            }
        }

        //A child died, but it wasn't a plugin. Dafuq?
        logmsg(LOG_ERR, "signals: A child died, but was not a mapped plugin\n");
        _exit(-1);

        success:

        if(WIFSIGNALED(wstatus)){
            //if we move to SUSv4, use strsignal() here
            logmsg(LOG_WARNING, "signals: Plugin '%s' terminated due to unhandled signal: %d\n", p->name, WTERMSIG(wstatus));
        }
        else{
            logmsg(LOG_WARNING, "signals: Plugin '%s' exited with status: %d\n", p->name, WEXITSTATUS(wstatus));
        }

        watch_remove(p->sock);
        htable_remove(rc_plugin_sock, &p->sock, sizeof(&p->sock));
        close(p->sock);
        p->pid = 0;
        p->sock = 0;
    }

    if(pid == -1){
        switch(errno){
            //None of these should ever happen
            case EINTR:
            case EINVAL:
                logmsg(LOG_ERR, "signal: %s\n", strerror(errno));
                logmsg(LOG_ERR, "signal: Software failure. Press left mouse button to continue. Guru Meditation #b00b.b00b.b00b\n");
                _exit(-1);
        }
    }

    htable_key_list_free(plugins, false);
    return 0;
}

int sighup_handler(){
    return 0;
}

int sigpipe_handler(){
    return 0;
}

void sigterm_handler(){
    irc_disconnect_all();
    _exit(-1);
}
