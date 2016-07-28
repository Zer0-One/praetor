#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <tls.h>

#include "log.h"

int inet_connect(const char* host, const char* service){
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int s = getaddrinfo(host, service, &hints, &result);
    if(s != 0){
        logmsg(LOG_WARNING, "inet: Could not get address info for host %s, %s\n", host, gai_strerror(s));
        return -1;
    }
    int sock = 0;
    for(; result != NULL; result = result->ai_next){
        sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if(sock == -1){
            logmsg(LOG_DEBUG, "inet: Could not open socket for host %s, %s\n", host, strerror(errno));
            continue;
        }
        if(connect(sock, result->ai_addr, result->ai_addrlen) != -1){
            break;
        }
        logmsg(LOG_WARNING, "inet: Could not connect to host %s, %s\n", host, strerror(errno));
        close(sock);
    }
    //put the socket fd into non-blocking mode
    //int flags;
    //if((flags = fcntl(sock, F_GETFL, 0)) == -1){
    //    logmsg(LOG_DEBUG, "inet: Could not get socket file descriptor flags for host %s, %s\n", host, strerror(errno));
    //    return -1;
    //}
    //if(fcntl(sock, F_SETFL, flags | O_NONBLOCK)){
    //    logmsg(LOG_DEBUG, "inet: Could not set socket file descriptor flags for host %s, %s\n", host, strerror(errno));
    //    return -1;
    //}
    freeaddrinfo(result);
    return (result == NULL) ? -1 : sock;
}

struct tls* inet_tls_connect(int sock, const char* host){
    tls_init();
    struct tls_config* cfg;
    struct tls* ctx;
    if((ctx = tls_client()) == NULL){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to host '%s', system out of memory\n", host);
        close(sock);
        return NULL;
    }
    if((cfg = tls_config_new()) == NULL){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to host '%s', system out of memory\n", host);
        goto fail;
    }
    if(!tls_configure(ctx, cfg)){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to host '%s', %s\n", host, tls_error(ctx));
        tls_config_free(cfg);
        goto fail;
    }
    tls_config_free(cfg);
    
    if(!tls_connect_socket(ctx, sock, host)){
        logmsg(LOG_WARNING, "inet: Could not establish TLS connection to host %s, %s\n", host, tls_error(ctx));
        goto fail;
    }

    return ctx;
    fail:
        tls_free(ctx);
        close(sock);
        return NULL;
}
