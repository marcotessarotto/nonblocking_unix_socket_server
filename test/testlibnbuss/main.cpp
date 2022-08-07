#include "gtest/gtest.h"

void init_log();

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    init_log();

    // TestUnixSocketThreadedServer2 TcpServerClientReadWriteTest
    // ::testing::GTEST_FLAG(filter) = "*.TestUnixSocketThreadedServer2";
    //::testing::GTEST_FLAG(filter) = "*.TcpServerMultipleThreadClientsReadWriteTest";

    //::testing::GTEST_FLAG(filter) = "*.TcpServerSameClientMultipleTimesConnectOnly";

    //::testing::GTEST_FLAG(filter) = "*.TcpServerSameClientMultipleTimesConnectOnlyWorkQueue";

    //::testing::GTEST_FLAG(filter) = "*.TcpSumServerSameClientMultipleTimesConnectOnlyWorkQueue";


    // valgrind -s --leak-check=full --show-leak-kinds=all cmake.debug.linux.x86_64/test/testlibnbuss/testlibnbuss

    int ret = RUN_ALL_TESTS();
    return ret;
}
