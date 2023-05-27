#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <endian.h>
#include <string>
#include <regex>
#include <set>
#include <list>

#include "err.h"
#include "util.h"
#include "cb.h"

#define INDEX_NR 438620

std::string discovery_addr = "255.255.255.255";
uint16_t data_port = 20000 + (INDEX_NR % 10000);
uint16_t ctrl_port = 30000 + (INDEX_NR % 10000);
uint16_t ui_port = 10000 + (INDEX_NR % 10000);
uint32_t pSize = 512;
uint32_t bSize = 65536;

uint32_t rTime = 250;

std::string desired_station_name = "";

pthread_mutex_t mutex;
sem_t whait_for_buffer_fill;

bool read_parameters(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0) {
            i++;
            if (i < argc) {
                data_port = read_port(argv[i]);
                if (data_port <= 0) {
                    fatal("%s is not valid port number", argv[i]);
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
                fatal("-C flag requires a ctrl_port value.\n");
            }
        }
        else if (strcmp(argv[i], "-U") == 0) {
            i++;
            if (i < argc) {
                ui_port = read_port(argv[i]);
                if (ui_port <= 0) {
                    fatal("%s is not a valid port number", argv[i]);
                }
            }
            else {
                fatal("-U flag requires a ui_port value.\n");
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
        else if (strcmp(argv[i], "-b") == 0) {
            i++;
            if (i < argc) {
                bSize = read_number(argv[i]);
                if (bSize <= 0) {
                    fatal("bSize <= 0");
                }
            }
            else {
                fatal("-b flag requires a bSize value.\n");
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
                desired_station_name = argv[i];
                if (desired_station_name == "") {
                    fatal("sationName cannot be empty");
                }
                if (desired_station_name.length() > 64) {
                    fatal("sationName lenght cannot be longer than 64");
                }
                if (desired_station_name[0] == ' ') {
                    fatal("sationName cannot start with space");
                }
                if (desired_station_name[desired_station_name.length() - 1] == ' ') {
                    fatal("sationName cannot end with space");
                }
                for (long unsigned int j = 0;desired_station_name.length() > j;j++) {
                    if (desired_station_name[j] < 32 || desired_station_name[j]>127) {
                        fatal("sationName can only contain ASCII from 32 to 127");
                    }
                }
            }
            else {
                fatal("-n flag requires a name of station.\n");
            }
        }
    }

    return false;
}

size_t recive_package(int socket_fd, struct sockaddr_in* client_address, uint8_t* buffer, size_t max_length) {
    socklen_t address_length = (socklen_t)sizeof(*client_address);
    int flags = 0; // we do not request anything special
    errno = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
        (struct sockaddr*)client_address, &address_length);
    if (len < 0) {
        errno = 0;
        return 0;
        // PRINT_ERRNO(); // TODO: restart conection or just return len 0
    }
    return (size_t)len;
}

void u8tou64(uint8_t* const u8, uint64_t* u64) {
    memcpy(u64, u8, 8);
    *u64 = be64toh(*u64);
}

std::map<station, time_t> stations;
pthread_mutex_t stations_mutex;
station crr_station;
station choosen_station;
bool is_station_choosen = false;

struct reader_args {
    int64_t* curent;
    int64_t* max_nr;
    uint8_t* b;
    int64_t* b_nr;
    bool* is_whaiting;
    bool* stop;
};

