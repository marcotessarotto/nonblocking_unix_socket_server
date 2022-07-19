set(Main_SRCS
    main.cpp
)
 
set(CMAKE_CXX_FLAGS -pthread)
 

add_executable(nbuss_server  ${Main_SRCS}
)

find_package(Boost REQUIRED log log_setup )
include_directories(${BOOST_INCLUDE_DIRS})
message("boost lib: ${Boost_LIBRARIES}")
message("boost inc:${Boost_INCLUDE_DIR}")

# Specify here the libraries this program depends on
target_link_libraries(nbuss_server 
	libnbuss
    ${Boost_LIBRARIES}
)



install(TARGETS nbuss_server DESTINATION bin)
