/**
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <jansson.h>
#include <openssl/opensslv.h>

#include "config.h"
#include "daemonize.h"
#include "htable.h"
#include "inet.h"
#include "log.h"
#include "nexus.h"
#include "plugin.h"
#include "signals.h"

/**
 * Prints command-line application usage information.
 */
void print_usage(){
    printf("\nUsage: praetor [-d][-f] -c config_path\n");
    printf("\nCommand-line options:\n");
    printf("\n\t-c,\tSpecifies praetor's main configuration file path");
    printf("\n\t-d,\tEnables debug mode, increasing logging verbosity");
    printf("\n\t-f,\tEnables foreground mode. praetor will run in the foreground, and log to stdout");
    printf("\n\t-h,\tPrints this help information");
    printf("\n\t-v,\tPrints version information\n\n");
}

int main(int argc, char* argv[]){
    setlogmask(LOG_MASK(LOG_WARNING) | LOG_MASK(LOG_ERR) | LOG_MASK(LOG_CRIT) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_EMERG));
    char* config_path = NULL;
    int opt;
    while((opt = getopt(argc, argv, "c:dfhv")) != -1){
        switch(opt){
            case 'c':
                config_path = optarg;
                break;
            case 'd':
                debug = true;
                setlogmask(LOG_MASK(LOG_DEBUG) | LOG_MASK(LOG_INFO) | LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_WARNING) | LOG_MASK(LOG_ERR) | LOG_MASK(LOG_CRIT) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_EMERG));
                break;
            case 'f':
                foreground = true;
                break;
            case 'h':
                print_usage();
                _exit(0);
            case 'v':
                printf("\npraetor Version: %s\nCommit Hash: %s\nCopyright 2015-2018 David Zero\n\n", PRAETOR_VERSION, COMMIT_HASH);
                printf("This build of praetor has been compiled with support for:\n");
                printf("Jansson Version: %s\n", JANSSON_VERSION);
                printf("LibreSSL Version: %s\n\n", LIBRESSL_VERSION_TEXT);
                _exit(0);
            case '?':
                print_usage();
                _exit(-1);
        }
    }

    if(config_path == NULL){
        logmsg(LOG_ERR, "You must specify a configuration file path\n");
        print_usage();
        _exit(-1);
    }
    logmsg(LOG_DEBUG, "Config file path = %s\n", config_path);

    //allocate space for various configuration
    if((rc_praetor = calloc(1, sizeof(struct praetor))) == NULL){
        logmsg(LOG_ERR, "Could not allocate global data structures, the system is out of memory\n");
        _exit(-1);
    }

    rc_network = htable_create(5);
    rc_network_sock = htable_create(5);
    rc_plugin = htable_create(5);
    rc_plugin_sock = htable_create(5);

    if(rc_network == NULL || rc_network_sock == NULL || rc_plugin == NULL || rc_plugin_sock == NULL){
        logmsg(LOG_ERR, "Could not allocate global data structures, the system is out of memory\n");
        _exit(-1);
    }

    //load configuration
    if(config_load(config_path) == -1){
        _exit(-1);
    }

    //daemonize
    if(!foreground){
        if(daemonize(rc_praetor->workdir, rc_praetor->user, rc_praetor->group) == -1){
            _exit(-1);
        }
    }

    //install signal handlers
    if(signal_init() < 0){
        logmsg(LOG_ERR, "main: Could not install signal handlers\n");
        _exit(-1);
    }

    //load plugins
    if(plugin_load_all() < 0){
        logmsg(LOG_WARNING, "main: Could not load all plugins\n");
    }

    //connect to IRC
    if(inet_connect_all() == -1){
        logmsg(LOG_WARNING, "main: Could not connect to any IRC networks\n");
    }

    //main event loop
    while(true){
        run();
    }
}
