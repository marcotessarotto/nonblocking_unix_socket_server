#include "gtest/gtest.h"

void init_log();

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    init_log();

    // TestUnixSocketThreadedServer2 TcpServerClientReadWriteTest
    // ::testing::GTEST_FLAG(filter) = "*.TestUnixSocketThreadedServer2";
    ::testing::GTEST_FLAG(filter) = "*.TcpServerMultipleThreadClientsReadWriteTest";

    int ret = RUN_ALL_TESTS();
    return ret;
}
