cmake_minimum_required (VERSION 3.1)

project(MIMUW_SIK_UDP_SOCKETS)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g -Wall -Wextra")

add_executable(sikradio-receiver sikradio-receiver.c)
add_executable(sikradio-sender sikradio-sender.c)
target_link_libraries(sikradio-receiver pthread)

install(TARGETS DESTINATION .)
