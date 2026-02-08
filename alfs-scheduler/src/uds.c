/**
 * ALFS - Unix Domain Socket Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "uds.h"

#define INITIAL_BUFFER_SIZE 4096
#define MAX_MESSAGE_SIZE (16 * 1024 * 1024)  /* 16MB max message */

int uds_connect(const char *socket_path) {
    if (!socket_path) {
        return -1;
    }
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    
    return sock;
}

void uds_disconnect(int sock) {
    if (sock >= 0) {
        close(sock);
    }
}

int uds_receive(int sock, char *buffer, size_t buffer_size) {
    if (sock < 0 || !buffer || buffer_size == 0) {
        return -1;
    }
    
    ssize_t n = recv(sock, buffer, buffer_size - 1, 0);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv");
        }
        return -1;
    }
    
    buffer[n] = '\0';
    return (int)n;
}

char *uds_receive_message(int sock) {
    if (sock < 0) {
        errno = EINVAL;
        return NULL;
    }
    
    size_t buffer_size = INITIAL_BUFFER_SIZE;
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        errno = ENOMEM;
        return NULL;
    }
    
    size_t total_received = 0;
    int brace_count = 0;
    bool in_string = false;
    bool found_start = false;
    
    while (1) {
        /* Ensure we have room for more data */
        if (total_received >= buffer_size - 1) {
            if (buffer_size >= MAX_MESSAGE_SIZE) {
                errno = EMSGSIZE;
                free(buffer);
                return NULL;  /* Message too large */
            }
            buffer_size *= 2;
            char *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                errno = ENOMEM;
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
        
        /* Receive one byte at a time for proper JSON boundary detection */
        ssize_t n = recv(sock, buffer + total_received, 1, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;
            }
            /*
             * Clean EOF can happen right after a newline terminator from the
             * peer. If we never saw '{', the buffered bytes are only framing
             * whitespace and should not be treated as a JSON message.
             */
            if (n == 0 && !found_start) {
                errno = 0;  /* Clean EOF between JSON messages */
                free(buffer);
                return NULL;
            }
            if (total_received > 0) {
                buffer[total_received] = '\0';
                return buffer;
            }
            free(buffer);
            return NULL;
        }
        
        char c = buffer[total_received];
        total_received++;
        
        /* Track JSON structure for complete message detection */
        if (c == '"' && (total_received < 2 || buffer[total_received - 2] != '\\')) {
            in_string = !in_string;
        } else if (!in_string) {
            if (c == '{') {
                found_start = true;
                brace_count++;
            } else if (c == '}') {
                brace_count--;
                if (found_start && brace_count == 0) {
                    /* Complete JSON object received */
                    buffer[total_received] = '\0';
                    return buffer;
                }
            }
        }
    }
}

int uds_send(int sock, const char *message, size_t length) {
    if (sock < 0 || !message) {
        return -1;
    }
    
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t n = send(sock, message + total_sent, length - total_sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("send");
            return -1;
        }
        total_sent += n;
    }
    
    return (int)total_sent;
}

int uds_send_message(int sock, const char *message) {
    if (!message) {
        return -1;
    }
    
    size_t length = strlen(message);
    int result = uds_send(sock, message, length);
    
    /* Send newline as message terminator */
    if (result >= 0) {
        if (send(sock, "\n", 1, 0) < 0) {
            return -1;
        }
    }
    
    return result;
}
