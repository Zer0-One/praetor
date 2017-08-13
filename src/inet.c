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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <tls.h>

#include "config.h"
#include "hashtable.h"
#include "irc.h"
#include "log.h"
#include "nexus.h"

#define DEFAULT_PORT "6667"

int inet_getaddrinfo(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "inet: No configuration found for network '%s'\n", network);
        return -1;
    }

    const char* host = n->host, *service = DEFAULT_PORT;
    char* tmp = NULL;
    if(strstr(n->host, ":") != NULL){
        //strtok modifies its argument, so make a copy
        if((tmp = calloc(1, strlen(n->host)+1)) == NULL){
            logmsg(LOG_WARNING, "inet: Could not parse host:port pair for network '%s', the system is out of memory\n", network);
            return -1;
        }
        
        strcpy(tmp, n->host);
        host = strtok(tmp, ":");
        service = strtok(NULL, ":");
    }

    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    
    hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    int s = getaddrinfo(host, service, &hints, &result);

    if(s < 0 || result == NULL){
        if(s == EAI_SYSTEM){
            logmsg(LOG_WARNING, "inet: Could not get address info for host '%s', %s\n", host, strerror(errno));
        }
        else{
            logmsg(LOG_WARNING, "inet: Could not get address info for host '%s', %s\n", host, gai_strerror(s));
        }
        free(tmp);
        return -1;
    }

    free(tmp);
    n->addr = result;
    n->addr_idx = 0;
    return 0;
}

int inet_tls_upgrade(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "inet: No configuration found for network '%s'\n", network);
        return -1;
    }

    if(!n->ssl){
        logmsg(LOG_DEBUG, "inet: Not establishing TLS connection to network '%s'\n", network);
        return 0;
    }

    const char* host = n->host;
    char* tmp = NULL;
    if(strstr(n->host, ":") != NULL){
        //strtok modifies its argument, so make a copy
        if((tmp = calloc(1, strlen(n->host)+1)) == NULL){
            logmsg(LOG_WARNING, "inet: Could not parse host:port pair for network '%s', the system is out of memory\n", network);
            return -1;
        }
        strcpy(tmp, n->host);
        host = strtok(tmp, ":");
    }

    tls_init();
    struct tls_config* cfg;
    struct tls* ctx;
    if((ctx = tls_client()) == NULL){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to '%s' host '%s', system out of memory\n", network, host);
        close(n->sock);
        free(tmp);
        return -1;
    }
    if((cfg = tls_config_new()) == NULL){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to '%s' host '%s', system out of memory\n", network, host);
        goto fail;
    }
    if(!tls_configure(ctx, cfg)){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to '%s' host '%s', %s\n", network, host, tls_error(ctx));
        tls_config_free(cfg);
        goto fail;
    }
    tls_config_free(cfg);
    
    if(!tls_connect_socket(ctx, n->sock, host)){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to '%s' host %s, %s\n", network, host, tls_error(ctx));
        goto fail;
    }

    logmsg(LOG_DEBUG, "inet: Established TLS connection to '%s' host '%s'\n", network, host);
    free(tmp);
    n->ctx = ctx;
    return 0;

    fail:
        free(tmp);
        tls_free(ctx);
        close(n->sock);
        return -1;
}

