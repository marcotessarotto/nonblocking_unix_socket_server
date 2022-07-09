cmake_minimum_required(VERSION 3.14)


project(nonblocking_unix_socket_server)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

enable_testing()

add_subdirectory(main)
add_subdirectory(test)
add_subdirectory(libnbuss) 
add_subdirectory(unix_socket_client)
 