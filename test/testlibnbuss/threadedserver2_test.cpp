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

/**
 * single server instance, single client instance; using unix sockets
 * buffer size: 4096 * N bytes
 * tests:
 * UnixSocketServer, UnixSocketClient, ThreadedServer2
 */
TEST(ThreadedServer2Test, TestUnixSocketThreadedServer2) {

	const ssize_t bufferSize = 4096 * 16 * 1;

	TEST_LOG(info) << "***TestUnixSocketThreadedServer2**  bufferSize=" << bufferSize << " " << __FILE__;


	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadedServer2 threadedServer2(uss);

	// ThreadedServer2 threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));


	std::function<void(IGenericServer *, int, job_type_t)> myListener =
	[&threadedServer2](IGenericServer *srv, int fd, enum job_type_t job_type) {

		static int counter = 0;

		//TEST_LOG(info) << "*** threadedServer2: " << &threadedServer2;

		switch (job_type) {
		case CLOSE_SOCKET:

			TEST_LOG(info)	<< "[lambda][myListener] CLOSE_SOCKET " << fd;

			threadedServer2.close(fd);

			break;
		case AVAILABLE_FOR_READ:
			TEST_LOG(info)	<< "[lambda][myListener] AVAILABLE_FOR_READ fd=" << fd;

			while (true) {
				int _errno;
				// read all data from socket
				auto data = srv->read(fd, 1024*128, &_errno);

				if ((_errno == EAGAIN || _errno == EWOULDBLOCK) && data.size() == 0) {
					if (counter++ < 20)
						TEST_LOG(info) << "errno and data.size() == 0";
					break;
				}

				if (counter < 20)
				   TEST_LOG(trace)	<< "[lambda][myListener] number of vectors returned: " << data.size();

				// implement an echo server
				int counter = 0;
				for (std::vector<char> item : data) {
					TEST_LOG(trace)	<< "[lambda][myListener] buffer " << counter++ << ": " << item.size() << " bytes";

					ThreadedServer2 * srv2 = reinterpret_cast<ThreadedServer2 *>(srv);

					TEST_LOG(trace)	<< "[lambda][myListener] calling srv2->write " << item.size();
					threadedServer2.write<char>(fd, item);

				}
			}

			//TEST_LOG(info)	<< "[lambda][myListener] write complete";

			break;


		}
	};


	// when start returns, server has started listening for incoming connections
	threadedServer2.start(myListener);

	// non blocking client connection
	UnixSocketClient usc(false);

	TEST_LOG(info) << "[client] connect to server";
	usc.connect(socketName);

	std::vector<char> longBuffer(bufferSize);

	//longBuffer.assign(bufferSize, '*');
	// initialize longBuffer
	for (std::size_t i = 0; i < longBuffer.size(); ++i) {
		longBuffer[i] = i % 10;
	}


	// calculate crc16 of data we are going to send
	uint16_t clientCrc = crc.crc_16(
			reinterpret_cast<const unsigned char*>(longBuffer.data()),
			longBuffer.size());

	//TEST_LOG(info) << "[client] crc16 of data sent by client = " << clientCrc;

	TEST_LOG(info) << "[client] writing to socket";
	usc.write<char>(longBuffer);

	uint16_t clientCrc2 = CRC_START_16;

	size_t total_bytes_received = 0;

	while (total_bytes_received < bufferSize) {
		TEST_LOG(trace)	<< "[client] reading from socket";
		// read server response
		auto response = usc.read(1024*32);

		if (response.size() > 0)
			TEST_LOG(trace)	<< "[client] received data size: " << response.size();

		//TEST_LOG(info) << total_bytes_received;

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

	TEST_LOG(info) << "[server] crc16 of data received from client = " << clientCrc;
	TEST_LOG(info) << "[client] crc16 of data received from server = " << clientCrc2;

	TEST_LOG(info) << "[client] closing socket";
	usc.close();

	//TEST_LOG(info) << "[server] long buffer crc = " << serverDataCrc16;

	TEST_LOG(info) << "uss.getActiveConnections() = " << uss.get_active_connections();
	// spin... consider using a condition variable
	while (uss.get_active_connections() > 0) {
		// TEST_LOG(info) << uss.getActiveConnections();

//		struct timespec t;
//
//		t.tv_sec = 0;  // seconds
//		t.tv_nsec = 100 * 1000 * 1000; // nanoseconds
//
//		nanosleep(&t, NULL);

	}

	threadedServer2.stop();

	TEST_LOG(info) << "test finished!";

	//calcCrc = false;

	//EXPECT_EQ(clientCrc2, serverDataCrc16);
	EXPECT_EQ(clientCrc2, clientCrc);
}
