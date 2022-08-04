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
 * listener used by most tests
 * implementation of an echo server
 */
void listener_echo_server(IGenericServer *srv, int fd, enum job_type_t job_type) {

	switch (job_type) {
	case NEW_SOCKET:
		TEST_LOG(info)	<< "[listener_echo_server] NEW_SOCKET " << fd;

		break;
	case CLOSE_SOCKET:

		TEST_LOG(info)	<< "[listener_echo_server] CLOSE_SOCKET " << fd;

		srv->close(fd);

		break;
	case SOCKET_IS_CLOSED:
		TEST_LOG(trace)	<< "[listener_echo_server] SOCKET_IS_CLOSED " << fd;
		break;
	case AVAILABLE_FOR_WRITE:
		TEST_LOG(info)	<< "[listener_echo_server] AVAILABLE_FOR_WRITE fd=" << fd;

		// implementation of write buffer is in ThreadedServer2
		break;
	case AVAILABLE_FOR_READ_AND_WRITE:
	case AVAILABLE_FOR_READ:
		TEST_LOG(info)	<< "[listener_echo_server] AVAILABLE_FOR_READ fd=" << fd;

		// read all data from socket
		auto data = srv->read(fd, 4096);

		TEST_LOG(debug)
		<< "[listener_echo_server] number of vectors returned: " << data.size();

		int counter = 0;
		for (std::vector<char> item : data) {
			TEST_LOG(trace)
			<< "[listener_echo_server] buffer " << counter++ << ": "
					<< item.size() << " bytes";

			while (true) {
				int _errno;
				int res;

				res = srv->write(fd, item, &_errno);

				if (res >= 0)
					break;

				if (res == -1 && ((_errno == EAGAIN) || (_errno == EWOULDBLOCK))) {
					struct timespec ts { 0, 1000000 };

					nanosleep(&ts, NULL);
				} else {

					TEST_LOG(error)	<< "[listener_echo_server] calling terminate";

					terminate();
				}
			}

		}
		break;


	}

	TEST_LOG(debug)	<< "[listener_echo_server] returning - fd=" << fd;

}


void initialize1(std::vector<char> & buffer) {
	//longBuffer.assign(bufferSize, '*');
	// initialize longBuffer
	for (std::size_t i = 0; i < buffer.size(); ++i) {
		buffer[i] = i % 11;
	}

	//	for(auto it = std::begin(longBuffer); it != std::end(longBuffer); ++it) {
	//	    std::cout << *it << "\n";
	//	}
}


struct InternalSocketData {

};

static std::map<int, InternalSocketData> socket_data;

/**
 * implements a server which receives two 64 bit integers and sends back the sum
 */
void listener_sum_server(WorkQueue *srv, int fd, enum job_type_t job_type) {

	switch (job_type) {
	case NEW_SOCKET:
		TEST_LOG(info)	<< "[listener_sum_server] NEW_SOCKET " << fd;

		break;
	case CLOSE_SOCKET:

		TEST_LOG(info)	<< "[listener_sum_server] CLOSE_SOCKET " << fd;

		srv->close(fd);

		break;
	case SOCKET_IS_CLOSED:
		TEST_LOG(trace)	<< "[listener_sum_server] SOCKET_IS_CLOSED " << fd;
		break;
	case AVAILABLE_FOR_WRITE:
		TEST_LOG(info)	<< "[listener_sum_server] AVAILABLE_FOR_WRITE fd=" << fd;

		// implementation of write buffer is in ThreadedServer2
		break;
	case AVAILABLE_FOR_READ_AND_WRITE:
	case AVAILABLE_FOR_READ:
		TEST_LOG(info)	<< "[listener_sum_server] AVAILABLE_FOR_READ fd=" << fd;

		// get reference to server (instance of ThreadedServer2 class)
		auto &server = srv->getServer();

		// read data from socket
		auto d = server.read_one_buffer_t<long long>(fd, 2);

		auto op_a = d[0];
		auto op_b = d[1];

		auto result = op_a + op_b;

		int res;

		res = server.write(fd, reinterpret_cast<char*>(&result), sizeof(result));

		if (res == -1) {
			TEST_LOG(error) << "[listener_sum_server] server.write error: " << strerror(errno);
		}


		break;


	}

	TEST_LOG(debug)	<< "[listener_sum_server] returning - fd=" << fd;

}

