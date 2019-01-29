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
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "htable.h"
#include "inet.h"
#include "irc.h"
#include "ircmsg.h"
#include "log.h"
#include "plugin.h"
#include "signals.h"

#define NOMEM_WAIT_SECONDS 0
#define NOMEM_WAIT_NANOSECONDS 500000000
#define POLL_TIMEOUT 1000

//The size of the monitor array.
size_t monitor_list_size = 0;

//The number of pollfd structs currently being monitored for input.
size_t monitor_list_count = 0;

/**
 * An array of pollfd structs for monitoring I/O readiness in sockets for
 * plugin I/O and IRC network I/O.
 */
struct pollfd* monitor_list = NULL;

int watch_add(const int fd, bool connection){
    if(monitor_list != NULL){
        for(size_t i = 0; i < monitor_list_size; i++){
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
    void* tmp = realloc(monitor_list, (monitor_list_size+1) * sizeof(struct pollfd));
    if(tmp == NULL){
        logmsg(LOG_WARNING, "nexus: Could not add socket to global monitor list, %s\n", strerror(errno));
        return -1;
    }
    monitor_list = tmp;
    monitor_list[monitor_list_size].fd = fd;
    if(connection){
        monitor_list[monitor_list_size].events = POLLOUT;
    }
    else{
        monitor_list[monitor_list_size].events = POLLIN;
    }
    monitor_list_size++;
    monitor_list_count++;
    return 0;
}

void watch_remove(const int fd){
    for(size_t i = 0; i < monitor_list_size; i++){
        if(monitor_list[i].fd == fd){
            monitor_list[i].fd = -1;
            monitor_list[i].events = 0;
            monitor_list[i].revents = 0;

            monitor_list_count--;
            return;
        }
    }
}

void run(){
    struct timespec ts = {.tv_sec = NOMEM_WAIT_SECONDS, .tv_nsec = NOMEM_WAIT_NANOSECONDS};

    //If handling of any signal fails, we were out of memory
    if(handle_signals() == -1){
        //Sleep for a bit and then retry
        nanosleep(&ts, NULL);
        return;
    }

    if(monitor_list_count < 1){
        logmsg(LOG_ERR, "nexus: No sockets to monitor, exiting\n");
        _exit(0);
    }

    int poll_status = poll(monitor_list, monitor_list_size, POLL_TIMEOUT);
    if(poll_status == -1){
        switch(errno){
            case EINTR:
                logmsg(LOG_DEBUG, "nexus: Socket polling interrupted by signal, restarting\n");
                return;
            case EFAULT:
            case EINVAL:
                logmsg(LOG_ERR, "nexus: Could not poll, %s\n", strerror(errno));
                _exit(-1);
            case ENOMEM:
                logmsg(LOG_WARNING, "nexus: Could not poll sockets, the system is out of memory\n");
                logmsg(LOG_DEBUG, "nexus: Attempting another poll in %d seconds and %d nanoseconds\n", NOMEM_WAIT_SECONDS, NOMEM_WAIT_NANOSECONDS);
                //Sleep for a quarter of a second and try again
                nanosleep(&ts, NULL);
                return;
        }
    }
    //Try to flush all send queues
    else if(poll_status == 0){
        inet_send_all();
        return;
    }

    //Grab the current list of configured plugins
    size_t size = 0;
    struct htable_key** plugins = NULL;
    while((plugins = htable_get_keys(rc_plugin, &size)) == NULL){
        logmsg(LOG_WARNING, "nexus: Could not load list of configured plugins, the system is out of memory\n");
        logmsg(LOG_WARNING, "nexus: Attempting to load list again in %d seconds and %d nanoseconds\n", NOMEM_WAIT_SECONDS, NOMEM_WAIT_NANOSECONDS);
        nanosleep(&ts, NULL);
    }

    for(size_t i = 0; i < monitor_list_count; i++){
        if(monitor_list[i].fd == -1){
            continue;
        }

        struct network* n;
        struct plugin* p;

        n = htable_lookup(rc_network_sock, (uint8_t*)&monitor_list[i].fd, sizeof(monitor_list[i].fd));
        if(n == NULL){
           p = htable_lookup(rc_plugin_sock, (uint8_t*)&monitor_list[i].fd, sizeof(monitor_list[i].fd));
        }

        if(n != NULL){
            //Connection has been completed, check status
            if(monitor_list[i].revents & POLLOUT){
                if(inet_check_connection(n) == 0){
                    while(irc_register_connection(n) != 0){
                        nanosleep(&ts, NULL);
                    }
                    irc_join_all(n);
                }
                else{
                    inet_connect(n);
                }
            }
            //There is input waiting on a socket queue
            else if(monitor_list[i].revents & POLLIN){
                if(inet_recv(n) == -1){
                    continue;
                }

                struct ircmsg* parsed_msg = NULL;
                char msg[IRCMSG_SIZE_MAX];
                
                while(irc_recv(n, msg, IRCMSG_SIZE_MAX) != -1){
                    parsed_msg = ircmsg_parse(n->name, msg, strlen(msg));
                    if(parsed_msg->type == PING){
                        while(irc_handle_ping(n, parsed_msg) == -1){
                            nanosleep(&ts, NULL);
                        }
                    }
                }

                for(size_t j = 0; j < size; j++){
                    struct plugin* p_this = htable_lookup(rc_plugin, plugins[i]->key, plugins[i]->key_size);
                    plugin_send(p_this, parsed_msg);
                }
            }
        }
        else if(p != NULL){
            //Dispatch messages to networks according to ACLs and rate-limits
            //json_t* obj = plugin_recv(p);
            logmsg(LOG_DEBUG, "nexus: Plugin '%s' has data in the queue waiting to be read\n", p->name);
            //if(obj == NULL){
            //    continue;
            //}

            //char* network;
            //char* msg = ircmsg_from_json(obj, &network);

            //struct network* n_this = htable_lookup(rc_network, (uint8_t*)network, strlen(network)+1);
            //irc_send(n_this, msg, strlen(msg));
        }
        else{
            //This should never happen.
            logmsg(LOG_ERR, "nexus: Polled socket belonged to neither a network nor a plugin\n");
            _exit(-1);
        }
    }
}
