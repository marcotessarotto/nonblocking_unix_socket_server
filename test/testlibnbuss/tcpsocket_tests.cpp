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




/**
 * single server instance, single client instance; using tcp sockets
 * buffer size: 12 bytes
 * tests:
 * TcpServer, TcpClient, ThreadDecorator
 */
TEST(NonblockingTcpSocketServerTest, TcpServerClientReadWriteTest) {

	try {

		TEST_LOG(info) << "***TcpServerClientReadWriteTest*** " << __FILE__;

		// create instance of tcp non blocking server

		TcpServer ts { 10001, "0.0.0.0", 10 };

		// listen method is called in another thread
		ThreadedServer threadedServer { ts };

		std::atomic_thread_fence(std::memory_order_release);

		TEST_LOG(info)	<< "[server] starting server";
		// when start returns, server has started listening for incoming connections
		threadedServer.start(listener_echo_server);

		TcpClient tc;

		TEST_LOG(info)
		<< "[client] connect to server";
		tc.connect("0.0.0.0", 10001);

		std::string s = "test message";
		std::vector<char> v(s.begin(), s.end());

		TEST_LOG(debug)	<< "[client] writing to socket";
		tc.write<char>(v);

		TEST_LOG(debug)	<< "[client] reading from socket";
		// read server response
		auto response = tc.read(1024);

		TEST_LOG(info)
		<< "[client] received data size: " << response.size();

		TEST_LOG(info)
		<< "[client] closing socket";
		tc.close();

		// spin... consider using a condition variable
		while (ts.getActiveConnections() > 0)
			;

		TEST_LOG(info)
		<< "[server] stopping server";
		threadedServer.stop();

		TEST_LOG(info)
		<< "[main] test finished!";

		EXPECT_EQ(response.size(), 12);

	} catch (const std::exception &e) {
		TEST_LOG(error)
		<< "exception: " << e.what();
	}

}
