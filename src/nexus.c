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
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "hashtable.h"
#include "inet.h"
#include "irc.h"
#include "log.h"
#include "plugin.h"
#include "signals.h"

#define NOMEM_WAIT_SECONDS 0
#define NOMEM_WAIT_NANOSECONDS 250000000

//The number of pollfd structs currently being monitored for input.
size_t monitor_list_count = 0;

/**
 * An array of pollfd structs for monitoring I/O readiness in sockets for
 * plugin I/O and IRC network I/O.
 */
struct pollfd* monitor_list = NULL;

int watch_add(const int fd){
    if(monitor_list != NULL){
        for(size_t i = 0; i < monitor_list_count; i++){
            if(monitor_list[i].fd == -1){
                monitor_list[i].fd = fd;
                monitor_list[i].events = POLLIN;
                return 0;
            }
        }
    }
    void* tmp = realloc(monitor_list, (monitor_list_count+1) * sizeof(struct pollfd));
    if(tmp == NULL){
        logmsg(LOG_WARNING, "nexus: Could not allocate space for struct pollfd, %s", strerror(errno));
        return -1;
    }
    monitor_list = tmp;
    monitor_list[monitor_list_count].fd = fd;
    monitor_list[monitor_list_count].events = POLLIN;
    monitor_list_count++;
    return 0;
}

void watch_remove(const int fd){
    for(size_t i = 0; i < monitor_list_count; i++){
        if(monitor_list[i].fd == fd){
            monitor_list[i].fd = -1;
            monitor_list[i].events = 0;
            monitor_list[i].revents = 0;
        }
    }
}

void run(){
    if(monitor_list_count < 1){
        logmsg(LOG_ERR, "nexus: No sockets to monitor, exiting\n");
        return;
    }
    
    sigset_t mask_set;
    sigemptyset(&mask_set);

    struct timespec ts = {.tv_sec = NOMEM_WAIT_SECONDS, .tv_nsec = NOMEM_WAIT_NANOSECONDS};

    while(true){
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
        
        //If any of these fail, we sleep for a quarter-second and retry until
        //we can acquire enough memory to run the handler completely
        if(sigchld){
            if(sigchld_handler() == -1){
                nanosleep(&ts, NULL);
                continue;
            }
            sigchld = 0;
        }
        else if(sighup){
            if(sighup_handler() == -1){
                nanosleep(&ts, NULL);
                continue;
            }
            sighup = 0;
        }
        else if(sigpipe){
            if(sigpipe_handler() == -1){
                nanosleep(&ts, NULL);
                continue;
            }
            sigpipe = 0;
        }
        else if(sigterm){
            sigterm_handler();
        }
            
        sigprocmask(SIG_UNBLOCK, &mask_set, NULL);
        sigemptyset(&mask_set);

        int poll_status = poll(monitor_list, monitor_list_count, -1);
        if(poll_status == -1){
            switch(errno){
                case EINTR:
                    logmsg(LOG_DEBUG, "nexus: Socket polling interrupted by signal, restarting\n");
                    break;
                case EFAULT:
                    logmsg(LOG_ERR, "nexus: Global socket monitor list was outside of the program's address space\n");
                    _exit(-1);
                case EINVAL:
                    logmsg(LOG_ERR, "nexus: Too many open file descriptors\n");
                    _exit(-1);
                case ENOMEM:
                    logmsg(LOG_WARNING, "nexus: Could not poll sockets, the system is out of memory\n");
                    logmsg(LOG_DEBUG, "nexus: Attempting another poll in %d seconds and %d nanoseconds\n", NOMEM_WAIT_SECONDS, NOMEM_WAIT_NANOSECONDS);
                    //Sleep for a quarter of a second and try again
                    nanosleep(&ts, NULL);
            }
        }

        for(size_t i = 0; i < monitor_list_count; i++){
            if(monitor_list[i].revents & POLLIN){
                struct plugin* p = htable_lookup(rc_plugin_sock, &monitor_list[i].fd, sizeof(monitor_list[i].fd));
                struct network* n = htable_lookup(rc_network_sock, &monitor_list[i].fd, sizeof(monitor_list[i].fd));
                //process network input
                if(n){
                    irc_msg_recv(n->name);
                }
                //process plugin input
                else if(p){
                }
            }
        }
    }
}
