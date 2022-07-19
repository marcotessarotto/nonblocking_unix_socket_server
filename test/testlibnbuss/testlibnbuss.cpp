#include <time.h>
#include <exception>
#include <stdexcept>

#include "testlibnbuss.h"

#include <ThreadDecorator.h>
#include "UnixSocketClient.h"
#include "Crc16.h"


#include "Logger.h"


//using ::testing::Return;

using namespace std;
using namespace nbuss_server;
using namespace nbuss_client;

extern void init_log();


#define TEST_LOG(lvl) BOOST_LOG_TRIVIAL(lvl)

NonblockingUnixSocketServerTest::NonblockingUnixSocketServerTest()  {
	// Have qux return true by default
	//ON_CALL(m_bar,qux()).WillByDefault(Return(true));
	// Have norf return false by default
	//ON_CALL(m_bar,norf()).WillByDefault(Return(false));
}

NonblockingUnixSocketServerTest::~NonblockingUnixSocketServerTest() {
}

void NonblockingUnixSocketServerTest::SetUp() {
	openlog("testlibnbuss", LOG_CONS | LOG_PERROR, LOG_USER);

	init_log();

    //logging::core::get()->

	//sink.set_format("[" << boost::logging::level << "]" << " - " << boost::logging::date("%d/$m/$Y") << "," << boost::logging::timestamp << "," << boost::logging::message);

	// https://www.boost.org/doc/libs/1_79_0/libs/log/example/doc/tutorial_trivial.cpp

	// https://www.boost.org/doc/libs/1_79_0/libs/log/doc/html/index.html

	// debian packages: libboost-log1.74-dev
}

void NonblockingUnixSocketServerTest::TearDown() {
}

TEST_F(NonblockingUnixSocketServerTest, TestParameters) {

	TEST_LOG(info) << "***TestParameters**" << endl;

	// expect exception

	EXPECT_THROW(UnixSocketServer server("", 0), std::invalid_argument);
}

static Crc16 crc;
uint16_t serverDataCrc16;
bool calcCrc = false;

static void my_listener(IGenericServer *srv, int fd, enum job_type_t job_type) {

	switch (job_type) {
	case CLOSE_SOCKET:

		TEST_LOG(debug) << "[server][my_listener] closing socket " << fd << endl;
		//close(fd);
		srv->close(fd);

		break;
	case DATA_REQUEST:
		TEST_LOG(debug) << "[server][my_listener] incoming data fd=" << fd << endl;

		// read all data from socket
		auto data = UnixSocketServer::read(fd, 256);

		TEST_LOG(debug) << "[server][my_listener] number of vectors returned: " << data.size() << endl;

		int counter = 0;
		for (std::vector<char> item : data) {
			TEST_LOG(trace) << "[server][my_listener] buffer " << counter++ << ": " << item.size() << " bytes" << endl;

			if (calcCrc) {
				serverDataCrc16 = crc.update_crc_16(serverDataCrc16,
						reinterpret_cast<const unsigned char*>(item.data()),
						item.size());
			}

			UnixSocketServer::write(fd, item);
		}

	}

	TEST_LOG(info) << "[server][my_listener] ending - fd=" << fd << endl;

}

TEST_F(NonblockingUnixSocketServerTest, UnixSocketServerTest) {
	TEST_LOG(info) << "***UnixSocketServerTest***" << endl;


	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };


	std::thread th([](UnixSocketServer *uss) {

		TEST_LOG(debug) << "[server] thread starting" << endl;

		uss->listen(::my_listener);

		TEST_LOG(debug) << "[server] thread ending" << endl;
	}, &uss);

//	std::thread th([&uss]() {
//
//		cout << "[thread] server thread starting" << endl;
//
//		uss.listen(::my_listener);
//
//		cout << "[thread] server thread ending" << endl;
//	});

    uss.waitForServerReady();

	UnixSocketClient usc;

	TEST_LOG(info) << "[client] connect to server\n";
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	TEST_LOG(info) << "[client] writing to socket\n";
	usc.write(v);

	TEST_LOG(info) << "[client] reading from socket\n";
	// read server response
	auto response = usc.read(1024);

	TEST_LOG(info) << "[client] received data size: " << response.size() << endl;

	TEST_LOG(info) << "[client] closing socket" << endl;
	usc.close();

	// TODO: valgrind block here; but the program alone works
	// valgrind -s   --leak-check=yes test/testlibnbuss/testlibnbuss


	struct timespec ts1, ts2;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts1);
	long dt = 0;

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0) {
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);
		dt = ((ts2.tv_sec - ts1.tv_sec) * 1000000000L + ts2.tv_nsec) - ts1.tv_nsec;


		if (dt > 2000000) // 2 milliseconds
			break;
	}

	if (dt > 0) {
		cout << "dt = " << dt << endl;
	}

	TEST_LOG(info) << "[server] calling terminate" << endl;
	uss.terminate();

	TEST_LOG(info) << "[server] join thread" << endl;
	th.join();

	TEST_LOG(info) << "test finished!" << endl;

	EXPECT_EQ(response.size(), 12);



}

