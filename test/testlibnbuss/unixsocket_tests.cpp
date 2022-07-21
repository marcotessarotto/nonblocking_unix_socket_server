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



/**
 * single server instance, single client instance; using unix sockets
 * buffer size: 4096 * N bytes
 * tests:
 * UnixSocketServer, UnixSocketClient, ThreadedServer2
 */
TEST_F(NonblockingUnixSocketServerTest, UnixSocketServerClientReadWriteLongBufferDoubleThreadTest) {

	// TODO must fix ThreadedServer2
	return;

	calcCrc = true;
	const ssize_t bufferSize = 4096 * 16;

	TEST_LOG(info)
	<< "***UnixSocketServerClientReadWriteLongBufferDoubleThreadTest**  bufferSize=" << bufferSize;

	//Crc16 crc;
	//uint16_t longBufferCrc;

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadedServer2 threadedServer(uss);

	// ThreadedServer2 threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));


	std::function<void(IGenericServer *, int, job_type_t)> myListener =
	[&threadedServer](IGenericServer *srv, int fd, enum job_type_t job_type) {

		TEST_LOG(info) << "*** threadedServer: " << &threadedServer;

		switch (job_type) {
		case CLOSE_SOCKET:

			TEST_LOG(info)	<< "[server][myListener] CLOSE_SOCKET " << fd;
			//close(fd);
			srv->close(fd);

			break;
		case AVAILABLE_FOR_READ:
			TEST_LOG(info)	<< "[server][myListener] AVAILABLE_FOR_READ fd=" << fd;

			// read all data from socket
			auto data = IGenericServer::read(fd, 1024*16);

			TEST_LOG(info)	<< "[server][myListener] number of vectors returned: " << data.size();

			int counter = 0;
			for (std::vector<char> item : data) {
				TEST_LOG(info)	<< "[server][myListener] buffer " << counter++ << ": " << item.size() << " bytes";

//				if (calcCrc) {
//					serverDataCrc16 = crc.update_crc_16(serverDataCrc16,
//							reinterpret_cast<const unsigned char*>(item.data()),
//							item.size());
//				}

				ThreadedServer2 * srv2 = reinterpret_cast<ThreadedServer2 *>(srv);

				TEST_LOG(info)	<< "[server][myListener] calling srv2->write " << item.size();
				threadedServer.write<char>(fd, item);

//				// TODO: if write returns -1, copy remaining data
//				while (IGenericServer::write(fd, item) == -1) {
//					struct timespec ts { 0, 1000000 };
//
//					nanosleep(&ts, NULL);
//				}
			}

			TEST_LOG(info)	<< "[server][myListener] write complete";

			break;


		}
	};



	// when start returns, server has started listening for incoming connections
	threadedServer.start(myListener);

	// non blocking client connection
	UnixSocketClient usc(true);

	TEST_LOG(info) << "[client] connect to server\n";
	usc.connect(socketName);

//	std::string s = "test message";
//	std::vector<char> v(s.begin(), s.end());

	std::vector<char> longBuffer(bufferSize);

	//longBuffer.assign(bufferSize, '*');
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

	TEST_LOG(info) << "[client] crc16 of data sent by client = " << clientCrc;
	// crc16: 61937

	// reset CRC16
	serverDataCrc16 = CRC_START_16;

	TEST_LOG(info) << "[client] writing to socket\n";
	usc.write<char>(longBuffer);

	uint16_t clientCrc2 = CRC_START_16;

	size_t total_bytes_received = 0;

	while (total_bytes_received < bufferSize) {
		TEST_LOG(debug)	<< "[client] reading from socket\n";
		// read server response
		auto response = usc.read(1024);

		TEST_LOG(debug)	<< "[client] received data size: " << response.size();

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

	TEST_LOG(info) << "[server] crc16 of data received from client = " << serverDataCrc16;
	TEST_LOG(info) << "[client] crc16 of data received from server = " << clientCrc2;

	TEST_LOG(info) << "[client] closing socket";
	usc.close();

	TEST_LOG(info) << "[server] long buffer crc = " << serverDataCrc16;

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	TEST_LOG(info)
	<< "test finished!";

	calcCrc = false;

	EXPECT_EQ(clientCrc2, serverDataCrc16);
	EXPECT_EQ(clientCrc2, clientCrc);
}
