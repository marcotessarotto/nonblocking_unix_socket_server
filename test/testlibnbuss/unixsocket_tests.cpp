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

static Crc16 crc;
static uint16_t serverDataCrc16;
static bool calcCrc = false;


/**
 * check that server instance throws an exception when provided with invalid parameters
 */
TEST_F(NonblockingUnixSocketServerTest, CheckConstructorException) {

	TEST_LOG(info)
	<< "***CheckConstructorException**";

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
		// TODO: check if there are buffers to write to this socket
		break;
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

	TEST_LOG(info)
	<< "***UnixSocketServerClientReadWriteTest**";

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadedServer threadedServer(uss);

	// ThreadedServer threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	UnixSocketClient usc;

	TEST_LOG(info)
	<< "[client] connect to server\n";
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	TEST_LOG(debug) << "[client] writing to socket\n";
	usc.write<char>(v);

	TEST_LOG(debug) << "[client] reading from socket\n";
	// read server response
	auto response = usc.read(1024);

	TEST_LOG(info)
	<< "[client] received data size: " << response.size();

	TEST_LOG(info)
	<< "[client] closing socket";
	usc.close();

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	TEST_LOG(info)
	<< "test finished!";

	EXPECT_EQ(response.size(), 12);
}


