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

//using ::testing::Return;

using namespace std;
using namespace nbuss_server;
using namespace nbuss_client;

extern void init_log();

#define TEST_LOG(lvl) BOOST_LOG_TRIVIAL(lvl)

// https://stackoverflow.com/a/12149643/974287
std::vector<int> create_random_data(int arrays_size, const seed_seq& ) {
  std::random_device r;
  std::seed_seq      seed{r(), r(), r(), r(), r(), r(), r(), r()};
  std::mt19937       eng(seed); // a source of random data

  std::uniform_int_distribution<int> dist;
  std::vector<int> v(arrays_size);

  generate(begin(v), end(v), bind(dist, eng));
  return v;
}



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

	// init_log();

	//logging::core::get()->

	//sink.set_format("[" << boost::logging::level << "]" << " - " << boost::logging::date("%d/$m/$Y") << "," << boost::logging::timestamp << "," << boost::logging::message);

	// https://www.boost.org/doc/libs/1_79_0/libs/log/example/doc/tutorial_trivial.cpp

	// https://www.boost.org/doc/libs/1_79_0/libs/log/doc/html/index.html

	// debian packages: libboost-log1.74-dev
}

void NonblockingUnixSocketServerTest::TearDown() {
}


static Crc16 crc;
static uint16_t serverDataCrc16;
static bool calcCrc = false;

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
 * buffer size: 4096 * N bytes
 * tests:
 * UnixSocketServer, UnixSocketClient, ThreadedServer
 */
TEST_F(NonblockingUnixSocketServerTest, UnixSocketServerClientReadWriteLongBufferTest) {

	calcCrc = true;
	const ssize_t bufferSize = 4096 * 16;

	TEST_LOG(info)
	<< "***UnixSocketServerClientReadWriteLongBufferTest**  bufferSize=" << bufferSize << " " << __FILE__;

	//Crc16 crc;
	//uint16_t longBufferCrc;

	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadedServer threadedServer(uss);

	// ThreadedServer threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	// non blocking client connection
	UnixSocketClient usc(true);

	TEST_LOG(info) << "[client] connect to server";
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

	TEST_LOG(info) << "[client] writing to socket";
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

	TEST_LOG(info) << "[server] crc16 of data received from client = " << serverDataCrc16;
	TEST_LOG(info) << "[client] crc16 of data received from server = " << clientCrc2;

	TEST_LOG(info) << "[client] closing socket";
	usc.close();

	//TEST_LOG(info) << "[server] long buffer crc = " << serverDataCrc16;

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	TEST_LOG(info) << "test finished!";

	calcCrc = false;

	EXPECT_EQ(clientCrc2, serverDataCrc16);
	EXPECT_EQ(clientCrc2, clientCrc);
}

/**
 * 	one server instance,
 * 	loop N times, serially:
 * 	   create a client instance,
 * 	   connect,
 * 	   generate random data,
 * 	   write data,
 * 	   read data (the server echoes it),
 * 	   check that the received data is the same as the sent one, using crc16 to check equality
 */
TEST_F(NonblockingUnixSocketServerTest, UnixSocketServerMultipleClientsReadWriteTest) {

	TEST_LOG(info)
	<< "***UnixSocketServerMultipleClientsReadWriteTest** " << __FILE__;


	string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadedServer threadedServer(uss);

	// ThreadedServer threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	for (unsigned int i = 0; i < 10; i++) {

		Crc16 crc;

		UnixSocketClient usc;

		TEST_LOG(info) << "[client " << i << "] connect to server";
		usc.connect(socketName);

		/////
	    const auto input = std::initializer_list<std::uint32_t>{1,2,3,4,5,i};
	    const auto output_size = 256;

	    // using std version of seed_seq (std::seed_seq can be used to feed a random value generator)
	    std::seed_seq seq(input);
	    std::vector<std::uint8_t> inputBuffer(output_size);
	    seq.generate(inputBuffer.begin(), inputBuffer.end()); // fill v with seed values

		uint16_t clientCrc = crc.crc_16(reinterpret_cast<const unsigned char*>(inputBuffer.data()),
				inputBuffer.size()*sizeof(std::uint8_t));

		TEST_LOG(debug) << "[client] writing to socket";
		usc.write<std::uint8_t>(inputBuffer);

		TEST_LOG(debug) << "[client] reading from socket";
		// read server response
		auto response = usc.read(1024);

		uint16_t clientCrc2 = CRC_START_16;

		clientCrc2 = crc.update_crc_16(clientCrc2,
			reinterpret_cast<const unsigned char*>(response.data()),
			response.size());

		TEST_LOG(info) << "[client] received data size: " << response.size();

		TEST_LOG(info) << "crc16 of data: " << clientCrc;

		EXPECT_EQ(clientCrc, clientCrc2);



		TEST_LOG(info)
		<< "[client] closing socket";
		usc.close();



		EXPECT_EQ(response.size(), output_size);
	}

	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	TEST_LOG(info)
	<< "test finished!";

}

