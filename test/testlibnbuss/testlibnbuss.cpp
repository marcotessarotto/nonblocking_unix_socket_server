#include <exception>
#include <stdexcept>

#include "testlibnbuss.h"

#include <ThreadDecorator.h>
#include "UnixSocketClient.h"

//using ::testing::Return;


using namespace std;
using namespace nbuss_server;
using namespace nbuss_client;

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


static void my_listener(int fd, enum job_type_t job_type) {

	switch(job_type) {
	case CLOSE_SOCKET:

		cout << "closing socket " << fd << endl;
		close(fd);

		break;
	case DATA_REQUEST:
		cout << "incoming data fd=" << fd  << endl;

		// read all data from socket
		auto data = UnixSocketServer::readAllData(fd);

		cout << "size of data: " << data.size() << endl;

		int counter = 0;
		for (std::vector<char> item: data) {
			cout << counter << ": " << item.data() << endl;

			UnixSocketServer::writeToSocket(fd, item);
		}

	}

}


//TEST_F(NonblockingUnixSocketServerTest, CreateServerAndConnect) {
//
//	std::string socketName{"/tmp/testsocket"};
//
//	UnixSocketServer server(socketName, 1);
//
//	Worker worker(server);
//
//	//server.setup();
//	worker.setup();
//
//	worker.start(my_listener);
//
//	UnixSocketClient client;
//
//	client.connect(socketName);
//
//	client.write( something....)
//
//	// wait for server answer
//
//	// close client
//
//	// close server
//
//
//}



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

