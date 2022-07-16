set(libnbuss_SRCS
configuration.h
IGenericServer.h
IGenericServer.cpp
UnixSocketServer.h
UnixSocketServer.cpp
FileDescriptor.h
ThreadDecorator.cpp
ThreadDecorator.h
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
)

# Declare the library
add_library(libnbuss STATIC
    ${libnbuss_SRCS}
)

# Specify here the include directories exported
# by this library
target_include_directories(libnbuss PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
