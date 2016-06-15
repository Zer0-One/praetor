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
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "log.h"

/**
 * An upper limit for the number of file descriptors to close.
 * This field will only be relevant in the case that sysconf() returns an
 * indeterminate result for the system upper limit on the number of file
 * descriptors a process can have.
 */
const int FD_ULIMIT = 9001;

void daemonize(const char* workdir, const char* user, const char* group){
    //fetch user and group info
	errno = 0;
	struct passwd* usr = getpwnam(user);
	if(usr == NULL){
		logmsg(LOG_ERR, "Failed to get uid for user: '%s'. %s", user, strerror(errno));
		_exit(-1);
	}
	errno = 0;
	struct group* grp = getgrnam(group);
	if(grp == NULL){
		logmsg(LOG_ERR, "Failed to get gid for group: '%s'. %s", group, strerror(errno));
		_exit(-1);
	}
    //permanently drop privs
    if(setgid(grp->gr_gid) == -1){
        logmsg(LOG_ERR, "Failed to set group to: '%s'. %s", group, strerror(errno));
        _exit(-1);
    }
	if(setuid(usr->pw_uid) == -1){
		logmsg(LOG_ERR, "Failed to set user to: '%s'. %s", user, strerror(errno));
		_exit(-1);
	}

	switch(fork()){
		case -1:
			logmsg(LOG_ERR, "Failed to fork. %s", strerror(errno));
			_exit(-1);
		case 0:
			break;
		default:
			_exit(0);
	}

	if(setsid() == -1){
		logmsg(LOG_ERR, "Failed to start new session. %s", strerror(errno));
		_exit(-1);
	}

	//fork again so we're not a session leader
	switch(fork()){
		case -1:
			logmsg(LOG_ERR, "Failed to fork. %s", strerror(errno));
			_exit(-1);
		case 0:
			break;
		default:
			_exit(0);
	}

	umask(S_IWGRP | S_IWOTH);

	if(chdir(workdir) == -1){
		logmsg(LOG_ERR, "Failed to change directory to %s. %s", workdir, strerror(errno));
		_exit(-1);
	}

	int fd_ulimit = sysconf(_SC_OPEN_MAX);
	if(fd_ulimit == -1){
		logmsg(LOG_INFO, "Could not determine upper limit on process file descriptors, using value: %i", FD_ULIMIT);
		fd_ulimit = FD_ULIMIT;
	}
	closelog();
	for(int i = 0; i < fd_ulimit; i++){
		close(i);
	}
	if(open("/dev/null", O_RDWR) != 0){
		logmsg(LOG_ERR, "Failed to open /dev/null. %s", strerror(errno));
		_exit(-1);
	}
	if(dup2(0, 1) != 1){
		logmsg(LOG_ERR, "Failed to copy file descriptor 0 (/dev/null) to 1. %s", strerror(errno));
		_exit(-1);
	}
	if(dup2(0, 2) != 2){
		logmsg(LOG_ERR, "Failed to copy file descriptor 1 (/dev/null) to 2. %s", strerror(errno));
		_exit(-1);
	}
}
