#include <exception>
#include <stdexcept>

#include "testlibnbuss.h"

//using ::testing::Return;


using namespace std;
using namespace nbuss_server;

NonblockingUnixSocketServerTest::NonblockingUnixSocketServerTest() {
	// Have qux return true by default
	//ON_CALL(m_bar,qux()).WillByDefault(Return(true));
	// Have norf return false by default
	//ON_CALL(m_bar,norf()).WillByDefault(Return(false));
}

NonblockingUnixSocketServerTest::~NonblockingUnixSocketServerTest() {
}
;

void NonblockingUnixSocketServerTest::SetUp() {
}
;

void NonblockingUnixSocketServerTest::TearDown() {
}
;

TEST_F(NonblockingUnixSocketServerTest, TestParameters) {

	// expect exception

	EXPECT_THROW( UnixSocketServer server("", 0) , std::invalid_argument );
}

//TEST_F(FooTest, ByDefaultBazTrueIsTrue) {
//    Foo foo(m_bar);
//    EXPECT_EQ(foo.baz(true), true);
//}
//
//TEST_F(FooTest, ByDefaultBazFalseIsFalse) {
//    Foo foo(m_bar);
//    EXPECT_EQ(foo.baz(false), false);
//}
//
//TEST_F(FooTest, SometimesBazFalseIsTrue) {
//    Foo foo(m_bar);
//    // Have norf return true for once
//    EXPECT_CALL(m_bar,norf()).WillOnce(Return(true));
//    EXPECT_EQ(foo.baz(false), true);
//}

