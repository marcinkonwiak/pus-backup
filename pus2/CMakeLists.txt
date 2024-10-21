cmake_minimum_required(VERSION 3.29)
project(pus2 C)

set(CMAKE_C_STANDARD 11)

add_executable(pus2
    server.c
        client.c
        client2.c)
