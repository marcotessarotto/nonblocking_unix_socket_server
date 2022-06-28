#include "gtest/gtest.h"
#include "UnixSocketServer.h"

// testing class NonblockingUnixSocketServer
class NonblockingUnixSocketServerTest : public ::testing::Test {

protected:

    // You can do set-up work for each test here.
    NonblockingUnixSocketServerTest();

    // You can do clean-up work that doesn't throw exceptions here.
    virtual ~NonblockingUnixSocketServerTest();

    // Code here will be called immediately after the constructor (right
    // before each test).
    virtual void SetUp();

    // Code here will be called immediately after each test (right
    // before the destructor).
    virtual void TearDown();

};
