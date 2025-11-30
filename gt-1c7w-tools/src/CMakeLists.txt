
cmake_minimum_required(VERSION 3.5)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR sh4a)

set(X_DIR /home/parallels/x-tools/sh4a-unknown-linux-gnu)

set(CMAKE_C_COMPILER ${X_DIR}/bin/sh4a-unknown-linux-gnu-gcc)

set(CMAKE_FIND_ROOT_PATH ${X_DIR}/sh4a-unknown-linux-gnu/sysroot)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)


project (hello_cmake)

add_executable(monitor main.c)
add_executable(unlock unlock.c)
add_executable(hookioctl hookioctl.c)
add_executable(guard_open guard_open.c)
add_executable(mon mon.c)
add_executable(openvideo openvideo.c)
add_executable(startstream startstream.c)
add_executable(busserver busserver.c)
add_executable(scanserial scanserial.c)
target_link_options(busserver PRIVATE -static -static)
target_link_libraries(startstream PRIVATE librt.so -static -static)