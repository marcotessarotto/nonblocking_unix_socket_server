file(GLOB SRCS *.cpp)

ADD_EXECUTABLE(testlibnbuss ${SRCS})

TARGET_LINK_LIBRARIES(testlibnbuss
    libnbuss
    libgtest
    libgmock
)

add_test(NAME testlibnbuss
         COMMAND testlibnbuss)
