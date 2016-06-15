/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015,2016 David Zero
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
#include <unistd.h>

#include "config.h"
#include "hashtable.h"
#include "log.h"

#define MSG_BUF_SIZE 500

/**
 * The number of pollfd structs currently being monitored for input.
 */
size_t monitor_list_count = 0;

/**
 * An array of pollfd structs for monitoring I/O readiness in both pipes for
 * plugin I/O, and sockets for IRC network I/O.
 */
struct pollfd* monitor_list = NULL;

int watch_add(const int fd){
    if(monitor_list != NULL){
        for(int i = 0; i < monitor_list_count; i++){
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
    for(int i = 0; i < monitor_list_count; i++){
        if(monitor_list[i].fd == fd){
            monitor_list[i].fd = -1;
            monitor_list[i].events = 0;
            monitor_list[i].revents = 0;
        }
    }
}

void run(){
    if(monitor_list_count < 1){
        logmsg(LOG_ERR, "nexus: There are no sockets to monitor, dummy\n");
        return;
    }
    char msg[MSG_BUF_SIZE];
    while(true){
        if(poll(monitor_list, monitor_list_count, -1) < 1){
            //poll will never return 0, because we never time out
            //check to see if we were interrupted by a signal, or if we ran out of memory
        }
        for(int i = 0; i < monitor_list_count; i++){
            if(monitor_list[i].revents & POLLIN){
                struct networkinfo* n = htable_lookup(rc_network_sock, &monitor_list[i].fd, sizeof(monitor_list[i].fd));

                logmsg(LOG_DEBUG, "%s\n", msg);
            }
        }
    }
}