void* reader_function(void* arg) {
    uint32_t buffer_segments = bSize / pSize;
    uint8_t output_buffer[pSize];
    struct reader_args buf = *(struct reader_args*)arg;
    free(arg);

    while (1) {
        pthread_mutex_lock(&mutex);
        if (*buf.stop || buf.b_nr[*buf.curent % buffer_segments] != (int64_t)*buf.curent) {
            if (buf.b_nr[*buf.curent % buffer_segments] != (int32_t)*buf.curent) {
                std::cerr << "stoping on " << *buf.curent << " becouse in buf is:" << buf.b_nr[*buf.curent % buffer_segments] << "\n";
            }
            // fprintf(stderr, "reader whait 1 on crr_nr = %u\n" , *buf.curent);
            *buf.stop = false;
            *buf.is_whaiting = true;
            pthread_mutex_unlock(&mutex);
            sem_wait(&whait_for_buffer_fill);
            *buf.is_whaiting = false;

        }

        // fprintf(stderr, "reader: crr_nr = %u\n", *buf.curent);
        // fprintf(stderr, "dest size = %lu, src size = %lu", sizeof output_buffer,sizeof buf.b[*buf.curent % buffer_segments] );
        memcpy(output_buffer, &buf.b[(*buf.curent % buffer_segments) * pSize], pSize); //  &buf.b[*buf.curent % buffer_segments] not ideal
        buf.b_nr[*buf.curent % buffer_segments] = -1;
        *buf.curent = *buf.curent + 1;

        pthread_mutex_unlock(&mutex);

        fwrite(output_buffer, sizeof(uint8_t), pSize, stdout);
        // fprintf(stderr, "reader done written: \n");
    }
}


int control_socket_fd;
struct sockaddr_in control_address;

void* reply_listiner_function(void* arg) {

    size_t read_length;
    uint8_t buffer[(13 + 5 + 30 + 64) + 10];

    std::string input;
    std::regex reply_pattern(R"(BOREWICZ_HERE (\d{1,3}.\d{1,3}.\d{1,3}.\d{1,3}) (\d+) (\w+(?: \w+)*)\n)");
    std::smatch matches;
    station stt;
    struct sockaddr_in antelope;

    while (1) {

        read_length = recive_package(control_socket_fd, &antelope, buffer, sizeof(buffer));
        if (read_length < 0) {
            // std::cerr << "got read_length = " << read_length << " on control port\n";
            continue;
        }

        //LOOKUP
        input = uint8ArrayToString(buffer, read_length);
        //REPLY
        if (std::regex_match(input, matches, reply_pattern)) {
            stt.address = matches[1].str();
            stt.port = read_port(matches[2].str().c_str());
            stt.name = matches[3].str();
            stt.direct_address = antelope;

            // std::cerr << "(reply_listiner) got ip = " << "'" << stt.address << "' port=" << stt.port << " station name: '" << stt.name << "'\n";

            //check 
            if (inet_aton(stt.address.c_str(), &antelope.sin_addr) != 1) continue;
            if ((antelope.sin_addr.s_addr & 0xf0) != 0xe0) continue; //TODO: use macro because 240.999.0.0

            if (stt.port <= 0)continue;

            if (stt.name.length() > 64) continue;

            // std::cerr << "(reply_listiner) validated\n";

            pthread_mutex_lock(&stations_mutex);
            stations[stt] = time(NULL);
            if (!is_station_choosen) {
                if (desired_station_name == "" || desired_station_name == stt.name) {
                    choosen_station = stt;
                    is_station_choosen = true;
                }
            }
            // std::cerr << "(reply_listiner) stations size = " << stations.size() << "\n";
            pthread_mutex_unlock(&stations_mutex);
        }
    }
}

