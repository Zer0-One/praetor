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
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "hashtable.h"
#include "inet.h"
#include "irc.h"
#include "log.h"
#include "plugin.h"

/**
 * This flag is set by signal_handle_sigchld() when praetor receives a SIGCHLD.
 */
volatile sig_atomic_t sigchld = 0;
/**
 * This flag is set by signal_handle_sighup() when praetor receives a SIGHUP.
 */
volatile sig_atomic_t sighup = 0;
/**
 * This flag is set by signal_handle_sigpipe() when praetor receives a SIGPIPE.
 */
volatile sig_atomic_t sigpipe = 0;
/**
 * This flag is set by signal_handle_sigterm() when praetor receives a SIGTERM.
 */
volatile sig_atomic_t sigterm = 0;

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

int sigchld_handler(){
    struct list* plugins = htable_get_keys(rc_plugin, false);
    if(plugins == NULL){
        logmsg(LOG_WARNING, "nexus: Failed to load list of configured plugins\n");
        logmsg(LOG_WARNING, "nexus: There are no configured plugins, or the system is out of memory\n");
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
        logmsg(LOG_ERR, "plugin: Software failure. Press left mouse button to continue. Guru Meditation #b33f.b33f.b33f\n");
        exit(-1);

        success:

        if(WIFSIGNALED(wstatus)){
            //if we move to SUSv4, use strsignal() here
            logmsg(LOG_WARNING, "nexus: Plugin '%s' terminated due to unhandled signal: %d\n", p->name, WTERMSIG(wstatus));
        }
        else{
            logmsg(LOG_WARNING, "nexus: Plugin '%s' exited with status: %d\n", p->name, WEXITSTATUS(wstatus));
        }

        watch_remove(p->sock);
        htable_remove(rc_plugin_sock, &p->sock, sizeof(&p->sock));
        close(p->sock);
        p->pid = 0;
        p->sock = 0;
    }

    htable_key_list_free(plugins, false);
    return 0;
}

void run(){
    if(monitor_list_count < 1){
        logmsg(LOG_ERR, "nexus: No sockets to monitor, exiting\n");
        return;
    }
    
    sigset_t mask_set;
    sigemptyset(&mask_set);

    while(true){
        if(sigchld){
            /*
             * If we receive SIGCHLD during handler execution, we might set
             * sigchld = 0 without cleaning up a newly-terminated child.
             * Setting the signal mask like this guards against that
             * possibility. If the handler does clean up the newly-terminated
             * child before the next time it's called, it'll safely return 0
             * the next time it's called.
             */
            sigaddset(&mask_set, SIGCHLD);
            sigprocmask(SIG_BLOCK, &mask_set, NULL);
            
            //Spin until we can acquire enough memory to run the handler completely
            if(sigchld_handler() == -1){
                //TO-DO: make this less aggressive. Maybe sleep for a bit?
                continue;
            }

            sigchld = 0;
            sigprocmask(SIG_UNBLOCK, &mask_set, NULL);
            sigemptyset(&mask_set);
        }
        else if(sighup){
            //handle re-initialization of the daemon
            sighup = 0;
        }
        else if(sigpipe){
            //handle remote end of a socket hanging up unexpectedly (for both plugins and networks)
    		//    - clean up (kill the plugin process) and attempt to re-connect (re-start the plugin)
    		//    - if reconnect fails, give up (admin can attempt to initiate another connection via IRC)
            sigpipe = 0;
        }
        else if(sigterm){
            irc_disconnect_all();
            _exit(0);
        }

        if(poll(monitor_list, monitor_list_count, -1) < 1){
            //poll will never return 0, because we never time out
            //check to see if we were interrupted by a signal, or if we ran out of memory
        }
        for(size_t i = 0; i < monitor_list_count; i++){
            if(monitor_list[i].revents & POLLIN){// && monitor_list[i].revents & POLLOUT){
                struct plugin* p = htable_lookup(rc_plugin_sock, &monitor_list[i].fd, sizeof(monitor_list[i].fd));
                struct network* n = htable_lookup(rc_network_sock, &monitor_list[i].fd, sizeof(monitor_list[i].fd));
                //process network input
                if(n){
                    ssize_t ret;
                    //copy bytes from the current position in the buffer until the end, or until we run out of bytes to copy
                    if(n->ssl){
                        if((ret = tls_read(n->ctx, n->msg + n->msg_pos, MSG_SIZE_MAX - n->msg_pos)) == -1){
                            //error has occurred, re-connect and clear message buffer and position, continue
                        }
                    }
                    else{
                        if((ret = recv(n->sock, n->msg + n->msg_pos, MSG_SIZE_MAX - n->msg_pos, 0)) == -1){
                            //error has occurred, re-connect and clear message buffer and position, continue
                        }
                        else if (ret == 0){
                            //the socket has shutdown, re-connect and clear message buffer and position, continue
                        }
                    }
                    n->msg_pos += ret;
                   
                    char buf[MSG_SIZE_MAX];
                    while((ret = irc_msg_recv(n->name, buf, MSG_SIZE_MAX)) != -1){
                        logmsg(LOG_DEBUG, "%.*s", (int)ret, buf);
                        //if any full lines can be read from the buffer, handle numerics and pass them on to plugins
                        //plugin_message_to_json(the_message)
                        if(irc_handle_ping(n->name, buf, (int)ret) == -1){
                            //the socket has shutdown, re-connect and clear message buffer and position, continue
                        }
                    }
                }
                //process plugin input
                else if(p){
                    //parse the JSON document, craft an IRC message, and send according to permissions and rate limit
                    //plugin_message_from_json(the_message)
                }
            }
        }
    }
}
