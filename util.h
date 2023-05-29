#ifndef _UTIL_
#define _UTIL_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
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
#include <iostream>

#include "err.h"

#define TTL_VALUE     4

inline static void connect_socket(int socket_fd, const struct sockaddr_in* address) {
    CHECK_ERRNO(connect(socket_fd, (struct sockaddr*)address, sizeof(*address)));
}

inline static int open_udp_socket() {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }

    return socket_fd;
}

inline static void bind_socket(int socket_fd, uint16_t port) {
    struct sockaddr_in address;
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    address.sin_port = htons(port);

    // bind the socket to a concrete address
    CHECK_ERRNO(bind(socket_fd, (struct sockaddr*)&address,
        (socklen_t)sizeof(address)));
}

int bind_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    ENSURE(socket_fd > 0);
    // after socket() call; we should close(sock) on any execution path;

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port);

    // bind the socket to a concrete address
    CHECK_ERRNO(bind(socket_fd, (struct sockaddr*)&server_address,
        (socklen_t)sizeof(server_address)));

    return socket_fd;
}

void lave_mulitcast_recive_socket(int* socket_fd,struct ip_mreq ip_mreq) {
    /* odłączenie od grupy rozsyłania */
    CHECK_ERRNO(setsockopt(*socket_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof(ip_mreq)));
    CHECK_ERRNO(close(*socket_fd));
    (*socket_fd) = -1;
}


int bind_mulitcast_socket(uint16_t port, char* address, struct sockaddr_in* remote_address) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    ENSURE(socket_fd > 0);
    // after socket() call; we should close(sock) on any execution path;

    /* ustawienie TTL dla datagramów rozsyłanych do grupy */
    int optval = TTL_VALUE;
    CHECK_ERRNO(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval));

    /* ustawienie adresu i portu odbiorcy */
    remote_address->sin_family = AF_INET;
    remote_address->sin_port = htons(port);
    if (inet_aton(address, &remote_address->sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }

    return socket_fd;
}

in_addr_t get_raw_ip_address(const char* host) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* address_result;
    CHECK(getaddrinfo(host, NULL, &hints, &address_result));

    in_addr_t ret = ((struct sockaddr_in*)(address_result->ai_addr))->sin_addr.s_addr; // IP address
    freeaddrinfo(address_result);

    return ret;
}

struct sockaddr_in get_send_address(const char* host, uint16_t port) {
    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET; // IPv4
    send_address.sin_addr.s_addr = get_raw_ip_address(host); // IP address
    send_address.sin_port = htons(port); // port from the command line


    return send_address;
}

void send_message(int socket_fd, const struct sockaddr_in* send_address, const uint8_t* buffer, ssize_t buffer_size) {
    int send_flags = 0;
    socklen_t address_length = (socklen_t)sizeof(*send_address);
    errno = 0;
    ssize_t sent_length = sendto(socket_fd, buffer, buffer_size, send_flags,
        (struct sockaddr*)send_address, address_length);
    if (sent_length != buffer_size) {
        std::cout << "send len = " << sent_length << ",   to send =" << buffer_size << "\n";
        //     PRINT_ERRNO(); // to change
    }
    // ENSURE(sent_length == buffer_size); // to change
}

inline static size_t receive_message(int socket_fd, void *buffer, size_t max_length, int flags) {
    errno = 0;
    ssize_t received_length = recv(socket_fd, buffer, max_length, flags);
    if (received_length < 0) {
        return 0;
    }
    return (size_t) received_length;
}

ssize_t
writen(int fd, const char *b, size_t n)
{
    size_t left = n;

    while (left)
    {
        ssize_t res = write(fd, b, left);
        if (res == -1)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if(res == 0 ) return -1;
        left -= res;
        b += res;
    }
    return n;
}

inline static int accept_connection(int socket_fd, struct sockaddr_in *client_address) {
    socklen_t client_address_length = (socklen_t) sizeof(*client_address);

    int client_fd = accept(socket_fd, (struct sockaddr *) client_address, &client_address_length);

    return client_fd;
}

long read_number(const char* str) {
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

uint16_t read_port(const char* string) {
    errno = 0;
    long port = 0;

    port = read_number(string);
    PRINT_ERRNO();
    if (port > UINT16_MAX || port <= 0) {
        return 0;
    }

    return port;
}

std::string uint8ArrayToString(const uint8_t* arr, size_t size) {
    std::stringstream ss;

    for (size_t i = 0; i < size; ++i) {
        ss << static_cast<char>(arr[i]);
    }

    return ss.str();
}

long get_milis() {
    namespace sc = std::chrono;

    auto time = sc::system_clock::now(); // get the current time
    auto since_epoch = time.time_since_epoch(); // get the duration since epoch
    auto millis = sc::duration_cast<sc::milliseconds>(since_epoch);
    return millis.count();
}


struct station {
    std::string address;
    uint16_t port;
    std::string name;
    sockaddr_in direct_address;
};
inline bool operator<(const station& lhs, const station& rhs)
{
    if (lhs.address == rhs.address) {
        if (lhs.port == rhs.port) return lhs.name < rhs.name;
        return lhs.port < rhs.port;
    }
    return lhs.address < rhs.address;
}
inline bool operator==(const station& lhs, const station& rhs)
{
    return (lhs.address == rhs.address && lhs.port == rhs.port && lhs.name == rhs.name);
}
inline bool operator!=(const station& lhs, const station& rhs)
{
    return !(lhs == rhs);
}

std::string separator_line = "------------------------------------------------------------------------\n";
std::string title_line = " SIK Radio\n";
void print_ui(std::vector<station> stations, bool is_choosen, station choosen, int ui_fd){
    std::stringstream ss;
    ss <<'\033'<<'c'<< separator_line<< "\n" << title_line <<"\n"<<separator_line<< "\n";

    for(station st : stations){
        if(is_choosen && choosen == st) ss<<" > ";
        ss<<st.name<<"\n"<<"\n";
    }

    ss<<separator_line;
    std::string out = ss.str();
    writen(ui_fd, reinterpret_cast<const char*>(out.c_str()), out.length());
}

void print_bytes(uint8_t* bytes, size_t num_bytes) {
    for (size_t i = 0; i < num_bytes; i++) {
        printf("%02x ", bytes[i]);
    }
    printf("\n");
}
#endif