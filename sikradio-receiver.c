#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "err.h"
#include "util.h"

#define INDEX_NR 438620
uint16_t data_port = 20000 + (INDEX_NR % 10000);
uint32_t pSize = 512;
uint32_t bSize = 65536;


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
        if (strcmp(argv[i], "-P") == 0) {
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
        else if (strcmp(argv[i], "-b") == 0) {
            i++;
            if (i < argc) {
                bSize = strtoul(argv[i], NULL, 10);
                PRINT_ERRNO();
            }
            else {
                fatal("-b flag requires a bSize value.\n");
            }
        }
    }

    return false;
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

size_t recive_package(int socket_fd, struct sockaddr_in* client_address, uint8_t* buffer, size_t max_length) {
    socklen_t address_length = (socklen_t)sizeof(*client_address);
    int flags = 0; // we do not request anything special
    errno = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
        (struct sockaddr*)client_address, &address_length);
    if (len < 0) {
        PRINT_ERRNO(); // TODO: restart conection
    }
    return (size_t)len;
}

void u8tou64(uint8_t* const u8, uint64_t* u64) {
    memcpy(&u64, u8, sizeof u64);
}

int main(int argc, char* argv[]) {
    read_parameters(argc, argv);

    uint64_t session_id;
    uint64_t first_byte_num;
    uint8_t input_buffer[pSize + sizeof(first_byte_num) + sizeof(session_id)];

    uint32_t buffer_segments = bSize / pSize;
    if (buffer_segments < 2) fatal(" there has to be atleast 2 buffer_segments, bSize >= 2*pSize");
    uint8_t b_buffer[buffer_segments][pSize];
    uint32_t b_buffer_segment_nr[buffer_segments];
    uint32_t last_compleated_segment_nr = -1;
    uint32_t current_segment_nr;
    uint32_t currently_read_segment_index = -1;


    printf("Listening on port %u\n", data_port);

    memset(input_buffer, 0, sizeof(input_buffer));

    int socket_fd = bind_socket(data_port);

    struct sockaddr_in client_address;
    size_t read_length;
    while(1) {
        read_length = recive_package(socket_fd, &client_address, input_buffer, sizeof(input_buffer));
        if (read_length != sizeof(input_buffer)) {
            printf("recived unexpected amouth of bytes %lu", read_length);
        }
        else {
            // TODO: get mutex for b_bufor
            u8tou64(input_buffer, &session_id);
            u8tou64(&input_buffer[sizeof(session_id)], &first_byte_num);
            current_segment_nr = first_byte_num / pSize;
            if (current_segment_nr != last_compleated_segment_nr + 1) {
                for(uint32_t i = last_compleated_segment_nr; i < current_segment_nr; i++){
                    if(b_buffer_segment_nr[i%buffer_segments] == -1){
                        fprintf("MISSING: BEFORE %u EXPECTED %u", current_segment_nr, i);
                    }
                }
            }

            memcpy(b_buffer[buffer_segments], &input_buffer[sizeof(session_id) + sizeof(first_byte_num)], sizeof(b_buffer[buffer_segments]) );
            b_buffer_segment_nr[buffer_segments] = current_segment_nr;
        }

    } 
    printf("finished exchange\n");

    CHECK_ERRNO(close(socket_fd));

    return 0;
}
