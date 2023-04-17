#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>

#include "err.h"
#include "util.h"


#define INDEX_NR 438620
char* dest_addr = (char*)-1;
uint16_t data_port = 20000 + (INDEX_NR % 10000);
uint32_t pSize = 512;


void read_port(char* string) {
    errno = 0;
    unsigned long port = strtoul(string, NULL, 10);
    PRINT_ERRNO();
    if (port > UINT16_MAX) {
        fatal("%ul is not a valid port number", port);
    }

    data_port = (uint16_t)port;
}
bool read_parameters(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            i++;
            if (i < argc) {
                dest_addr = argv[i];
            }
            else {
                fatal("-a flag requires a dest_addr value.\n");
            }
        }
        else if (strcmp(argv[i], "-P") == 0) {
            i++;
            if (i < argc) {
                read_port(argv[i]);
            }
            else {
                fatal("-P flag requires a data_port value.\n");
            }
        }
        else if (strcmp(argv[i], "-p") == 0) {
            i++;
            if (i < argc) {
                pSize = strtoul(argv[i], NULL, 10);
                PRINT_ERRNO();
            }
            else {
                fatal("-p flag requires a pSize value.\n");
            }
        }
    }

    if (dest_addr == (char*)-1) {
        fatal("DEST_ADDR has to be secified by parameters '-a'");
    }

    return false;
}

struct sockaddr_in get_send_address(char* host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* address_result;
    CHECK(getaddrinfo(host, NULL, &hints, &address_result));

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET; // IPv4
    send_address.sin_addr.s_addr =
        ((struct sockaddr_in*)(address_result->ai_addr))->sin_addr.s_addr; // IP address
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

void uint64_to_uint8(uint64_t value, uint8_t* bytes) {
    for (int i = 0;8 > i;i++) {
        bytes[7 - i] = value & 0xff;
        value = value >> 8;
    }
}

void prepare_buffer() {

}

void send_message(int socket_fd, const struct sockaddr_in* send_address, uint8_t* buffer, ssize_t buffer_size) {
    int send_flags = 0;
    socklen_t address_length = (socklen_t)sizeof(*send_address);
    errno = 0;
    ssize_t sent_length = sendto(socket_fd, buffer, buffer_size, send_flags,
        (struct sockaddr*)send_address, address_length);
    if (sent_length < 0) {
        PRINT_ERRNO(); // to change
    }
    ENSURE(sent_length == buffer_size); // to change
}

int main(int argc, char* argv[]) {
    read_parameters(argc, argv);

    uint64_t session_id = time(NULL);
    uint64_t first_byte_num = 0;
    uint8_t buffer[pSize + sizeof(first_byte_num) + sizeof(session_id)];
    // beginign of err loop

    struct sockaddr_in send_address = get_send_address(dest_addr, data_port);

    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }

    char* client_ip = inet_ntoa(send_address.sin_addr);
    uint16_t client_port = ntohs(send_address.sin_port);

    while (1) {

        size_t bytes_read = fread(buffer + sizeof(first_byte_num) + sizeof(session_id), 1, pSize, stdin);
        if (bytes_read != pSize) {
            fprintf(stderr, "Error: first_byte_num=%lu could not read pSize=%u bytes from stdin. Last %ld won't be sent \n", first_byte_num, pSize, bytes_read);
            break;
        }
        else {
            uint64_to_uint8(session_id, buffer);
            uint64_to_uint8(first_byte_num, buffer + sizeof(session_id));
            send_message(socket_fd, &send_address, buffer, sizeof(buffer));

            // printf("sent to %s:%u: \n", client_ip, client_port);
            // printf("session_id of size %lu = ", sizeof(session_id));
            // print_bytes(buffer, sizeof(session_id));

            // printf("first_byte_num of size %lu = ", sizeof(first_byte_num));
            // print_bytes(&buffer[sizeof(session_id)], sizeof(first_byte_num));

            // printf("audio_data of size %u = ", pSize);
            // print_bytes(&buffer[sizeof(session_id) + sizeof(first_byte_num)], pSize);
        }
        first_byte_num += pSize;




    }

    CHECK_ERRNO(close(socket_fd));

    return 0;
}
