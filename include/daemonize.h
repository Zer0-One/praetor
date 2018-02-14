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

#ifndef PRAETOR_DAEMONIZE
#define PRAETOR_DAEMONIZE

/**
 * Turns the calling process into a daemon.
 *
 * \param workdir The directory that will be used as the daemon's working directory
 * \param user    The user with whose privileges the daemon will run
 * \param group   The group with whose privileges the daemon will run
 *
 * \return 0 on success.
 * \return -1 on error.
 */
int daemonize(const char* workdir, const char* user, const char* group);

#endif
