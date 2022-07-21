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

#include <ThreadedServer.h>
#include <ThreadedServer2.h>
#include "UnixSocketClient.h"
#include "Crc16.h"

#include "Logger.h"

using namespace std;
using namespace nbuss_server;
using namespace nbuss_client;


#define TEST_LOG(lvl) BOOST_LOG_TRIVIAL(lvl)


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
	<< "[client] connect to server\n";
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	TEST_LOG(debug)	<< "[client] writing to socket\n";
	usc.write<char>(v);

	TEST_LOG(debug) << "[client] reading from socket\n";
	// read server response
	auto response = usc.read(1024);

	TEST_LOG(info)
	<< "[client] received data size: " << response.size();

	TEST_LOG(info)
	<< "[client] closing socket";
	usc.close();

	// TODO: valgrind block here; but the program alone works
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




