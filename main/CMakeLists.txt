set(Main_SRCS
    main.cpp
)

add_executable(nbuss_server
    ${Main_SRCS}
)

# Specify here the libraries this program depends on
target_link_libraries(nbuss_server
    libnbuss
)

install(TARGETS nbuss_server DESTINATION bin)
