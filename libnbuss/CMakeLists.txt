set(libnbuss_SRCS
configuration.h
IGenericServer.h
IGenericServer.cpp
UnixSocketServer.h
UnixSocketServer.cpp
FileDescriptor.h
Worker.cpp
Worker.h
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