TEST_F(NonblockingUnixSocketServerTest, TcpServerClientReadWriteTest) {

	try {

		TEST_LOG(info) << "***TcpServerClientReadWriteTest***" << endl;

		// create instance of tcp non blocking server

		TcpServer ts { 10001, "0.0.0.0", 10 };

		// listen method is called in another thread
		ThreadDecorator threadedServer { ts };

		std::atomic_thread_fence(std::memory_order_release);

		TEST_LOG(info) << "[server] starting server\n";
		// when start returns, server has started listening for incoming connections
		threadedServer.start(my_listener);

//
//		// ?!?!?! sometimes threadedServer is destroyed here, terminating the program with message:
//		// terminate called without an active exception
//
		TcpClient tc;

		TEST_LOG(info) << "[client] connect to server\n";
		tc.connect("0.0.0.0", 10001);

		std::string s = "test message";
		std::vector<char> v(s.begin(), s.end());

		TEST_LOG(info) << "[client] writing to socket\n";
		tc.write(v);

		TEST_LOG(info) << "[client] reading from socket\n";
		// read server response
		auto response = tc.read(1024);

		TEST_LOG(info) << "[client] received data size: " << response.size() << endl;

		TEST_LOG(info) << "[client] closing socket" << endl;
		tc.close();

		// spin... consider using a condition variable
		while (ts.getActiveConnections() > 0)
			;

		TEST_LOG(info) << "[server] stopping server" << endl;
		threadedServer.stop();

		TEST_LOG(info) << "[main] test finished!" << endl;

		EXPECT_EQ(response.size(), 12);

	} catch (const std::exception &e) {
		TEST_LOG(error) << "exception: " << e.what() << endl;
	}

}

TEST_F(NonblockingUnixSocketServerTest, UdpServerClientReadWriteTest) {

	TEST_LOG(info) << "***UdpServerClientReadWriteTest**" << endl;

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadDecorator threadedServer(uss);

	// ThreadDecorator threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	UnixSocketClient usc;

	TEST_LOG(info) << "[client] connect to server\n";
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	TEST_LOG(info) << "[client] writing to socket\n";
	usc.write(v);

	TEST_LOG(info) << "[client] reading from socket\n";
	// read server response
	auto response = usc.read(1024);

	TEST_LOG(info) << "[client] received data size: " << response.size() << endl;

	TEST_LOG(info) << "[client] closing socket" << endl;
	usc.close();

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	TEST_LOG(info) << "test finished!" << endl;

	EXPECT_EQ(response.size(), 12);
}

TEST_F(NonblockingUnixSocketServerTest, UdpServerClientReadWriteLongBufferTest) {

	TEST_LOG(info) << "***UdpServerClientReadWriteLongBufferTest**" << endl;

	calcCrc = true;
	const ssize_t bufferSize = 4096*3;

	//Crc16 crc;
	//uint16_t longBufferCrc;

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadDecorator threadedServer(uss);

	// ThreadDecorator threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	UnixSocketClient usc(true);

	TEST_LOG(info) << "[client] connect to server\n";
	usc.connect(socketName);

//	std::string s = "test message";
//	std::vector<char> v(s.begin(), s.end());

	std::vector<char> longBuffer(bufferSize);

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

	TEST_LOG(info) << "[client] crc16 of data sent by client = " << clientCrc << endl;
	// crc16: 61937

	// reset CRC16
	serverDataCrc16 = CRC_START_16;

	TEST_LOG(info) << "[client] writing to socket\n";
	usc.write(longBuffer);

	uint16_t clientCrc2 = CRC_START_16;

	size_t total_bytes_received = 0;

	while (total_bytes_received < bufferSize) {
		TEST_LOG(info) << "[client] reading from socket\n";
		// read server response
		auto response = usc.read(1024);

		TEST_LOG(info) << "[client] received data size: " << response.size() << endl;

		if (response.size() == 0) {
			// no data available, sleep for 1 ms
			struct timespec t;

			t.tv_sec = 0;  // seconds
			t.tv_nsec = 1 * 1000 * 1000; // nanoseconds

			nanosleep(&t, NULL);

			continue;
		}

		total_bytes_received += response.size();

		clientCrc2 = crc.update_crc_16(clientCrc2,
				reinterpret_cast<const unsigned char*>(response.data()),
				response.size());

	}

	TEST_LOG(info) << "[server] crc16 of data received from client = " << serverDataCrc16 << endl;
	TEST_LOG(info) << "[client] crc16 of data received from server = " << clientCrc2 << endl;


	TEST_LOG(info) << "[client] closing socket" << endl;
	usc.close();

	TEST_LOG(info) << "[server] long buffer crc = " << serverDataCrc16 << endl;

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	TEST_LOG(info) << "test finished!" << endl;

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