int inet_connect(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "inet: No configuration found for network '%s'\n", network);
        return -1;
    }

    struct addrinfo* addr = NULL;
    //If we haven't done DNS lookups yet, do them
    if(n->addr == 0){
        if(inet_getaddrinfo(network) == -1){
            return -2;
        }
        addr = n->addr;
    }
    //If we have done DNS lookups already, find the next address we should try
    else{
        addr = n->addr;
        //Update addr to the one indexed by n->addr_idx
        for(size_t i = 0; i < n->addr_idx; i++){
            addr = n->addr->ai_next;
            if(addr == NULL){
                logmsg(LOG_WARNING, "inet: All usable addresses available for network '%s' have been exhausted, aborting connection\n", network);
                freeaddrinfo(n->addr);
                n->addr = NULL;
                return -2;
            }
        }
    }

    //Obtain address in presentation form for debugging and logging purposes
    char* host = NULL;
    if(addr->ai_family == AF_INET6){
        host = malloc(INET6_ADDRSTRLEN);
        inet_ntop(addr->ai_family, &((struct sockaddr_in6*)addr->ai_addr)->sin6_addr, host, INET6_ADDRSTRLEN);
    }
    else{
        host = malloc(INET_ADDRSTRLEN);
        inet_ntop(addr->ai_family, &((struct sockaddr_in*)(addr->ai_addr))->sin_addr, host, INET_ADDRSTRLEN);
    }
    if(host == NULL){
        //This should never happen
        logmsg(LOG_ERR, "inet: Could not convert address for network '%s' into presentation form, %s\n", network, strerror(errno));
        _exit(-1);
    }

    //Create socket file descriptor, add it to the network config
    int sock = 0;
    sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if(sock == -1){
        logmsg(LOG_DEBUG, "inet: Could not open socket for '%s' host '%s', %s\n", network, host, strerror(errno));
        return -1;
    }
    n->sock = sock;

    //Put the socket fd into non-blocking mode
    int flags = fcntl(sock, F_GETFL);
    if(flags == -1){
        logmsg(LOG_DEBUG, "inet: Could not get socket file descriptor flags for '%s' host '%s', %s\n", network, host, strerror(errno));
        close(sock);
        return -1;
    }
    if(fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1){
        logmsg(LOG_DEBUG, "inet: Could not set socket file descriptor flags for '%s' host '%s', %s\n", network, host, strerror(errno));
        close(sock);
        return -1;
    }

    //Create a message buffer for the network, if one doesn't already exist
    if(n->msgbuf == 0){
        //+1 to account for a null-terminator; the buffer can then be read like any string
        if((n->msgbuf = calloc(MSG_SIZE_MAX + 1, sizeof(char))) == NULL){
            logmsg(LOG_WARNING, "inet: Could not allocate message buffer for network '%s', the system is out of memory\n", network);
            close(sock);
            return -1;
        }
        n->msgbuf_idx = 0;
        n->msgbuf_size = MSG_SIZE_MAX + 1;
    }

    int rval = 0;
    //Initiate connection
    if(connect(sock, addr->ai_addr, addr->ai_addrlen) == -1){
        if(errno == EINPROGRESS){
            logmsg(LOG_DEBUG, "inet: Connection to '%s' host '%s' initiated\n", network, host);
            watch_add(sock, true);
            rval = 1;
            //return 1;
        }
        else{
            logmsg(LOG_WARNING, "inet: Could not connect to '%s' host '%s', %s\n", network, host, strerror(errno));
            //connection failed, clean up and use the next address next time
            close(sock);
            n->addr_idx++;
            return -1;
        }
    }
    else{
        logmsg(LOG_DEBUG, "inet: Connection to '%s' host '%s' completed immediately\n", network, host);
        if(inet_tls_upgrade(n->name) == -1){
            close(sock);
            n->addr_idx++;
            return -1;
        }
        watch_add(sock, false);
    }
    
    //Map the socket to the network config struct
    int ret = htable_add(rc_network_sock, &n->sock, sizeof(n->sock), n);
    if(ret == -1){
        logmsg(LOG_WARNING, "inet: Could not map socket for '%s' host '%s', the system is out of memory\n", network, host);
        close(sock);
        watch_remove(sock);
        return -1;
    }
    else if(ret == -2){
        //mapping already exists, this should never happen
        logmsg(LOG_ERR, "inet: Could not map socket for '%s' host '%s', mapping already exists\n", network, host);
        close(sock);
        _exit(-1);
    }

    free(host);

    return rval;
}

int inet_connect_all(){
    struct list* networks = htable_get_keys(rc_network, false);
    if(networks == NULL){
        logmsg(LOG_WARNING, "inet: Failed to load list of configured networks\n");
        logmsg(LOG_WARNING, "inet: There are no configured networks or the system is out of memory\n");
        return -1;
    }

    for(struct list* this = networks; this != 0; this = this->next){
        inet_connect(this->key);
    }

    htable_key_list_free(networks, false);

    return 0;
}

int inet_check_connection(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "inet: No configuration found for network '%s'\n", network);
        return -1;
    }

    int optval = INT_MAX;
    socklen_t optlen = sizeof(optval);
    if(getsockopt(n->sock, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1){
        //This is never supposed to fail
        logmsg(LOG_ERR, "nexus: Unable to check socket connection for errors\n");
        _exit(-1);
    }

    if(optval == 0){
        logmsg(LOG_DEBUG, "nexus: Connection to network '%s' was successful\n", network);
        watch_remove(n->sock);

        if(inet_tls_upgrade(network) == -1){
            close(n->sock);
            n->addr_idx++;
        }

        watch_add(n->sock, false);
		return 0;
    }
    else if(optval == INT_MAX){
        //This is never supposed to happen
        logmsg(LOG_ERR, "nexus: Unable to check socket connection for errors\n");
        _exit(-1);
    }
	
	logmsg(LOG_WARNING, "nexus: Connection to network '%s' was unsuccessful\n", network);
	watch_remove(n->sock);
	n->addr_idx++;
	inet_connect(network);
	return -1;
}

