file(GLOB SRCS
 main.cpp
 testlibnbuss.cpp
 unixsocket_tests.cpp
 test_common.cpp
 *.cpp
 )

ADD_EXECUTABLE(testlibnbuss ${SRCS})


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

         


 