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
#include <stdbool.h>
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
#define NOMEM_WAIT_NANOSECONDS 500000000

//The number of pollfd structs currently being monitored for input.
size_t monitor_list_count = 0;

/**
 * An array of pollfd structs for monitoring I/O readiness in sockets for
 * plugin I/O and IRC network I/O.
 */
struct pollfd* monitor_list = NULL;

int watch_add(const int fd, bool connection){
    if(monitor_list != NULL){
        for(size_t i = 0; i < monitor_list_count; i++){
            if(monitor_list[i].fd == -1){
                monitor_list[i].fd = fd;
                if(connection){
                    monitor_list[i].events = POLLOUT;
                }
                else{
                    monitor_list[i].events = POLLIN;
                }
                return 0;
            }
        }
    }
    void* tmp = realloc(monitor_list, (monitor_list_count+1) * sizeof(struct pollfd));
    if(tmp == NULL){
        logmsg(LOG_WARNING, "nexus: Could not add socket to global monitor list, %s\n", strerror(errno));
        return -1;
    }
    monitor_list = tmp;
    monitor_list[monitor_list_count].fd = fd;
    if(connection){
        monitor_list[monitor_list_count].events = POLLOUT;
    }
    else{
        monitor_list[monitor_list_count].events = POLLIN;
    }
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

    struct timespec ts = {.tv_sec = NOMEM_WAIT_SECONDS, .tv_nsec = NOMEM_WAIT_NANOSECONDS};
    
    while(true){
        if(handle_signals() == -1){
            //If handling of any signal fails, we were out of memory
            //Sleep for a bit and then retry
            nanosleep(&ts, NULL);
            continue;
        }

        //The majority of our time is spent here.
        int poll_status = poll(monitor_list, monitor_list_count, 500);
        //Handle polling errors
        if(poll_status == -1){
            switch(errno){
                case EINTR:
                    logmsg(LOG_DEBUG, "nexus: Socket polling interrupted by signal, restarting\n");
                    continue;
                case EFAULT:
                case EINVAL:
                    logmsg(LOG_ERR, "nexus: Could not poll, %s\n", strerror(errno));
                    _exit(-1);
                case ENOMEM:
                    logmsg(LOG_WARNING, "nexus: Could not poll sockets, the system is out of memory\n");
                    logmsg(LOG_DEBUG, "nexus: Attempting another poll in %d seconds and %d nanoseconds\n", NOMEM_WAIT_SECONDS, NOMEM_WAIT_NANOSECONDS);
                    //Sleep for a quarter of a second and try again
                    nanosleep(&ts, NULL);
                    continue;
            }
        }
        //Try to flush all send queues
        else if(poll_status == 0){
            inet_send_all();
            continue;
        }

        for(size_t i = 0; i < monitor_list_count; i++){
            struct plugin* p = htable_lookup(rc_plugin_sock, &monitor_list[i].fd, sizeof(monitor_list[i].fd));
            struct network* n = htable_lookup(rc_network_sock, &monitor_list[i].fd, sizeof(monitor_list[i].fd));

            //Connection has been completed, check status
            if(monitor_list[i].revents & POLLOUT){
                if(inet_check_connection(n->name) == 0){
                    while(irc_register_connection(n->name) != 0){
                        logmsg(LOG_DEBUG, "fuck, we're stuck\n");
                        nanosleep(&ts, NULL);
                    }
                }
                else{
                    inet_connect(n->name);
                }
            }
            //There is input waiting on a socket queue
            else if(monitor_list[i].revents & POLLIN){
                //process network input
                if(n){
                    if(inet_recv(n->name) == -1){
                        continue;
                    }
                    char msg[MSG_SIZE_MAX];
                    while(irc_msg_recv(n->name, msg, MSG_SIZE_MAX) != -1){
                        while(irc_handle_ping(n->name, msg) == -2){
                            nanosleep(&ts, NULL);
                        }
                    }
                }
                //process plugin input
                else if(p){

                }
            }
        }
    }
}
