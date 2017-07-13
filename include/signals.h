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

#ifndef PRAETOR_SIGNAL
#define PRAETOR_SIGNAL

#include <signal.h>

extern volatile sig_atomic_t sigchld;
extern volatile sig_atomic_t sighup;
extern volatile sig_atomic_t sigpipe;
extern volatile sig_atomic_t sigterm;

/**
 * Sets signal disposition and installs handlers for:
 *     - SIGCHLD
 *     - SIGHUP
 *     - SIGPIPE
 *     - SIGTERM
 *
 * \return 0 on success.
 * \return -1 on error.
 */
int signal_init();

int sigchld_handler();
int sighup_handler();
int sigpipe_handler();
int sigterm_handler();

#endif