/**
 * 	one server instance,
 * 	create N threads which in parallel:
 * 	   create a client instance,
 * 	   connect,
 * 	   generate random data,
 * 	   write data,
 * 	   read data (the server echoes it),
 * 	   check that the received data is the same as the sent one, using crc16 to check equality
 */
TEST_F(NonblockingUnixSocketServerTest, UnixSocketServerMultipleThreadClientsReadWriteTest) {

	TEST_LOG(info)
	<< "***UnixSocketServerMultipleThreadClientsReadWriteTest** " << __FILE__;

	const string socketName = "/tmp/mysocket_test.sock";

	UnixSocketServer uss { socketName, 10 };

	ThreadedServer threadedServer(uss);

	// ThreadedServer threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	std::function<void(int id)> clientThread = [socketName] (unsigned int id) {
		TEST_LOG(info) << "thread " << id;


		Crc16 crc;

		UnixSocketClient usc;

		usc.connect(socketName);

		TEST_LOG(info) << "[client][ " << id << "] connect to server fd=" << usc.getFd();


		/////
	    const auto input = std::initializer_list<std::uint32_t>{1,2,3,4,5,id};
	    const auto output_size = 1024*256;

	    // using std version of seed_seq (std::seed_seq can be used to feed a random value generator)
	    std::seed_seq seq(input);
	    std::vector<std::uint8_t> inputBuffer(output_size);
	    seq.generate(inputBuffer.begin(), inputBuffer.end()); // fill v with seed values

		uint16_t clientCrc = crc.crc_16(reinterpret_cast<const unsigned char*>(inputBuffer.data()),
				inputBuffer.size()*sizeof(std::uint8_t));

		TEST_LOG(debug) << "[client][ " << id << "] writing to socket";
		usc.write<std::uint8_t>(inputBuffer);

		TEST_LOG(debug) << "[client][ " << id << "] reading from socket";

		ssize_t total_bytes_read = 0;
		uint16_t clientCrc2 = CRC_START_16;

		// client expects output_size bytes
		while (total_bytes_read < output_size) {
			// read server response
			auto response = usc.read(1024);

			TEST_LOG(debug) << "[client][ " << id << "] receiving " << response.size() << " bytes";

			clientCrc2 = crc.update_crc_16(clientCrc2,
				reinterpret_cast<const unsigned char*>(response.data()),
				response.size());

			total_bytes_read += response.size();
		}


		TEST_LOG(info) << "[client][ " << id << "] received data size: " << total_bytes_read;

		TEST_LOG(info) << "[client][ " << id << "] crc16 of received data: " << clientCrc2;

		EXPECT_EQ(clientCrc, clientCrc2);

		TEST_LOG(info)
		<< "[client][ " << id << "] closing socket";
		usc.close();


	};

	const int N = 10;
	std::thread th[N];

	TEST_LOG(info) << "starting " << N << " threads, each thread open a connection to server";

	for (int i = 0; i < N; i++){
		th[i] = std::thread{clientThread, i};
	}

	for (int i = 0; i < N; i++){
		th[i].join();
	}


	// spin... consider using a condition variable
	while (uss.getActiveConnections() > 0)
		;

	threadedServer.stop();

	TEST_LOG(info)
	<< "test finished!";
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

