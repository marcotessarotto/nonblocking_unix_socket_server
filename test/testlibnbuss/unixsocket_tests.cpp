#include <time.h>
#include <exception>
#include <stdexcept>

#include <random>
#include <algorithm>
#include <vector>
#include <iterator>
#include <functional>
#include <iostream>

#include "testlibnbuss.h"

#include "Crc16.h"

#include "Logger.h"

using namespace std;
using namespace nbuss_server;
using namespace nbuss_client;


#define TEST_LOG(lvl) BOOST_LOG_TRIVIAL(lvl)

static Crc16 crc;
static uint16_t serverDataCrc16;
static bool calcCrc = false;


/**
 * check that server instance throws an exception when provided with invalid parameters
 */
TEST_F(NonblockingUnixSocketServerTest, CheckConstructorException) {

	TEST_LOG(info)
	<< "***CheckConstructorException** " << __FILE__;

	// expect exception

	EXPECT_THROW(UnixSocketServer server("", 0), std::invalid_argument);
}


/**
 * listener used by most tests
 * implementation of an echo server
 */
static void my_listener(IGenericServer *srv, int fd, enum job_type_t job_type) {

	switch (job_type) {
	case NEW_SOCKET:
		TEST_LOG(info)	<< "[server][my_listener] NEW_SOCKET " << fd;

		break;
	case CLOSE_SOCKET:

		TEST_LOG(info)	<< "[server][my_listener] CLOSE_SOCKET " << fd;
		//close(fd);
		srv->close(fd);

		break;
	case AVAILABLE_FOR_WRITE:
		TEST_LOG(info)	<< "[server][my_listener] AVAILABLE_FOR_WRITE fd=" << fd;
		break;
	case AVAILABLE_FOR_READ_AND_WRITE:
	case AVAILABLE_FOR_READ:
		TEST_LOG(info)	<< "[server][my_listener] AVAILABLE_FOR_READ fd=" << fd;

		// read all data from socket
		auto data = IGenericServer::read(fd, 256);

		TEST_LOG(debug)
		<< "[server][my_listener] number of vectors returned: " << data.size();

		int counter = 0;
		for (std::vector<char> item : data) {
			TEST_LOG(trace)
			<< "[server][my_listener] buffer " << counter++ << ": "
					<< item.size() << " bytes";

			if (calcCrc) {
				serverDataCrc16 = crc.update_crc_16(serverDataCrc16,
						reinterpret_cast<const unsigned char*>(item.data()),
						item.size());
			}

			// TODO: if write queue for fd is empty, write buffer
			// else copy buffer to queue

			// TODO: if write returns -1, copy remaining data
			while (IGenericServer::write(fd, item) == -1) {
				struct timespec ts { 0, 1000000 };

				nanosleep(&ts, NULL);
			}
		}
		break;


	}

	TEST_LOG(debug)	<< "[server][my_listener] ending - fd=" << fd;

}





/**
 * single server instance, single client instance; using unix sockets
 * buffer size: 12 bytes
 * tests:
 * UnixSocketServer, UnixSocketClient, ThreadedServer
 */
TEST_F(NonblockingUnixSocketServerTest, UnixSocketServerClientReadWriteTest) {

	TEST_LOG(info) << "***UnixSocketServerClientReadWriteTest** " << __FILE__;

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadedServer threadedServer(uss);

	// ThreadedServer threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	UnixSocketClient usc;

	TEST_LOG(info) << "[client] connect to server";
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	TEST_LOG(debug) << "[client] writing to socket";

	usc.write<char>(v);

	TEST_LOG(debug) << "[client] reading from socket";
	// read server response
	auto response = usc.read(1024);

	TEST_LOG(info) << "[client] received data size: " << response.size();

	TEST_LOG(info) << "[client] closing socket";
	usc.close();

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	TEST_LOG(info) << "test finished!";

	EXPECT_EQ(response.size(), 12);
}



/**
 * single server instance, single client instance; using unix sockets;
 * buffer size: 12 bytes
 * tests:
 * UnixSocketServer, UnixSocketClient
 */
TEST_F(NonblockingUnixSocketServerTest, UnixSocketServerTest) {
	TEST_LOG(info) << "***UnixSocketServerTest*** " << __FILE__;

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	std::thread th([](UnixSocketServer *uss) {

		TEST_LOG(debug)
		<< "[server] thread starting";

		uss->listen(::listener_echo_server);

		TEST_LOG(debug)
		<< "[server] thread ending";
	}, &uss);

//	std::thread th([&uss]() {
//
//		cout << "[thread] server thread starting";
//
//		uss.listen(::my_listener);
//
//		cout << "[thread] server thread ending";
//	});

	uss.waitForServerReady();

	UnixSocketClient usc;

	TEST_LOG(info)
	<< "[client] connect to serve";
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	TEST_LOG(debug)	<< "[client] writing to socket";
	usc.write<char>(v);

	TEST_LOG(debug) << "[client] reading from socket";
	// read server response
	auto response = usc.read(1024);

	TEST_LOG(info)
	<< "[client] received data size: " << response.size();

	TEST_LOG(info)
	<< "[client] closing socket";
	usc.close();

	// TODO: valgrind blocks here; but the program alone works
	// valgrind -s   --leak-check=yes test/testlibnbuss/testlibnbuss
	// it seems that EPOLLRDHUP EPOLLHUP events are lost, missed(???) or not delivered (?!?)
	// or something else catches the events (?!?!??!?) or some kind of wrong interaction between gtest and valgring (!?!?!?)

	struct timespec ts1, ts2;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts1);
	long dt = 0;

	// spin... consider using a condition variable
	int c = 0;
	while (uss.getActiveConnections() > 0) {
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);
		dt = ((ts2.tv_sec - ts1.tv_sec) * 1000000000L + ts2.tv_nsec) - ts1.tv_nsec;

		c++;

		if (dt > 2000000) // 2 milliseconds
			break;
	}

	if (dt > 0) {
		TEST_LOG(info) << "dt = " << dt << " nanoseconds - c=" << c;
	}

	TEST_LOG(info)
	<< "[server] calling terminate";
	uss.terminate();

	TEST_LOG(info)
	<< "[server] join thread";
	th.join();

	TEST_LOG(info)
	<< "test finished!";

	EXPECT_EQ(response.size(), 12);

}