int inet_disconnect(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "inet: No configuration found for network '%s'\n", network);
        return -1;
    }

    close(n->sock);
    watch_remove(n->sock);
    if(htable_remove(rc_network_sock, &n->sock, sizeof(n->sock)) == -1){
        logmsg(LOG_WARNING, "inet: Could not remove socket mapping for network '%s', no such mapping exists\n", network);
        return -1;
    }

    memset(n->msgbuf, '\0', n->msgbuf_size);
    n->msgbuf_idx = 0;

    return 0;
}

int inet_recv(const char* network){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "inet: No configuration found for network '%s'\n", network);
        return -1;
    }
    
    //Subtract one to account for the null-terminator
    size_t bytes_to_read = (n->msgbuf_size - 1) - n->msgbuf_idx;
    if(bytes_to_read == 0){
        logmsg(LOG_DEBUG, "inet: Message buffer for network '%s' is full\n", network);
        return 0;
    }

    ssize_t ret;
    if(n->ssl){
        ret = tls_read(n->ctx, n->msgbuf + n->msgbuf_idx, bytes_to_read);
        if(ret == -1){
            logmsg(LOG_WARNING, "inet: Could not read from network '%s' via TLS connection, %s\n", network, tls_error(n->ctx));
            //I do this because idk how to get more specific info from libtls
            //This needs to be corrected eventually
            goto reconn;
        }
    }
    else{
        ret = recv(n->sock, n->msgbuf + n->msgbuf_idx, bytes_to_read, 0);
    }
    
    if(ret == -1){
        switch(errno){
#if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
#endif
            case EAGAIN:
                logmsg(LOG_DEBUG, "inet: Read from network '%s' would block\n", network);
                return 0;
            case ECONNRESET:
            case ENOTCONN:
            case ETIMEDOUT:
                logmsg(LOG_WARNING, "inet: Lost connection to network '%s', %s", network, strerror(errno));
                goto reconn;
            case ENOBUFS:
            case ENOMEM:
                logmsg(LOG_WARNING, "inet: Could not read from network '%s', %s\n", network, strerror(errno));
                return -1;
            default:
                logmsg(LOG_ERR, "inet: Unable to read from network '%s', %s\n", network, strerror(errno));
                _exit(-1);
        }
    }
    else if(ret == 0){
        goto reconn;
    }

    n->msgbuf_idx += ret;
    return 0;

    reconn:
        inet_disconnect(network);
        inet_connect(network);
        return -1;
}

int inet_send(const char* network, const char* buf, size_t len){
    struct network* n;
    if((n = htable_lookup(rc_network, network, strlen(network)+1)) == NULL){
        logmsg(LOG_WARNING, "inet: No configuration found for network '%s'\n", network);
        return -1;
    }

    ssize_t ret;
    if(n->ssl){
        ret = tls_write(n->ctx, buf, len);
        if(ret == -1){
            logmsg(LOG_WARNING, "inet: Could not send to network '%s' via TLS connection, %s\n", network, tls_error(n->ctx));
            //I do this because idk how to get more specific info from libtls
            //This needs to be corrected eventually
            goto reconn;            
        }
        return 0;
    }

    ret = send(n->sock, buf, len, MSG_NOSIGNAL);
    if(ret == -1){
        switch(errno){
#if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
#endif
            case EAGAIN:
                logmsg(LOG_WARNING, "inet: Send to network '%s' would block, discarding message\n", network);
            case ENOBUFS:
                return -1;
            case ECONNRESET:
            case EPIPE:
            //We might have an address that can be reached from another interface
            case ENETDOWN:
            case ENETUNREACH:
                goto reconn;
            default:
                logmsg(LOG_ERR, "inet: Unable to send to network '%s', %s\n", network, strerror(errno));
                _exit(-1);
        }
    }

    return 0;

    reconn:
        logmsg(LOG_WARNING, "inet: Lost connection to network '%s'\n", network);
        inet_disconnect(network);
        inet_connect(network);
        return -1;
}
