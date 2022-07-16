#include <exception>
#include <stdexcept>

#include "testlibnbuss.h"

#include <ThreadDecorator.h>
#include "UnixSocketClient.h"
#include "Crc16.h"

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

void NonblockingUnixSocketServerTest::SetUp() {
	openlog("testlibnbuss", LOG_CONS | LOG_PERROR, LOG_USER);
}

void NonblockingUnixSocketServerTest::TearDown() {
}

TEST_F(NonblockingUnixSocketServerTest, TestParameters) {

	cout << "\n***TestParameters**" << endl;

	// expect exception

	EXPECT_THROW(UnixSocketServer server("", 0), std::invalid_argument);
}

static Crc16 crc;
uint16_t serverDataCrc16;
bool calcCrc = false;

static void my_listener(IGenericServer *srv, int fd, enum job_type_t job_type) {

	switch (job_type) {
	case CLOSE_SOCKET:

		cout << "closing socket " << fd << endl;
		//close(fd);
		srv->close(fd);

		break;
	case DATA_REQUEST:
		cout << "incoming data fd=" << fd << endl;

		// read all data from socket
		auto data = UnixSocketServer::read(fd);

		cout << "number of vectors returned: " << data.size() << endl;

		int counter = 0;
		for (std::vector<char> item : data) {
			cout << counter++ << ": " << item.size() << " bytes" << endl;

			if (calcCrc) {
				serverDataCrc16 = crc.update_crc_16(serverDataCrc16,
						reinterpret_cast<const unsigned char*>(item.data()),
						item.size());
			}

			UnixSocketServer::write(fd, item);
		}

	}

}

TEST_F(NonblockingUnixSocketServerTest, TcpServerClientReadWriteTest) {

	try {

		cout << "\n***TcpServerClientReadWriteTest**" << endl;

		// create instance of tcp non blocking server

		TcpServer ts { 10001, "0.0.0.0", 10 };

		// listen method is called in another thread
		ThreadDecorator threadedServer { ts };

		std::atomic_thread_fence(std::memory_order_release);

		cout << "[server] starting server\n";
		// when start returns, server has started listening for incoming connections
		threadedServer.start(my_listener);

//
//		// ?!?!?! sometimes threadedServer is destroyed here, terminating the program with message:
//		// terminate called without an active exception
//
		TcpClient tc;

		cout << "[client] connect to server\n";
		tc.connect("0.0.0.0", 10001);

		std::string s = "test message";
		std::vector<char> v(s.begin(), s.end());

		cout << "[client] writing to socket\n";
		tc.write(v);

		cout << "[client] reading from socket\n";
		// read server response
		auto response = tc.read(1024);

		cout << "[client] received data size: " << response.size() << endl;

		cout << "[client] closing socket" << endl;
		tc.close();

		// spin... consider using a condition variable
		while (ts.getActiveConnections() > 0)
			;

		cout << "[server] stopping server" << endl;
		threadedServer.stop();

		cout << "[main] test finished!" << endl;

		EXPECT_EQ(response.size(), 12);

	} catch (const std::exception &e) {
		cout << "exception: " << e.what() << endl;
	}

}

TEST_F(NonblockingUnixSocketServerTest, UdpServerClientReadWriteTest) {

	cout << "\n***UdpServerClientReadWriteTest**" << endl;

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadDecorator threadedServer(uss);

	// ThreadDecorator threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	UnixSocketClient usc;

	cout << "[client] connect to server\n";
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	cout << "[client] writing to socket\n";
	usc.write(v);

	cout << "[client] reading from socket\n";
	// read server response
	auto response = usc.read(1024);

	cout << "[client] received data size: " << response.size() << endl;

	usc.close();

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	cout << "test finished!" << endl;

	EXPECT_EQ(response.size(), 12);
}

TEST_F(NonblockingUnixSocketServerTest, UdpServerClientReadWriteLongBufferTest) {

	cout << "\n***UdpServerClientReadWriteLongBufferTest**" << endl;

	calcCrc = true;

	//Crc16 crc;
	//uint16_t longBufferCrc;

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadDecorator threadedServer(uss);

	// ThreadDecorator threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	UnixSocketClient usc;

	cout << "[client] connect to server\n";
	usc.connect(socketName);

//	std::string s = "test message";
//	std::vector<char> v(s.begin(), s.end());

	std::vector<char> longBuffer(4096 * 3);

	//longBuffer.assign(4096*3, '*');
	// initialize longBuffer
	for (std::size_t i = 0; i < longBuffer.size(); ++i) {
		longBuffer[i] = i % 10;
	}

//	for(auto it = std::begin(longBuffer); it != std::end(longBuffer); ++it) {
//	    std::cout << *it << "\n";
//	}

	uint16_t clientCrc = crc.crc_16(
			reinterpret_cast<const unsigned char*>(longBuffer.data()),
			longBuffer.size());

	cout << "[client] crc16 of data sent by client = " << clientCrc << endl;
	// crc16: 61937

	// reset CRC16
	serverDataCrc16 = CRC_START_16;

	cout << "[client] writing to socket\n";
	usc.write(longBuffer);

	uint16_t clientCrc2 = CRC_START_16;

	while (true) {
		cout << "[client] reading from socket\n";
		// read server response
		auto response = usc.read(1024);

		cout << "[client] received data size: " << response.size() << endl;

		if (response.size() == 0)
			break;

		clientCrc2 = crc.update_crc_16(clientCrc2,
				reinterpret_cast<const unsigned char*>(response.data()),
				response.size());

	}

	cout << "[server] cr16 of data received from client = " << serverDataCrc16 << endl;
	cout << "[client] cr16 of data received from server = " << clientCrc2 << endl;

	usc.close();

	cout << "[server] long buffer crc = " << serverDataCrc16 << endl;

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	cout << "test finished!" << endl;

	calcCrc = false;

	EXPECT_EQ(clientCrc2, serverDataCrc16);
	EXPECT_EQ(clientCrc2, clientCrc);
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

