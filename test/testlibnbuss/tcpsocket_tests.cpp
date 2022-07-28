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
 * single server instance, single client instance; using tcp sockets
 * buffer size: 12 bytes
 * tests:
 * TcpServer, TcpClient, ThreadDecorator
 */
TEST(NonblockingTcpSocketServerTest, TcpServerClientReadWriteTest) {

	try {

		const ssize_t bufferSize = 4096 * 16 * 8;

		TEST_LOG(info) << "***TcpServerClientReadWriteTest*** bufferSize=" << bufferSize << " " << __FILE__;

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


		std::vector<char> longBuffer(bufferSize);

		initialize1(longBuffer);

		// calculate crc16 of data we are going to send
		uint16_t clientCrc = crc.crc_16(
				reinterpret_cast<const unsigned char*>(longBuffer.data()),
				longBuffer.size());

//		std::string s = "test message";
//		std::vector<char> v(s.begin(), s.end());

		TEST_LOG(debug)	<< "[client] writing to socket";
		tc.write<char>(longBuffer);

		TEST_LOG(debug)	<< "[client] reading from socket";


		ssize_t total_bytes_read = 0;

		while (total_bytes_read < bufferSize) {
			// read server response
			auto response = tc.read(1024);

			TEST_LOG(info)	<< "[client] received data size: " << response.size();

			total_bytes_read += response.size();
		}


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

		EXPECT_EQ(total_bytes_read, bufferSize);

	} catch (const std::exception &e) {
		TEST_LOG(error)
		<< "exception: " << e.what();
	}

}


TEST(WorkQueueTest, TestTcpSocketWithWorkQueue) {


	const ssize_t bufferSize = 4096 * 16 * 16;

	TEST_LOG(info) << "***TestTcpSocketWithWorkQueue**  bufferSize=" << bufferSize << " " << __FILE__;


	TcpServer ts { 10001, "0.0.0.0", 10 };

	ThreadedServer2 threadedServer2(ts);

	WorkQueue workQueue(threadedServer2);

	std::function<void(WorkQueue *, int, job_type_t)> myListener =
	[&threadedServer2](WorkQueue *srv, int fd, enum job_type_t job_type) {

		//TEST_LOG(info) << "*** threadedServer2: " << &threadedServer2;

		switch (job_type) {
		case NEW_SOCKET:
			TEST_LOG(info)	<< "[lambda][myListener] NEW_SOCKET " << fd;
			break;
		case CLOSE_SOCKET:

			TEST_LOG(info)	<< "[lambda][myListener] CLOSE_SOCKET " << fd;

			srv->close(fd);

			break;
		case AVAILABLE_FOR_READ:
			TEST_LOG(info)	<< "[lambda][myListener] AVAILABLE_FOR_READ fd=" << fd;

			// read all data from socket
			auto data = IGenericServer::read(fd, 1024*16);

			TEST_LOG(info)	<< "[lambda][myListener] number of vectors returned: " << data.size();

			// implement an echo server
			int counter = 0;
			for (std::vector<char> item : data) {
				TEST_LOG(debug)	<< "[lambda][myListener] buffer " << counter << ": " << item.size() << " bytes";

				ThreadedServer2 * srv2 = reinterpret_cast<ThreadedServer2 *>(srv);

				TEST_LOG(info)	<< "[lambda][myListener] calling srv2->write " << item.size();
				threadedServer2.write<char>(fd, item);

				counter++;
			}

			TEST_LOG(trace)	<< "[lambda][myListener] write complete";

			break;


		}
	};


	// when start returns, server has started listening for incoming connections
	workQueue.start(myListener);


	// non blocking client connection
	TcpClient tc(true);

	TEST_LOG(info) << "[client] connect to server";
	tc.connect("0.0.0.0", 10001);

	//sleep(2);

	std::vector<char> longBuffer(bufferSize);

	initialize1(longBuffer);

	// calculate crc16 of data we are going to send
	uint16_t clientCrc = crc.crc_16(
			reinterpret_cast<const unsigned char*>(longBuffer.data()),
			longBuffer.size());

	TEST_LOG(info) << "[client] writing data to socket, data size=" << longBuffer.size();
	tc.write<char>(longBuffer);

	uint16_t clientCrc2 = CRC_START_16;

	size_t total_bytes_received = 0;

	while (total_bytes_received < bufferSize) {
		TEST_LOG(debug)	<< "[client] reading from socket";
		// read server response
		auto response = tc.read(1024);

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

	TEST_LOG(info) << "[server] crc16 of data sent by client = " << clientCrc;
	TEST_LOG(info) << "[client] crc16 of data received from server = " << clientCrc2;

	TEST_LOG(info) << "[client] closing socket";
	tc.close();

	//TEST_LOG(info) << "[server] long buffer crc = " << serverDataCrc16;

	TEST_LOG(info) << "ts.getActiveConnections() = " << ts.getActiveConnections();
	// spin... consider using a condition variable
	while (ts.getActiveConnections() > 0) {
		// TEST_LOG(info) << uss.getActiveConnections();

//		struct timespec t;
//
//		t.tv_sec = 0;  // seconds
//		t.tv_nsec = 100 * 1000 * 1000; // nanoseconds
//
//		nanosleep(&t, NULL);

	}

	TEST_LOG(info) << "[server] stopping server";
	workQueue.stop();

	TEST_LOG(info) << "test finished!";


	EXPECT_EQ(clientCrc2, clientCrc);

}
