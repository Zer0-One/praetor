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
#include <fcntl.h>
#include <jansson.h>
#include <libgen.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "htable.h"
#include "ircmsg.h"
#include "log.h"
#include "nexus.h"
#include "plugin.h"

int plugin_load(struct plugin* p){
    int fds[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1){
        logmsg(LOG_WARNING, "plugin: Failed to create IPC socket for plugin '%s', %s\n", p->name, strerror(errno));
        return -1;
    }

    pid_t child_pid = fork();
    switch(child_pid){
        case -1:
            close(fds[0]);
            close(fds[1]);
            logmsg(LOG_WARNING, "plugin: Failed to fork plugin '%s', %s\n", p->name, strerror(errno));
            return -1;
        case 0:
            if(dup2(fds[1], 0) == -1 || dup2(fds[1], 1) == -1){
                logmsg(LOG_WARNING, "plugin: Failed to duplicate socket file descriptor to stdin/stdout for plugin '%s'\n", p->name);
                _exit(-1);
            }

            close(fds[0]);
            close(fds[1]);
            closelog();

            char* argv[] = {basename(p->path), NULL};
            char* envp[] = {"PRAETOR_PLUGIN=1", NULL};
            execve(p->path, argv, envp);

            //The following error can never be printed in foreground mode, we've already closed stdin/stdout
            logmsg(LOG_WARNING, "plugin: Failed to exec plugin '%s', %s\n", p->name, strerror(errno));
            _exit(-1);
        default:
            close(fds[1]);

            p->pid = child_pid;
            p->sock = fds[0];

            if(htable_add(rc_plugin_sock, (uint8_t*)&fds[0], sizeof(fds[0]), p) < 0){
                logmsg(LOG_WARNING, "plugin: Failed to map IPC socket to configuration for plugin '%s'\n", p->name);
                goto fail;
            }
            if(watch_add(fds[0], false) == -1){
                logmsg(LOG_WARNING, "plugin: Failed to add plugin socket to global monitor list for plugin '%s'\n", p->name);
                if(htable_remove(rc_plugin_sock, (uint8_t*)&fds[0], sizeof(fds[0])) != 0){
                    //If the index we just added doesn't exist, something's fucky
                    logmsg(LOG_ERR, "plugin: Software failure. Press left mouse button to continue. Guru Meditation #c4fe.b33f.b4b3\n");
                    _exit(-1);
                }
                goto fail;
            }

            //If the socket isn't non-blocking, we can't read from it; a single shitty plugin could hang the bot
            int flags = fcntl(fds[0], F_GETFL);
            if(flags == -1){
                logmsg(LOG_WARNING, "plugin: Failed to get flags for plugin socket for plugin '%s', %s\n", p->name, strerror(errno));
                goto fail;
            }
            if(fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == -1){
                logmsg(LOG_WARNING, "plugin: Failed to put plugin socket into non-blocking mode for plugin '%s', %s\n", p->name, strerror(errno));
                goto fail;
            }

            p->status = PLUGIN_LOADED;
            return fds[0];

            fail:
                close(fds[0]);
                if(kill(p->pid, SIGTERM) < 0){
                    logmsg(LOG_ERR, "plugin: Could not send SIGTERM to failed plugin\n");
                    _exit(-1);
                }
                p->status = PLUGIN_UNLOADED;
                return -1;
    }
}

int plugin_load_all(){
    size_t size = 0;
    struct htable_key** plugins = htable_get_keys(rc_plugin, &size);
    if(plugins == NULL){
        logmsg(LOG_WARNING, "plugin: Failed to load list of configured plugins\n");
        logmsg(LOG_WARNING, "plugin: There are no configured plugins, or the system is out of memory\n");
        return -1;
    }

    int ret = 0;
    for(size_t i = 0; i < size; i++){
        struct plugin* p = htable_lookup(rc_plugin, plugins[i]->key, plugins[i]->key_size);
        int fd = plugin_load(p);
        if(fd < 0){
            ret = -1;
        }
    }

    htable_key_list_free(plugins, size);

    return ret;
}

