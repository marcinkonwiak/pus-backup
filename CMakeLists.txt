cmake_minimum_required(VERSION 3.29)
project(pus_backup C)

set(CMAKE_C_STANDARD 11)

add_executable(pus_backup
    cw_02/k.c
    cw_02/s.c
    cw_03/k.c
    cw_03/s.c
    cw_03_cw/k.c
    cw_03_cw/s.c
    cw_04/k.c
    cw_04/s.c
    cw_04_cw/k.c
    cw_04_cw/s.c
    cw_05/k.c
    cw_05/s.c
    pus1/a/p.c
    pus1/b/p.c
    pus1/c/p.c
    pus1/d/p.c
    pus2/client.c
    pus2/client2.c
    pus2/server.c
    pus2/server3.c)
