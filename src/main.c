/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015, David Zero
* All rights reserved.
*
* The following code is licensed for use, modification, and redistribution
* according to the terms of the Revised BSD License. The text of this license
* can be found in the "LICENSE" file bundled with this source distribution.
*/

#include <errno.h>
#include <fcntl.h>
#include <jansson.h>
#include <openssl/opensslv.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "daemonize.h"
#include "log.h"
#include "util/hashtable.h"
//#include "sighandlers.h"

/**
 * Prints command-line application usage information.
 */
void print_usage(){
    printf("\nUsage: praetor [-d][-f] -c config_path\n");
    printf("\nCommand-line options:\n");
    printf("\n\t-c,\tSpecifies praetor's main configuration file path");
    printf("\n\t-d,\tEnables debug mode, increasing logging verbosity");
    printf("\n\t-f,\tEnables foreground mode. praetor will log to stdout");
    printf("\n\t-h,\tPrints this help information");
    printf("\n\t-v,\tPrints version information\n\n");
}

/**
 * Application entry-point.
 *
 * The daemon is initialized, and control is handed to the main loop. There are
 * a few steps to initialization:
 * 1. Command-line arguments are parsed
 * 2. The logging verbosity is set (either normal or debug)
 * 3. The main configuration file is loaded
 * 4. The application daemonizes
 * 5. Signal handlers are installed
 */
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
                printf("\npraetor Version: %s\nCommit Hash: %s\nCopyright 2015 David Zero\n\n", PRAETOR_VERSION, COMMIT_HASH);
                printf("This build of praetor has been compiled with support for:\n");
                printf("Jansson Version: %s\n", JANSSON_VERSION);
                printf("OpenSSL Version: %s\n\n", OPENSSL_VERSION_TEXT);
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
    struct praetorinfo rc_praetor;
    struct networkinfo rc_network = {.next = NULL};
    loadconfig(config_path, &rc_praetor, &rc_network);
    logmsg(LOG_DEBUG, "nick: %s\nalt_nick:%s\n", rc_network.nick, rc_network.alt_nick);

    while(true){
        struct htable* test_table = htable_create(20);
        htable_add(test_table, "blah", 5, "thisisateststring");
        logmsg(LOG_DEBUG, "The lookup gave us: %s\n", (char*)htable_lookup(test_table, "blah", 5));
        sleep(1);
    }
}
