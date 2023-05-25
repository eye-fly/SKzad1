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
#include <string>
#include <regex>
#include <list>
#include <iostream>
#include <algorithm>
#include <set>

#include "err.h"
#include "util.h"


#define INDEX_NR 438620
char* dest_addr = (char*)-1;
uint16_t data_port = 20000 + (INDEX_NR % 10000);
uint16_t ctrl_port = 30000 + (INDEX_NR % 10000);
uint32_t pSize = 512;
uint32_t fSize = 65536 * 2;
uint32_t rTime = 259;
std::string sationName = "Nienazwany Nadajnik";

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
                data_port = read_port(argv[i]);
                if (data_port <= 0) {
                    fatal("%s is not a valid port number", argv[i]);
                }
            }
            else {
                fatal("-P flag requires a data_port value.\n");
            }
        }
        else if (strcmp(argv[i], "-C") == 0) {
            i++;
            if (i < argc) {
                ctrl_port = read_port(argv[i]);
                if (ctrl_port <= 0) {
                    fatal("%s is not a valid port number", argv[i]);
                }
            }
            else {
                fatal("-P flag requires a data_port value.\n");
            }
        }
        else if (strcmp(argv[i], "-p") == 0) {
            i++;
            if (i < argc) {
                pSize = read_number(argv[i]);
                if (pSize <= 0) {
                    fatal("pSize <= 0");
                }
                if (pSize > 548) {
                    fatal("pSize > 548");
                }
            }
            else {
                fatal("-p flag requires a pSize value.\n");
            }
        }
        else if (strcmp(argv[i], "-f") == 0) {
            i++;
            if (i < argc) {
                fSize = read_number(argv[i]);
                if (fSize < 0) {
                    fatal("fSize <= 0");
                }
            }
            else {
                fatal("-f flag requires a fSize value.\n");
            }
        }
        else if (strcmp(argv[i], "-R") == 0) {
            i++;
            if (i < argc) {
                rTime = read_number(argv[i]);
                if (rTime <= 0) {
                    fatal("rTime <= 0");
                }
            }
            else {
                fatal("-R flag requires a rTime value.\n");
            }
        }
        else if (strcmp(argv[i], "-n") == 0) {
            i++;
            if (i < argc) {
                sationName = argv[i];
                if (sationName == "") {
                    fatal("sationName cannot be empty");
                }
                if (sationName.length() > 64) {
                    fatal("sationName lenght cannot be longer than 64");
                }
                if (sationName[0] == ' ') {
                    fatal("sationName cannot start with space");
                }
                if (sationName[sationName.length() - 1] == ' ') {
                    fatal("sationName cannot end with space");
                }
                for (long unsigned int j = 0;sationName.length() > j;j++) {
                    if (sationName[j] < 32 || sationName[j]>127) {
                        fatal("sationName can only contain ASCII from 32 to 127");
                    }
                }
            }
            else {
                fatal("-n flag requires a name of station.\n");
            }
        }
    }

    if (dest_addr == (char*)-1) {
        fatal("DEST_ADDR has to be secified by parameters '-a'");
    }

    return false;
}

void uint64_to_uint8(uint64_t value, uint8_t* bytes) {
    for (int i = 0;8 > i;i++) {
        bytes[7 - i] = value & 0xff;
        value = value >> 8;
    }
}

size_t recive_package(int socket_fd, struct sockaddr_in* client_address, uint8_t* buffer, size_t max_length) {
    socklen_t address_length = (socklen_t)sizeof(*client_address);
    int flags = 0; // we do not request anything special
    errno = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
        (struct sockaddr*)client_address, &address_length);
    if (len < 0) {
        return 0;
        // PRINT_ERRNO(); // TODO: restart conection or just return len 0
    }
    return (size_t)len;
}

int fifo_segments;
pthread_mutex_t fifo_mutex;
uint8_t* fifo;
uint64_t fifo_first = 0;
uint64_t fifo_last = -1;

std::set<uint64_t> set1;
std::set<uint64_t> set2;
std::set<uint64_t>* in_set = &set1;
std::set<uint64_t>* out_set = &set2;
pthread_mutex_t set_mutex;