void* discovery_function(void* arg) {
    time_t last_update = time(NULL) - 5;


    while (1) {
        while (last_update + 5 > time(NULL)) {
            sleep(last_update + 5 - time(NULL));
        }
        last_update = time(NULL);

        std::string str = "ZERO_SEVEN_COME_IN\n";
        send_message(control_socket_fd, &control_address, reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
    }
}

uint32_t buffer_segments;
int64_t max_segment_nr = 0;
int64_t currently_read_segment_index = 0;
int64_t* b_buffer_segment_nr;
int64_t* retransmit_segment_nr;
long* retransmit_time;
long do_retransmit_requester() {
    std::list<int64_t> lst;
    long now = get_milis();

    long erliest_request = now + (rTime);
    for (uint32_t i = currently_read_segment_index + 1; i < max_segment_nr; i++) {
        if (b_buffer_segment_nr[i % buffer_segments] == -1) {
            if (retransmit_segment_nr[i % buffer_segments] != i) {
                retransmit_segment_nr[i % buffer_segments] = i;
                lst.push_back(i * pSize);
                retransmit_time[i % buffer_segments] = now + (rTime);
                continue;
            }

            if (retransmit_time[i % buffer_segments] <= now) {
                lst.push_back(i * pSize);
                retransmit_time[i % buffer_segments] = now + (rTime);
                continue;
            }
            erliest_request = std::min(erliest_request, retransmit_time[i % buffer_segments]);
        }
    }

    if (!lst.empty()) {
        std::stringstream ss;
        ss << "LOUDER_PLEASE ";
        ss << lst.front();
        lst.pop_front();
        for (long x : lst) {
            ss << "," << x;
        }
        ss << "\n";
        std::string str = ss.str();
        std::cerr << "(retransmit_requester) " << str;

        send_message(control_socket_fd, &crr_station.direct_address, reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
    }
    return erliest_request;
}

void* retransmit_requester_function(void* arg) {
    long erliest_request = get_milis();
    while (1) {
        while (erliest_request > get_milis())
        {
            //   std::cerr<< "(retransmit_requester) ++\n"; 
            usleep((erliest_request - get_milis()) * 1000);
        }
        // std::cerr<< "(retransmit_requester) " << get_milis()<<"\n";

        if (crr_station.port != 0) { // check if is station is online
            erliest_request = do_retransmit_requester();
        }
        else {
            erliest_request = get_milis() + rTime;
        }
    }
}

std::set<int> ui_clients_fs;
pthread_mutex_t ui_mutex;
void* handle_ui_connection(void* client_fd_ptr) {
    int client_fd = *(int*)client_fd_ptr;
    free(client_fd_ptr);

    char input[1];
    for (;;) {
        ssize_t read = receive_message(client_fd, input, sizeof(input), 0);
        if (read <= 0)
            break;

        if (input[0] != '\033') continue; // Escape character

        read = receive_message(client_fd, input, sizeof(input), 0);
        if (read <= 0)
            break;
        std::cerr << "(ui) 2 leter" << (int)input[0] << "\n";
        if (input[0] != '[') continue;

        read = receive_message(client_fd, input, sizeof(input), 0);
        if (read <= 0)
            break;

        std::cerr << "(ui) secind leter" << (int)input[0] << "\n";
        if (input[0] == 'A') { // Up arrow
            std::cout << "Moving up" << std::endl;
            continue;
        }
        if (input[0] == 'B') { // Down arrow
            std::cout << "Moving down" << std::endl;
            continue;
        }
    }


    std::cerr << "(ui) cloasing " << client_fd << "\n";
    pthread_mutex_lock(&ui_mutex);
    ui_clients_fs.erase(client_fd);
    close(client_fd);
    pthread_mutex_unlock(&ui_mutex);

    return 0;
}

void* UI_updater_function(void* arg) {
    std::vector<station> crr_stations;
    station crr_choosen_station;
    bool is_crr_choosen;

    while (1) {
         sleep(1);//TODO add sleep until update


        pthread_mutex_lock(&stations_mutex);
        for (std::pair<station, time_t> st : stations) crr_stations.push_back(st.first);
        crr_choosen_station = choosen_station;
        is_crr_choosen = is_station_choosen;
        pthread_mutex_unlock(&stations_mutex);

        pthread_mutex_lock(&ui_mutex);
        for (int fd : ui_clients_fs) {
            print_ui(crr_stations, is_crr_choosen, crr_choosen_station, fd);

        }
        pthread_mutex_unlock(&ui_mutex);
    }
}

void* UI_accepter_function(void* arg) {
    int socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }
    bind_socket(socket_fd, ui_port);
    CHECK_ERRNO(listen(socket_fd, 5));

    std::cerr << ("Listening ui on port ") << ui_port << "\n";
    pthread_mutex_init(&ui_mutex, NULL);

    pthread_t thread;
    CHECK_ERRNO(pthread_create(&thread, NULL, UI_updater_function, NULL));

    while (1) {
        struct sockaddr_in client_addr;

        int client_fd = accept_connection(socket_fd, &client_addr);
        if (client_fd < 0) continue;

        std::cerr << "(ui) new " << client_fd << "\n";
        // Arguments for the thread must be passed by pointer
        int* client_fd_pointer = (int*)malloc(sizeof(int));
        if (client_fd_pointer == NULL) {
            fatal("malloc");
        }
        *client_fd_pointer = client_fd;

        pthread_mutex_lock(&ui_mutex);
        ui_clients_fs.insert(client_fd);
        pthread_mutex_unlock(&ui_mutex);

        CHECK_ERRNO(pthread_create(&thread, 0, handle_ui_connection, client_fd_pointer));
        CHECK_ERRNO(pthread_detach(thread));
    }
}

int main(int argc, char* argv[]) {
    read_parameters(argc, argv);

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&stations_mutex, NULL);
    sem_init(&whait_for_buffer_fill, 0, 0);

    uint64_t session_id;
    uint64_t first_byte_num;
    uint8_t input_buffer[pSize + sizeof(first_byte_num) + sizeof(session_id)];

    uint64_t crr_session_id = 0;
    buffer_segments = bSize / pSize;
    if (buffer_segments < 2) fatal(" there has to be atleast 2 buffer_segments, bSize >= 2*pSize");
    fprintf(stderr, "buffer_segments: %u\n", buffer_segments);
    uint8_t b_buffer[buffer_segments * pSize];
    int64_t b1[buffer_segments];
    b_buffer_segment_nr = b1;
    for (uint32_t i = 0;buffer_segments > i;i++) b_buffer_segment_nr[i] = -1;
    int64_t b2[buffer_segments];
    retransmit_segment_nr = b2;
    for (uint32_t i = 0;buffer_segments > i;i++) retransmit_segment_nr[i] = -1;
    long b3[buffer_segments];
    retransmit_time = b3;

    // uint32_t last_compleated_segment_nr = -1;
    int64_t current_segment_nr;
    bool is_reader_whaiting = false;
    bool reader_stoop = true;
    bool has_reader_been_started = false;

    //network
    control_socket_fd = open_udp_socket(); //bind_socket(ctrl_port);
    /* uaktywnienie rozgłaszania (ang. broadcast) */
    int optval = 1;
    CHECK_ERRNO(setsockopt(control_socket_fd, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval));
    control_address = get_send_address(discovery_addr.c_str(), ctrl_port);
    fprintf(stderr, "Starting to listen on control \n");


    struct reader_args* args = (struct reader_args*)malloc(sizeof(struct reader_args));
    if (args == NULL) fatal("malloc");

    args->b = b_buffer;
    args->b_nr = b_buffer_segment_nr;
    args->curent = &currently_read_segment_index;
    args->max_nr = &max_segment_nr;
    args->is_whaiting = &is_reader_whaiting;
    args->stop = &reader_stoop;
    pthread_t reader_thread;
    CHECK(pthread_create(&reader_thread, NULL, reader_function, (void*)args));
    pthread_t discovery_thread;
    CHECK(pthread_create(&discovery_thread, NULL, discovery_function, NULL));
    pthread_t reply_listiner_thread;
    CHECK(pthread_create(&reply_listiner_thread, NULL, reply_listiner_function, NULL));
    pthread_t retransmit_requester_thread;
    CHECK(pthread_create(&retransmit_requester_thread, NULL, retransmit_requester_function, NULL));
    pthread_t UI_thread;
    CHECK(pthread_create(&UI_thread, NULL, UI_accepter_function, NULL));


    struct timeval tv; // timeout of 2s
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    struct ip_mreq ip_mreq;
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    int socket_fd = -1;
    struct sockaddr_in client_address;
    size_t read_length;

    crr_station.port = 0;
    while (1) {

        while (!is_station_choosen)
        {
            std::cerr << "whaiting for station\n";
            sleep(2);
        }
        if (choosen_station != crr_station) {
            crr_station = choosen_station;
            crr_session_id = 0; // start new play

            //disconect from old
            if (socket_fd != -1) lave_mulitcast_recive_socket(&socket_fd, ip_mreq);

            //connent to new
            socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // new udp ip4 socket
            if (socket_fd < 0) {
                PRINT_ERRNO();
            }
            CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv)); //timeout

            // multicast group
            if (inet_aton(crr_station.address.c_str(), &ip_mreq.imr_multiaddr) == 0) {
                fatal("inet_aton - invalid multicast address\n");
            }
            CHECK_ERRNO(setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq));

            bind_socket(socket_fd, crr_station.port);
            fprintf(stderr, "Starting to listen on port %u\n", data_port);

        }
        pthread_mutex_lock(&stations_mutex);
        if (time(NULL) > stations[crr_station] + 20) {
            std::cerr << "station inactive for more than 20s\n";
            crr_station.port = 0;
            is_station_choosen = false;
            if (socket_fd != -1) lave_mulitcast_recive_socket(&socket_fd, ip_mreq);
        }
        pthread_mutex_unlock(&stations_mutex);

        memset(input_buffer, 0, sizeof(input_buffer));
        read_length = recive_package(socket_fd, &client_address, input_buffer, sizeof(input_buffer));
        // fprintf(stderr, "recived bytes %lu\n", read_length); // TOdelete
        if (read_length != sizeof(input_buffer)) {
            if (read_length != 0) {
                fprintf(stderr, "recived unexpected amouth of bytes %lu\n", read_length);
            }
            continue;
        }


        u8tou64(input_buffer, &session_id);
        u8tou64(&input_buffer[sizeof(session_id)], &first_byte_num);
        current_segment_nr = first_byte_num / pSize;

        pthread_mutex_lock(&mutex);
        if (crr_session_id < session_id) {
            fprintf(stderr, "new session: %ld \n", session_id);

            crr_session_id = session_id;

            if (is_reader_whaiting == false) {
                reader_stoop = true;
            }
            has_reader_been_started = false;

            currently_read_segment_index = current_segment_nr;
            max_segment_nr = current_segment_nr;
            for (uint32_t i = 0;buffer_segments > i;i++) b_buffer_segment_nr[i] = -1;
            for (uint32_t i = 0;buffer_segments > i;i++) retransmit_segment_nr[i] = -1;

        }
        // fprintf(stderr,"session_id = %lu of size %lu \n",session_id, sizeof(session_id));

        // fprintf(stderr, "first_byte_num =%lu of size %lu \n ", first_byte_num, sizeof(first_byte_num));
        if (crr_session_id == session_id) {

            if (current_segment_nr > max_segment_nr) {
                max_segment_nr = current_segment_nr;
            }
            do_retransmit_requester();

            // fprintf(stderr, "writing to %u\n", current_segment_nr);
            if (b_buffer_segment_nr[current_segment_nr % buffer_segments] != -1)
            {
                fprintf(stderr, "OVERWRITING: bolck nr %lu", b_buffer_segment_nr[current_segment_nr % buffer_segments]);
            }
            memcpy(&b_buffer[(current_segment_nr % buffer_segments) * pSize], &input_buffer[sizeof(session_id) + sizeof(first_byte_num)], pSize);
            b_buffer_segment_nr[current_segment_nr % buffer_segments] = current_segment_nr;

            if (has_reader_been_started && is_reader_whaiting) {
                //restart play
                crr_session_id = 0;
            }

            if (!has_reader_been_started && (max_segment_nr >= (currently_read_segment_index + (3 * buffer_segments / 4)))) {
                has_reader_been_started = true;
                if (reader_stoop) reader_stoop = false;

                if (is_reader_whaiting) sem_post(&whait_for_buffer_fill);
                else pthread_mutex_unlock(&mutex);
            }
            else {
                // fprintf(stderr, "max = %d , buffer[%d] = %d\n", max_segment_nr, currently_read_segment_index, b_buffer_segment_nr[currently_read_segment_index %buffer_segments ]);
                pthread_mutex_unlock(&mutex);
            }
        }
        else {
            pthread_mutex_unlock(&mutex);
        }
    }
    fprintf(stderr, "finished exchange\n");

    CHECK_ERRNO(close(socket_fd));
    pthread_join(reader_thread, NULL);
    pthread_join(discovery_thread, NULL);
    pthread_join(reply_listiner_thread, NULL);
    pthread_join(retransmit_requester_thread, NULL);

    pthread_mutex_destroy(&stations_mutex);
    pthread_mutex_destroy(&mutex);
    sem_destroy(&whait_for_buffer_fill);

    return 0;
}
