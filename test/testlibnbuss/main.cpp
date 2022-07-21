#include "gtest/gtest.h"

void init_log();

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    init_log();

    int ret = RUN_ALL_TESTS();
    return ret;
}
