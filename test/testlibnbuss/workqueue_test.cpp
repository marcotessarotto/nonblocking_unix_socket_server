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

TEST(WorkQueueTest, TestUnixSocketWithWorkQueue) {


	const ssize_t bufferSize = 4096 * 16 * 1;

	TEST_LOG(info) << "***TestUnixSocketWithWorkQueue**  bufferSize=" << bufferSize << " " << __FILE__;


	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadedServer2 threadedServer2(uss);

	WorkQueue workQueue(threadedServer2);

	// ThreadedServer2 threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));


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
			auto data = threadedServer2.read(fd, 1024*16);

			TEST_LOG(trace)	<< "[lambda][myListener] number of vectors returned: " << data.size();

			// implement an echo server
			int counter = 0;
			for (std::vector<char> item : data) {
				TEST_LOG(trace)	<< "[lambda][myListener] buffer " << counter << ": " << item.size() << " bytes";

				ThreadedServer2 * srv2 = reinterpret_cast<ThreadedServer2 *>(srv);

				TEST_LOG(trace)	<< "[lambda][myListener] calling srv2->write " << item.size();
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
	UnixSocketClient usc(true);

	TEST_LOG(info) << "[client] connect to server";
	usc.connect(socketName);

	//sleep(2);

	std::vector<char> longBuffer(bufferSize);

	initialize1(longBuffer);

	// calculate crc16 of data we are going to send
	uint16_t clientCrc = crc.crc_16(
			reinterpret_cast<const unsigned char*>(longBuffer.data()),
			longBuffer.size());

	TEST_LOG(info) << "[client] writing data to socket, data size=" << longBuffer.size();
	usc.write<char>(longBuffer);

	uint16_t clientCrc2 = CRC_START_16;

	size_t total_bytes_received = 0;

	while (total_bytes_received < bufferSize) {
		TEST_LOG(debug)	<< "[client] reading from socket";
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

	TEST_LOG(info) << "[server] crc16 of data sent by client = " << clientCrc;
	TEST_LOG(info) << "[client] crc16 of data received from server = " << clientCrc2;

	TEST_LOG(info) << "[client] closing socket";
	usc.close();

	//TEST_LOG(info) << "[server] long buffer crc = " << serverDataCrc16;

	TEST_LOG(info) << "uss.getActiveConnections() = " << uss.getActiveConnections();
	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0) {
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

	//calcCrc = false;

	//EXPECT_EQ(clientCrc2, serverDataCrc16);
	EXPECT_EQ(clientCrc2, clientCrc);

}
