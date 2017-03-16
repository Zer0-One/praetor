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
#include <libgen.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "hashtable.h"
#include "log.h"
#include "nexus.h"

int plugin_load(const char* plugin){
    struct plugin* p;
    if((p = htable_lookup(rc_plugin, plugin, strlen(plugin)+1)) == NULL){
        logmsg(LOG_WARNING, "plugin: No configuration found for plugin '%s'\n", plugin);
        return -1;
    }

    int fds[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1){
        logmsg(LOG_WARNING, "plugin: Failed to create IPC socket for plugin '%s', %s\n", plugin, strerror(errno));
        return -1;
    }

    pid_t child_pid = fork();
    switch(child_pid){
        case -1:
            close(fds[0]);
            close(fds[1]);
            logmsg(LOG_WARNING, "plugin: Failed to fork plugin '%s', %s\n", plugin, strerror(errno));
            return -1;
        case 0:
            if(dup2(fds[1], 0) == -1 || dup2(fds[1], 1) == -1){
                logmsg(LOG_WARNING, "plugin: Failed to duplicate socket file descriptor to stdin/stdout for plugin '%s'\n", plugin);
                _exit(-1);
            }
            
            close(fds[0]);
            close(fds[1]);
            closelog();
            
            char* argv[] = {basename(p->path), NULL};
            char* envp[] = {"PRAETOR_PLUGIN=1", NULL};
            execve(p->path, argv, envp);

            //the following error can never be printed in foreground mode, we've already closed stdin/stdout
            logmsg(LOG_WARNING, "plugin: Failed to exec plugin '%s', %s\n", plugin, strerror(errno));
            _exit(-1);
        default:
            close(fds[1]);

            p->pid = child_pid;
            p->sock = fds[0];
            
            if(htable_add(rc_plugin_sock, &fds[0], sizeof(fds[0]), p) < 0){
                logmsg(LOG_WARNING, "plugin: Failed to map IPC socket to configuration for plugin '%s'\n", plugin);
                close(fds[0]);
                kill(p->pid, SIGTERM);
                return -1;
            }
            if(watch_add(fds[0]) < 0){
                logmsg(LOG_WARNING, "plugin: Failed to add plugin socket descriptor to watch-list for plugin '%s'\n", plugin);
                if(htable_remove(rc_plugin_sock, &fds[0], sizeof(fds[0])) != 0){
                    //If the index we just added doesn't exist, something's fucky
                    logmsg(LOG_ERR, "plugin: Software failure. Press left mouse button to continue. Guru Meditation #c4fe.b33f.b4b3\n");
                    _exit(-1);
                }
                kill(p->pid, SIGTERM);
                return -1;
            }

            return fds[0];
    }
}

int plugin_load_all(){
    struct list* plugins = htable_get_keys(rc_plugin, false);
    if(plugins == NULL){
        logmsg(LOG_WARNING, "plugin: Failed to load list of configured plugins\n");
        logmsg(LOG_WARNING, "plugin: There are no configured plugins, or the system is out of memory\n");
        return -1;
    }

    int ret = 0;
    for(struct list* this = plugins; this != 0; this = this->next){
        struct plugin* p = htable_lookup(rc_plugin, this->key, this->size);
        int fd = plugin_load(p->name);
        if(fd < 0){
            ret = -1;
        }
    }
    htable_key_list_free(plugins, false);

    return ret;
}

int plugin_unload(const char* plugin){
    struct plugin* p = htable_lookup(rc_plugin, plugin, strlen(plugin)+1);

    watch_remove(p->sock);
    if(htable_remove(rc_plugin_sock, &p->sock, sizeof(p->sock)) != 0){
        logmsg(LOG_WARNING, "plugin: Could not unload plugin '%s', no such plugin loaded", plugin);
        return -1;
    }

    if(kill(p->pid, SIGTERM) < 0){
        logmsg(LOG_WARNING, "plugin: Failed to send SIGTERM to plugin '%s', %s", plugin, strerror(errno));
        return -1;
    }
    return 0;
}

int plugin_unload_all(){
    struct list* plugins = htable_get_keys(rc_plugin, false);
    if(plugins == NULL){
        logmsg(LOG_WARNING, "plugin: Failed to load list of configured plugins\n");
        logmsg(LOG_WARNING, "pluin: There are no configured plugins, or the system is out of memory\n");
        return -1;
    }

    int ret = 0;
    for(struct list* this = plugins; this != 0; this = this->next){
        struct plugin* p = htable_lookup(rc_plugin, this->key, this->size);
        int stat = plugin_unload(p->name);
        if(stat < 0){
            ret = -1;
        }
    }
    htable_key_list_free(plugins, false);

    return ret;
}
