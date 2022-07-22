
 
 set(testlibnbuss_SRCS
  main.cpp
  tcpsocket_tests.cpp
  test_1server_1client_unixsocket.cpp
  test_common.cpp
  test_threadedserver2.cpp  
  testlibnbuss.h
  testlibnbuss.cpp
  unixsocket_tests.cpp
)
 

ADD_EXECUTABLE(testlibnbuss 
${testlibnbuss_SRCS})


find_package(Boost REQUIRED log log_setup )
include_directories(${BOOST_INCLUDE_DIRS})
message("boost lib: ${Boost_LIBRARIES}")
message("boost inc:${Boost_INCLUDE_DIR}")

TARGET_LINK_LIBRARIES(testlibnbuss
    libnbuss
    libgtest
    libgmock
    ${Boost_LIBRARIES}
)

add_test(NAME testlibnbuss
         COMMAND testlibnbuss)

         


 