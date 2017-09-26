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
#include "htable.h"
#include "irc.h"
#include "log.h"
#include "nexus.h"
#include "queue.h"

#define DEFAULT_PORT "6667"
#define DEFAULT_PORT_TLS "6697"

int inet_getaddrinfo(struct network* n){
    const char* host = n->host, *service;
    if(n->ssl){
        service = DEFAULT_PORT_TLS;
    }
    else{
        service = DEFAULT_PORT;
    }

    char* tmp = NULL;
    if(strstr(n->host, ":") != NULL){
        //strtok modifies its argument, so make a copy
        if((tmp = calloc(1, strlen(n->host)+1)) == NULL){
            logmsg(LOG_WARNING, "inet: Could not parse host:port pair for network '%s', the system is out of memory\n", n->name);
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

int inet_tls_upgrade(struct network* n){
    if(!n->ssl){
        logmsg(LOG_DEBUG, "inet: Not establishing TLS connection to network '%s'\n", n->name);
        return 0;
    }

    const char* host = n->host;
    char* tmp = NULL;
    if(strstr(n->host, ":") != NULL){
        //strtok modifies its argument, so make a copy
        if((tmp = calloc(1, strlen(n->host)+1)) == NULL){
            logmsg(LOG_WARNING, "inet: Could not parse host:port pair for network '%s', the system is out of memory\n", n->name);
            return -1;
        }
        strcpy(tmp, n->host);
        host = strtok(tmp, ":");
    }

    if(tls_init() == -1){
        logmsg(LOG_WARNING, "inet: Could not initialize TLS for connection to '%s' host '%s'\n", n->name, host);
        free(tmp);
        return -1;
    }

    struct tls_config* cfg;
    struct tls* ctx;
    if((ctx = tls_client()) == NULL){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to '%s' host '%s', system out of memory\n", n->name, host);
        free(tmp);
        return -1;
    }

    if((cfg = tls_config_new()) == NULL){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to '%s' host '%s', %s\n", n->name, host, tls_config_error(cfg));
        goto fail;
    }

    //uint32_t protocols;
    //tls_config_parse_protocols(&protocols, "secure");
    //tls_config_set_protocols(cfg, protocols);

    if(tls_configure(ctx, cfg) == -1){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to '%s' host '%s', %s\n", n->name, host, tls_config_error(cfg));
        tls_config_free(cfg);
        goto fail;
    }
    tls_config_free(cfg);
    
    if(tls_connect_socket(ctx, n->sock, host) == -1){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to '%s' host %s, %s\n", n->name, host, tls_error(ctx));
        goto fail;
    }

    if(tls_handshake(ctx) == -1){
        logmsg(LOG_WARNING, "inet: Could not perform TLS handshake with '%s' host '%s', %s\n", n->name, host, tls_error(ctx));
        goto fail;
    }

    logmsg(LOG_DEBUG, "inet: Established TLS connection to '%s' host '%s'\n", n->name, host);
    free(tmp);
    n->ctx = ctx;
    return 0;

    fail:
        free(tmp);
        tls_free(ctx);
        return -1;
}

int inet_connect(struct network* n){
    struct addrinfo* addr = NULL;
    //If we've exhausted all addresses, fail
    if(n->addr_idx == INT_MAX){
        logmsg(LOG_WARNING, "inet: All usable addresses available for network '%s' have been exhausted, aborting connection\n", n->name);
        return -2;
    }
    //If we haven't done DNS lookups yet, do them
    else if(n->addr == 0){
        if(inet_getaddrinfo(n) == -1){
            return -2;
        }
        addr = n->addr;
    }
    //If we have done DNS lookups already, find the next address we should try
    else{
        addr = n->addr;
        //Update addr to the one indexed by n->addr_idx
        for(size_t i = 0; i < n->addr_idx; i++){
            addr = addr->ai_next;
            if(addr == NULL){
                logmsg(LOG_WARNING, "inet: All usable addresses available for network '%s' have been exhausted, aborting connection\n", n->name);
                freeaddrinfo(n->addr);
                n->addr = 0;
                n->addr_idx = INT_MAX;
                return -2;
            }
        }
    }

    //Obtain address in presentation form for debugging and logging purposes
    char* host = NULL;
    const char* tmp = NULL;
    if(addr->ai_family == AF_INET6){
        host = malloc(INET6_ADDRSTRLEN);
        if(host == NULL){
            logmsg(LOG_ERR, "inet: Could not convert address for network '%s' into presentation form, the system is out of memory\n", n->name);
            return -1;
        }

        tmp = inet_ntop(addr->ai_family, &((struct sockaddr_in6*)addr->ai_addr)->sin6_addr, host, INET6_ADDRSTRLEN);
    }
    else{
        host = malloc(INET_ADDRSTRLEN);
        if(host == NULL){
            logmsg(LOG_ERR, "inet: Could not convert address for network '%s' into presentation form, the system is out of memory\n", n->name);
            return -1;
        }

        tmp = inet_ntop(addr->ai_family, &((struct sockaddr_in*)(addr->ai_addr))->sin_addr, host, INET_ADDRSTRLEN);
    }
    if(tmp == NULL){
        //This should never happen
        logmsg(LOG_ERR, "inet: Could not convert address for network '%s' into presentation form, %s\n", n->name, strerror(errno));
        _exit(-1);
    }

    //Create socket file descriptor, add it to the network config
    int sock = 0;
    sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if(sock == -1){
        logmsg(LOG_DEBUG, "inet: Could not open socket for '%s' host '%s', %s\n", n->name, host, strerror(errno));
        free(host);
        return -1;
    }
    n->sock = sock;

    //Put the socket fd into non-blocking mode
    int flags = fcntl(sock, F_GETFL);
    if(flags == -1){
        logmsg(LOG_DEBUG, "inet: Could not get socket file descriptor flags for '%s' host '%s', %s\n", n->name, host, strerror(errno));
        goto fail;
    }
    if(fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1){
        logmsg(LOG_DEBUG, "inet: Could not set socket file descriptor flags for '%s' host '%s', %s\n", n->name, host, strerror(errno));
        goto fail;
    }

    //Create a receive queue for the network, if one doesn't already exist
    if(n->recv_queue == 0){
        //+1 to account for a null-terminator; the buffer can then be read like any string
        if((n->recv_queue = calloc(MSG_SIZE_MAX + 1, sizeof(char))) == NULL){
            logmsg(LOG_WARNING, "inet: Could not allocate receive queue for network '%s', the system is out of memory\n", n->name);
            goto fail;
        }
        n->recv_queue_idx = 0;
        n->recv_queue_size = MSG_SIZE_MAX + 1;
    }

    //Create a send queue for the network, if one doesn't already exist
    if(n->send_queue == 0){
        if((n->send_queue = queue_create()) == NULL){
            logmsg(LOG_WARNING, "inet: Could not allocate send queue for network '%s', the system is out of memory\n", n->name);
            goto fail;
        }
    }

    int rval = 0;
    //Initiate connection
    if(connect(sock, addr->ai_addr, addr->ai_addrlen) == -1){
        if(errno == EINPROGRESS){
            logmsg(LOG_DEBUG, "inet: Connection to '%s' host '%s' initiated\n", n->name, host);
            //Add socket to watch-list
            if(watch_add(sock, true) == -1){
                logmsg(LOG_WARNING, "inet: Could not monitor connection to '%s' for completion, the system is out of memory\n", n->name);
                goto fail;
            }
            rval = 1;
        }
        else{
            logmsg(LOG_WARNING, "inet: Could not connect to '%s' host '%s', %s\n", n->name, host, strerror(errno));
            //connection failed, clean up and use the next address next time
            n->addr_idx++;
            goto fail;
        }
    }
    else{
        logmsg(LOG_DEBUG, "inet: Connection to '%s' host '%s' completed immediately\n", n->name, host);
        if(inet_tls_upgrade(n) == -1){
            n->addr_idx++;
            goto fail;
        }
        //Add socket to watch-list
        if(watch_add(sock, false) == -1){
            logmsg(LOG_WARNING, "inet: Could not monitor connection to '%s', the system is out of memory\n", n->name);
            goto fail;
        }
    }
    
    //Map the socket to the network config struct
    int ret = htable_add(rc_network_sock, (uint8_t*)&n->sock, sizeof(n->sock), n);
    if(ret == -1){
        logmsg(LOG_WARNING, "inet: Could not map socket for '%s' host '%s', the system is out of memory\n", n->name, host);
        watch_remove(sock);
        goto fail;
    }
    else if(ret == -2){
        //mapping already exists, this should never happen
        logmsg(LOG_ERR, "inet: Could not map socket for '%s' host '%s', mapping already exists\n", n->name, host);
        close(sock);
        _exit(-1);
    }

    free(host);

    return rval;

    fail:
        close(sock);
        free(host);
        return -1;
}

int inet_connect_all(){
    size_t size = 0;
    struct htable_key** networks = htable_get_keys(rc_network, &size);
    if(networks == NULL){
        logmsg(LOG_WARNING, "inet: Failed to load list of configured networks\n");
        logmsg(LOG_WARNING, "inet: There are no configured networks or the system is out of memory\n");
        return -1;
    }

    for(size_t i = 0; i < size; i++){
        struct network* n = htable_lookup(rc_network, networks[i]->key, networks[i]->key_size);
        if(n != NULL){
            inet_connect(n);
        }
        else{
            logmsg(LOG_WARNING, "inet: Could not connect to network '%s', the system is out of memory\n", networks[i]->key);
        }
    }

    htable_key_list_free(networks, size);

    return 0;
}

int inet_check_connection(struct network* n){
    int optval = INT_MAX;
    socklen_t optlen = sizeof(optval);
    if(getsockopt(n->sock, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1){
        //This is never supposed to fail
        logmsg(LOG_ERR, "inet: Unable to check socket connection for errors\n");
        _exit(-1);
    }

    if(optval == 0){
        logmsg(LOG_DEBUG, "inet: Connection to network '%s' was successful\n", n->name);
        watch_remove(n->sock);

        if(inet_tls_upgrade(n) == -1){
            goto fail;
        }

        if(watch_add(n->sock, false) == -1){
            logmsg(LOG_WARNING, "inet: Could not monitor connection to '%s', the system is out of memory\n", n->name);
            goto fail;
        }
		return 0;
    }
    else if(optval == INT_MAX){
        //This is never supposed to happen
        logmsg(LOG_ERR, "inet: Unable to check socket connection for errors\n");
        _exit(-1);
    }

    fail:
        logmsg(LOG_WARNING, "inet: Connection to network '%s' was unsuccessful, %s\n", n->name, strerror(optval));
        htable_remove(rc_network_sock, (uint8_t*)&n->sock, sizeof(n->sock));
        watch_remove(n->sock);
        n->addr_idx++;
        close(n->sock);
        return -1;
}

int inet_disconnect(struct network* n){
    close(n->sock);
    watch_remove(n->sock);
    if(htable_remove(rc_network_sock, (uint8_t*)&n->sock, sizeof(n->sock)) == -1){
        logmsg(LOG_WARNING, "inet: Could not remove socket mapping for network '%s', no such mapping exists\n", n->name);
    }

    //De-allocate receive queue
    free(n->recv_queue);
    n->recv_queue = 0;
    n->recv_queue_size = 0;
    n->recv_queue_idx = 0;

    //De-allocate send queue
    queue_destroy(n->send_queue);
    n->send_queue = 0;

    return 0;
}

int inet_recv(struct network* n){
    //Subtract one to account for the null-terminator
    size_t bytes_to_read = (n->recv_queue_size - 1) - n->recv_queue_idx;
    if(bytes_to_read == 0){
        logmsg(LOG_DEBUG, "inet: Message buffer for network '%s' is full\n", n->name);
        return 0;
    }

    ssize_t ret;
    if(n->ssl){
        ret = tls_read(n->ctx, n->recv_queue + n->recv_queue_idx, bytes_to_read);
        if(ret == TLS_WANT_POLLIN){
            return -1;
        }
        else if(ret == -1){
            logmsg(LOG_WARNING, "inet: Could not read from network '%s' via TLS connection, %s\n", n->name, tls_error(n->ctx));
            //I do this because idk how to get more specific info from libtls
            //This needs to be corrected eventually
            n->addr_idx++;
            goto reconn;
        }
    }
    else{
        ret = recv(n->sock, n->recv_queue + n->recv_queue_idx, bytes_to_read, 0);
    }
    
    if(ret == -1){
        switch(errno){
#if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
#endif
            case EAGAIN:
                logmsg(LOG_DEBUG, "inet: Read from network '%s' would block\n", n->name);
                return 0;
            case ECONNRESET:
            case ENOTCONN:
            case ETIMEDOUT:
                logmsg(LOG_WARNING, "inet: Lost connection to network '%s', %s", n->name, strerror(errno));
                goto reconn;
            case ENOBUFS:
            case ENOMEM:
                logmsg(LOG_WARNING, "inet: Could not read from network '%s', %s\n", n->name, strerror(errno));
                return -1;
            default:
                logmsg(LOG_ERR, "inet: Unable to read from network '%s', %s\n", n->name, strerror(errno));
                _exit(-1);
        }
    }
    else if(ret == 0){
        logmsg(LOG_WARNING, "inet: Network '%s' closed the connection\n", n->name);
        goto reconn;
    }

    n->recv_queue_idx += ret;
    return 0;

    reconn:
        logmsg(LOG_DEBUG, "inet: Attempting to reconnect to network '%s'\n", n->name);
        inet_disconnect(n);
        inet_connect(n);
        return -1;
}

int inet_send_immediate(struct network* n, const char* buf, size_t len){
    ssize_t ret;
    if(n->ssl){
        ret = tls_write(n->ctx, buf, len);
        if(ret == TLS_WANT_POLLOUT || ret == TLS_WANT_POLLIN){
            return -1;
        }
        else if(ret == -1){
            logmsg(LOG_WARNING, "inet: Could not send to network '%s' via TLS connection, %s\n", n->name, tls_error(n->ctx));
            //I do this because idk how to get more specific info from libtls
            //This needs to be corrected eventually
            n->addr_idx++;
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
                logmsg(LOG_WARNING, "inet: Send to network '%s' would block, discarding message\n", n->name);
            case ENOBUFS:
                return -1;
            case ECONNRESET:
            case EPIPE:
            //We might have an address that can be reached from another interface
            case ENETDOWN:
            case ENETUNREACH:
                logmsg(LOG_WARNING, "inet: Lost connection to network '%s', %s", n->name, strerror(errno));
                goto reconn;
            default:
                logmsg(LOG_ERR, "inet: Unable to send to network '%s', %s\n", n->name, strerror(errno));
                _exit(-1);
        }
    }

    return 0;

    reconn:
        logmsg(LOG_DEBUG, "inet: Attempting to reconnect to network '%s'\n", n->name);
        inet_disconnect(n);
        inet_connect(n);
        return -1;
}

int inet_send(struct network* n){
    struct item* itm = NULL;
    while((itm = queue_peek(n->send_queue)) != NULL){
        if(inet_send_immediate(n, (char*)itm->value, itm->size) == 0){
            logmsg(LOG_DEBUG, "%s >> %.*s", n->name, (int)itm->size, itm->value);
            free(queue_dequeue(n->send_queue));
        }
        else{
            free(itm);
            return -1;
        }
    }

    free(itm);

    return 0;
}

int inet_send_all(){
    size_t size = 0;
    struct htable_key** fds = htable_get_keys(rc_network_sock, &size);
    if(fds == NULL){
        logmsg(LOG_WARNING, "inet: Failed to load list of connected networks\n");
        logmsg(LOG_WARNING, "inet: There are no connected networks, or the system is out of memory\n");
        return -1;
    }

    for(size_t i = 0; i < size; i++){
        struct network* n = htable_lookup(rc_network_sock, fds[i]->key, fds[i]->key_size);
        //This should never happen.
        if(n == NULL){
            logmsg(LOG_ERR, "inet: Failed to load network configuration\n");
            _exit(-1);
        }

        inet_send(n);
    }

    htable_key_list_free(fds, size);

    return 0;
}
