set(libnbuss_SRCS
configuration.h
IGenericServer.h
IGenericServer.cpp
UnixSocketServer.h
UnixSocketServer.cpp
FileDescriptor.h
ThreadedServer.cpp
ThreadedServer.h
UnixSocketClient.h
UnixSocketClient.cpp
IThreadable.h
IThreadable.cpp
FiniteStateMachine.h
TcpServer.h
TcpServer.cpp
TcpClient.h
TcpClient.cpp
WorkQueue.h
WorkQueue.cpp
Crc16.h
Crc16.cpp
BaseClient.h
BaseClient.cpp
Logger.h
Logger.cpp
)

# Declare the library
add_library(libnbuss STATIC
    ${libnbuss_SRCS}
)

find_package(Boost REQUIRED log log_setup )
include_directories(${BOOST_INCLUDE_DIRS})
message("boost lib: ${Boost_LIBRARIES}")
message("boost inc:${Boost_INCLUDE_DIR}")

# Specify here the include directories exported
# by this library
target_include_directories(libnbuss PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)


target_link_libraries(libnbuss 
    ${Boost_LIBRARIES}
)