int plugin_unload(struct plugin* p){
    //If PLUGIN_UNLOADED, there's a logic error in your code.
    if(p->status == PLUGIN_UNLOADED){
        logmsg(LOG_WARNING, "plugin: Attempted to unload already unloaded plugin '%s'\n", p->name);
        return 0;
    }
    //If PLUGIN_LOADED, we need to kill the process.
    if(p->status == PLUGIN_LOADED){
        if(kill(p->pid, SIGTERM) < 0){
            //We couldn't send SIGTERM to the plugin. This should never happen.
            logmsg(LOG_ERR, "plugin: Failed to send SIGTERM to plugin '%s', %s\n", p->name, strerror(errno));
            _exit(-1);
        }
    }
    //If PLUGIN_DEAD, then we came here from the signal handler and we need to run cleanup.

    watch_remove(p->sock);
    if(htable_remove(rc_plugin_sock, (uint8_t*)&p->sock, sizeof(p->sock)) != 0){
        //We had a plugin loaded, but that plugin had no mapped socket connection
        //This should never happen
        logmsg(LOG_ERR, "plugin: Could not unmap socket for plugin '%s', no mapping exists\n", p->name);
        _exit(-1);
    }

    close(p->sock);
    p->sock = -1;
    p->status = PLUGIN_UNLOADED;

    return 0;
}

int plugin_unload_all(){
    size_t size = 0;
    struct htable_key** plugins = htable_get_keys(rc_plugin, &size);
    if(plugins == NULL){
        logmsg(LOG_WARNING, "plugin: Failed to load list of configured plugins\n");
        logmsg(LOG_WARNING, "pluin: There are no configured plugins, or the system is out of memory\n");
        return -1;
    }

    int ret = 0;
    for(size_t i = 0; i < size; i++){
        struct plugin* p = htable_lookup(rc_plugin, plugins[i]->key, plugins[i]->key_size);
        if(plugin_unload(p) < 0){
            ret = -1;
        }
    }
    htable_key_list_free(plugins, size);

    return ret;
}

int plugin_reload(struct plugin* p){
    if(plugin_unload(p) != -1){
        return plugin_load(p);
    }

    return -1;
}

int plugin_reload_all(){
    if(plugin_unload_all() != -1){
        return plugin_load_all();
    }

    return -1;
}

json_t* plugin_recv(struct plugin* p){
    json_error_t error;
    json_t* obj = json_loadfd(p->sock, JSON_DISABLE_EOF_CHECK, &error);
    //This covers malformed messages, and since the sockets are non-blocking, I
    //*think* it covers hanging plugins as well (reads returning EAGAIN/EWOULDBLOCK).
    if(obj == NULL){
        logmsg(LOG_WARNING, "plugin: jansson error %d", json_error_code(&error));
        logmsg(LOG_WARNING, "plugin: %s at Line: %d, Column: %d in message sent by plugin '%s'\n", error.text, error.line, error.column, p->name);
        //logmsg(LOG_WARNING, "plugin: Restarting plugin '%s'\n", p->name);
        //plugin_reload(p);
        logmsg(LOG_WARNING, "plugin: Unloading plugin '%s' due to error\n", p->name);
        plugin_unload(p);
        return NULL;
    }
    
    char* plain = json_dumps(obj, JSON_INDENT(4));
    logmsg(LOG_DEBUG, "plugin: Received message from plugin '%s':\n%s\n", p->name, plain);
    free(plain);

    return obj;
}

int plugin_send(struct plugin* p, struct ircmsg* msg){
    json_t* obj = ircmsg_to_json(msg);
    if(obj == NULL){
        return -2;
    }

    char* plain = json_dumps(obj, JSON_INDENT(4));
    logmsg(LOG_DEBUG, "plugin: Sending message to plugin '%s':\n%s\n", p->name, plain);
    free(plain);

    int ret = json_dumpfd(obj, p->sock, JSON_COMPACT);
    if(ret == -1){
        logmsg(LOG_WARNING, "plugin: Unable to send message to plugin '%s'\n", p->name);
        //logmsg(LOG_WARNING, "plugin: Restarting plugin '%s'\n", p->name);
        //plugin_reload(p);
        logmsg(LOG_WARNING, "plugin: Unloading plugin '%s' due to error\n", p->name);
        plugin_unload(p);
    }

    return 0;
}
