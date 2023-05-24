#ifndef _UTIL_
#define _UTIL_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "err.h"

#define TTL_VALUE     4

inline static void connect_socket(int socket_fd, const struct sockaddr_in *address) {
    CHECK_ERRNO(connect(socket_fd, (struct sockaddr *) address, sizeof(*address)));
}


int bind_mulitcast_socket(uint16_t port, char * address, struct sockaddr_in* remote_address) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    ENSURE(socket_fd > 0);
    // after socket() call; we should close(sock) on any execution path;
    
    /* ustawienie TTL dla datagramów rozsyłanych do grupy */
    int optval = TTL_VALUE;
    CHECK_ERRNO(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optval, sizeof optval));

     /* ustawienie adresu i portu odbiorcy */
    remote_address->sin_family = AF_INET;
    remote_address->sin_port = htons(port);
    if (inet_aton(address, &remote_address->sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }

    return socket_fd;
}

long read_number(char* str) {
    char* endptr;

    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            // fatal("ARGUMENT PRASING error: Not a number\n");
            return -1;
        }
    }

    errno = 0;
    long num = strtol(str, &endptr, 10);

    // Check for various error conditions
    if ((errno == ERANGE && (num == LONG_MAX || num == LONG_MIN)) || (errno != 0 && num == 0) || num < 0) {
        // fatal("ARGUMENT PRASING error: %s is invalid number\n", str);
        return -1;
    }

    if (endptr == str) {
        // fatal("ARGUMENT PRASING error: %s is not a number\n", str);
        return -1;
    }

    return num;
}

// inline static int open_udp_socket() {
//     int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
//     if (socket_fd < 0) {
//         PRINT_ERRNO();
//     }

//     return socket_fd;
// }

long get_milis() {
    namespace sc = std::chrono;

    auto time = sc::system_clock::now(); // get the current time
    auto since_epoch = time.time_since_epoch(); // get the duration since epoch
    auto millis = sc::duration_cast<sc::milliseconds>(since_epoch);
    return millis.count();
}

void print_bytes(uint8_t* bytes, size_t num_bytes) {
    for (size_t i = 0; i < num_bytes; i++) {
        printf("%02x ", bytes[i]);
    }
    printf("\n");
}
#endif