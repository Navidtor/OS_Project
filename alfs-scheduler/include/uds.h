/**
 * ALFS - Unix Domain Socket Interface
 */

#ifndef UDS_H
#define UDS_H

#include <stddef.h>

/**
 * Connect to a Unix Domain Socket
 * @param socket_path Path to socket file
 * @return Socket file descriptor on success, -1 on failure
 */
int uds_connect(const char *socket_path);

/**
 * Disconnect from Unix Domain Socket
 * @param sock Socket file descriptor
 */
void uds_disconnect(int sock);

/**
 * Receive a message from UDS
 * @param sock Socket file descriptor
 * @param buffer Buffer to store message
 * @param buffer_size Size of buffer
 * @return Number of bytes received, -1 on error, 0 on connection closed
 */
int uds_receive(int sock, char *buffer, size_t buffer_size);

/**
 * Receive a complete message (reads until newline or connection close)
 * @param sock Socket file descriptor
 * @return Dynamically allocated message string (caller must free), NULL on error
 */
char *uds_receive_message(int sock);

/**
 * Send a message via UDS
 * @param sock Socket file descriptor
 * @param message Message to send
 * @param length Length of message
 * @return Number of bytes sent, -1 on error
 */
int uds_send(int sock, const char *message, size_t length);

/**
 * Send a null-terminated string via UDS
 * @param sock Socket file descriptor
 * @param message Message string to send
 * @return 0 on success, -1 on error
 */
int uds_send_message(int sock, const char *message);

#endif /* UDS_H */
