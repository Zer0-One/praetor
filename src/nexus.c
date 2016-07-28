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
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "hashtable.h"
#include "inet.h"
#include "irc.h"
#include "log.h"

/**
 * The number of pollfd structs currently being monitored for input.
 */
size_t monitor_list_count = 0;

/**
 * An array of pollfd structs for monitoring I/O readiness in sockets for
 * plugin I/O and IRC network I/O.
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
        logmsg(LOG_ERR, "nexus: No sockets to monitor, exiting\n");
        return;
    }
    
    while(true){
        if(poll(monitor_list, monitor_list_count, -1) < 1){
            //poll will never return 0, because we never time out
            //check to see if we were interrupted by a signal, or if we ran out of memory
        }
        for(int i = 0; i < monitor_list_count; i++){
            if(monitor_list[i].revents & POLLIN){// && monitor_list[i].revents & POLLOUT){
                struct networkinfo* n = htable_lookup(rc_network_sock, &monitor_list[i].fd, sizeof(monitor_list[i].fd));
                ssize_t ret;
                //copy bytes from the current position in the buffer until the end, or until we run out of bytes to copy
                if(n->ssl){
                    if((ret = tls_read(n->ctx, n->msg + n->msg_pos, MSG_SIZE - n->msg_pos)) == -1){
                        //error has occurred, re-connect and clear message buffer and position, continue
                    }
                }
                else{
                    if((ret = recv(n->sock, n->msg + n->msg_pos, MSG_SIZE - n->msg_pos, 0)) == -1){
                        //error has occurred, re-connect and clear message buffer and position, continue
                    }
                    else if (ret == 0){
                        //the socket has shutdown, re-connect and clear message buffer and position, continue
                    }
                }
                n->msg_pos += ret;
               
                char buf[MSG_SIZE];
                while((ret = irc_msg_recv(n->name, buf, MSG_SIZE)) != -1){
                    logmsg(LOG_DEBUG, "%.*s", (int)ret, buf);
                    //if any full lines can be read from the buffer, handle numerics and pass them on to plugins
                    if(irc_handle_ping(n->name, buf, (int)ret) == -1){
                        //the socket has shutdown, re-connect and clear message buffer and position, continue
                    }
                }
            }
        }
    }
}
