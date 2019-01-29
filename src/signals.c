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
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "htable.h"
#include "irc.h"
#include "log.h"
#include "nexus.h"
#include "plugin.h"

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
    size_t size = 0;
    struct htable_key** plugins = htable_get_keys(rc_plugin, &size);
    if(plugins == NULL){
        logmsg(LOG_WARNING, "signals: Failed to load list of configured plugins\n");
        logmsg(LOG_WARNING, "signals: There are no configured plugins, or the system is out of memory\n");
        return -1;
    }

    int wstatus;
    pid_t pid;
    struct plugin* p;
    while((pid = waitpid(-1, &wstatus, WNOHANG)) > 0){
        for(size_t i = 0; i < size; i++){
            p = htable_lookup(rc_plugin, plugins[i]->key, plugins[i]->key_size);
            if(pid == p->pid){
                if(p->status == PLUGIN_UNLOADED){
                    logmsg(LOG_WARNING, "signals: Plugin '%s' successfully terminated via unload\n", p->name);
                }
                else if (p->status == PLUGIN_LOADED){
                    logmsg(LOG_WARNING, "signals: Plugin '%s' terminated unexpectedly\n", p->name);
                    p->status = PLUGIN_DEAD;
                    plugin_unload(p);
                }
                else{
                    //Process was waited on before, but we never cleaned it up. This should never happen.
                    logmsg(LOG_ERR, "signals: Unable to clean-up terminated plugin '%s', exiting\n", p->name);
                    _exit(-1);
                }
                goto success;
            }
        }

        //A child died, but it wasn't a plugin. This should never happen.
        logmsg(LOG_ERR, "signals: Child process (%d) died, but was not a mapped plugin\n", pid);
        _exit(-1);

        success:

        if(WIFSIGNALED(wstatus)){
            //if we move to SUSv4, use strsignal() here
            logmsg(LOG_WARNING, "signals: Plugin '%s' terminated due to unhandled signal: %d\n", p->name, WTERMSIG(wstatus));
        }
        else if(WIFEXITED(wstatus)){
            logmsg(LOG_WARNING, "signals: Plugin '%s' exited with status: %d\n", p->name, WEXITSTATUS(wstatus));
        }
        else{
            logmsg(LOG_WARNING, "signals: Plugin '%s' was terminated via black magic\n", p->name);
        }

        p->pid = -1;
    }

    if(pid == -1){
        switch(errno){
            //None of these should ever happen
            case EINTR:
            case EINVAL:
                logmsg(LOG_ERR, "signal: Error on waitpid(), %s\n", strerror(errno));
                _exit(-1);
        }
    }

    htable_key_list_free(plugins, size);
    return 0;
}

int sighup_handler(){
    return 0;
}

int sigpipe_handler(){
    return 0;
}

void sigterm_handler(){
    //irc_disconnect_all();
    _exit(-1);
}

int handle_signals(){
    sigset_t mask_set;
    sigemptyset(&mask_set);

    /*
     * If we receive SIGCHLD during handler execution, we might set
     * sigchld = 0 without cleaning up a newly-terminated child.
     * Setting the signal mask like this guards against that
     * possibility. If the handler does clean up the newly-terminated
     * child before the next time it's called, it'll safely return 0
     * the next time it's called.
     */
    sigaddset(&mask_set, SIGCHLD);
    sigaddset(&mask_set, SIGPIPE);
    sigprocmask(SIG_BLOCK, &mask_set, NULL);

    if(sigchld){
        if(sigchld_handler() == -1){
            return -1;
        }
        sigchld = 0;
    }
    else if(sighup){
        if(sighup_handler() == -1){
            return -1;
        }
        sighup = 0;
    }
    else if(sigpipe){
        if(sigpipe_handler() == -1){
            return -1;
        }
        sigpipe = 0;
    }
    else if(sigterm){
        sigterm_handler();
    }

    sigprocmask(SIG_UNBLOCK, &mask_set, NULL);
    sigemptyset(&mask_set);

    return 0;
}
