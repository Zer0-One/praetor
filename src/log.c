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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>

#define DATE_BUFFER_LENGTH 100

bool debug = false;
bool foreground = false;

void logprintf(int loglevel, char* msg, ...){
    va_list args;
    va_start(args, msg);

    struct timeval tv;
    char date_buffer[DATE_BUFFER_LENGTH];

    gettimeofday(&tv, NULL);
    struct tm* bd_time = localtime(&tv.tv_sec);
    strftime(date_buffer, DATE_BUFFER_LENGTH, "[%a %b %d %T %Y] ", bd_time);
    printf(date_buffer);

    if(loglevel == LOG_ALERT){
        printf("Alert: ");
    }
    else if(loglevel == LOG_CRIT){
        printf("Critical: ");
    }
    else if(loglevel == LOG_DEBUG){
        printf("Debug: ");
    }
    else if(loglevel == LOG_EMERG){
        printf("Alert: ");
    }
    else if(loglevel == LOG_ERR){
        printf("Error: ");
    }
    else if(loglevel == LOG_INFO){
        printf("Info: ");
    }
    else if(loglevel == LOG_NOTICE){
        printf("Notice: ");
    }
    else if(loglevel == LOG_WARNING){
        printf("Warning: ");
    }
    vprintf(msg, args);

    va_end(args);
}
