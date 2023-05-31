cmake_minimum_required(VERSION 3.25)
project(bftdoc)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(bftdoc
        bftdoc.cpp
        bftpeer.cpp
        bftpeer.h
        bftpeerwindow.cpp
        bftpeerwindow.h
        config.h
        datatypes.h bftclient.cpp bftclient.h util.h)