bool do_exit = false;
void* control_function(void* arg) {

    /* otwarcie gniazda */
    int socket_fd = bind_socket(ctrl_port); // TODO change to brodcast??
    // int out_socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    // if (socket_fd < 0) {
    //     PRINT_ERRNO();
    // }

    fprintf(stderr, "Starting to listen control on port %u\n", ctrl_port);
    struct sockaddr_in client_address;

    int buffer_size = std::max(13 + 5 + 30 + 64, 14 + 3 + 7 * fifo_segments);
    uint8_t buffer[buffer_size];
    size_t read_length;

    std::regex lookup_pattern(R"(ZERO_SEVEN_COME_IN\n)");
    std::regex resend_pattern(R"(LOUDER_PLEASE ((?:\d+,)*\d+)\n)");
    std::regex numberPattern(R"(\d+)");

    std::string input;
    std::smatch matches;

    while (!do_exit) {

        read_length = recive_package(socket_fd, &client_address, buffer, buffer_size);
        if (read_length < 0) {
            std::cout << "got read_length = " << read_length << " on control port\n";
            continue;
        }

        //LOOKUP
        input = uint8ArrayToString(buffer, read_length);
        if (std::regex_match(input, matches, lookup_pattern)) {
            //send respons
            std::stringstream ss;
            ss << "BOREWICZ_HERE " << dest_addr << " " << data_port << " " << sationName << "\n";
            std::string str = ss.str();
            std::cout<<str;

            char* client_ip = inet_ntoa(client_address.sin_addr);
            uint16_t client_port = ntohs(client_address.sin_port);
            std::cout<<"client_ip="<<client_ip<<", client_port="<<client_port<<"\n";

            // struct sockaddr_in send_address = get_send_address(client_ip, ctrl_port);
            send_message(socket_fd, &client_address, reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
            continue;
        }

        //RESEND
        if (std::regex_match(input, matches, resend_pattern)) {
            std::string numberList = matches[1].str();
            std::list<uint64_t> list;

            std::sregex_iterator it(numberList.begin(), numberList.end(), numberPattern);
            std::sregex_iterator end;

            for (; it != end; ++it) {
                std::smatch match = *it;
                uint64_t number = std::stoull(match.str());
                list.push_back(number);
            }

            // Check if the list has the correct format
            if (list.empty()) {
                std::cout << "Invalid format: Empty list" << std::endl;
                continue;
            }
            else {
                std::cout << "List:";
                pthread_mutex_lock(&set_mutex);
                for (uint64_t number : list) {
                    if (number >= 0) {
                        in_set->insert(number / pSize);
                    }
                    std::cout << " " << number;
                }
                pthread_mutex_unlock(&set_mutex);
                std::cout << std::endl;
            }
            continue;
        }

        std::cout << "got unmached input on control: '" << input << "'\n";
    }
    CHECK_ERRNO(close(socket_fd));
    return NULL;
}

int mulitcast_socket_fd;
struct sockaddr_in data_send_address;
uint64_t session_id;
void* resender_function(void* arg) {
    fprintf(stderr, "Starting resender\n");

    uint64_t crr_first_byte_num;
    uint8_t buffer[pSize + sizeof(crr_first_byte_num) + sizeof(session_id)];
    bool in_fifo;

    std::set<uint64_t>* set;
    long last_resend = get_milis();
    while (!do_exit) {
        while (last_resend + rTime > get_milis())
        {
            usleep(1000 * (last_resend + rTime - get_milis()));
        }

        pthread_mutex_lock(&set_mutex);
        out_set->clear();
        set = in_set;
        in_set = out_set;
        out_set = set;
        pthread_mutex_unlock(&set_mutex);

        for (auto x : (*out_set))
        {

            in_fifo = false;;
            pthread_mutex_lock(&fifo_mutex);
            if (x >= fifo_first && x <= fifo_last) {
                crr_first_byte_num = x * pSize;
                memcpy(buffer + sizeof(crr_first_byte_num) + sizeof(session_id), &fifo[(fifo_last % fifo_segments) * pSize], pSize);
                in_fifo = true;
            }
            pthread_mutex_unlock(&fifo_mutex);

            if (in_fifo) {
                uint64_to_uint8(session_id, buffer);
                uint64_to_uint8(crr_first_byte_num, buffer + sizeof(session_id));
                send_message(mulitcast_socket_fd, &data_send_address, buffer, sizeof(buffer));
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    read_parameters(argc, argv);

    session_id = time(NULL);
    uint64_t first_byte_num = 0;
    uint8_t buffer[pSize + sizeof(first_byte_num) + sizeof(session_id)];

    fifo_segments = fSize / pSize;
    if (fifo_segments < 2) {
        fatal("fifo has to have at least 1 segment");
    }
    uint8_t fifo_local[fifo_segments * pSize];
    fifo = fifo_local;

    // data_send_address = get_send_address(dest_addr, data_port);
    mulitcast_socket_fd = bind_mulitcast_socket(data_port, dest_addr,&data_send_address);
    if (mulitcast_socket_fd < 0) {
        PRINT_ERRNO();
    }

    pthread_mutex_init(&fifo_mutex, NULL);
    pthread_mutex_init(&set_mutex, NULL);

    pthread_t control_thread;
    CHECK(pthread_create(&control_thread, NULL, control_function, NULL));
    pthread_t resender_thread;
    CHECK(pthread_create(&resender_thread, NULL, resender_function, NULL));


    // char* client_ip = inet_ntoa(send_address.sin_addr);
    // uint16_t client_port = ntohs(send_address.sin_port);

    // int erdf =0;
    while (1) {

        size_t bytes_read = fread(buffer + sizeof(first_byte_num) + sizeof(session_id), 1, pSize, stdin);
        if (bytes_read != pSize) {
            fprintf(stderr, "Error: first_byte_num=%lu could not read pSize=%u bytes from stdin. Last %ld won't be sent \n", first_byte_num, pSize, bytes_read);
            break;
        }
        else {

            pthread_mutex_lock(&fifo_mutex);
            fifo_last++;
            memcpy(&fifo[(fifo_last % fifo_segments) * pSize], buffer + sizeof(first_byte_num) + sizeof(session_id), pSize);
            if (fifo_first + fifo_segments == fifo_last) fifo_first++;
            pthread_mutex_unlock(&fifo_mutex);
            // for(int i=0; pSize>i;i++){
            //     printf("%c", fifo[(fifo_last%fifo_segments)*pSize+i]);
            // }
            // printf("\n");


            uint64_to_uint8(session_id, buffer);
            uint64_to_uint8(first_byte_num, buffer + sizeof(session_id));

            // erdf ++;
            // if(erdf% 1000 != 0 ){
            send_message(mulitcast_socket_fd, &data_send_address, buffer, sizeof(buffer));
            // }
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

    CHECK_ERRNO(close(mulitcast_socket_fd));
    do_exit = true;
    pthread_join(control_thread, NULL);
    pthread_join(resender_thread, NULL);
    pthread_mutex_destroy(&fifo_mutex);
    pthread_mutex_destroy(&set_mutex);

    return 0;
}
