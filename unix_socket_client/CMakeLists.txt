set(client_SRCS
    unixsocketclient.c
)

add_executable(nbuss_client
    ${client_SRCS}
)

# Specify here the libraries this program depends on
#target_link_libraries(nbuss_client
 #   libnbuss
#)

install(TARGETS nbuss_client DESTINATION bin)
