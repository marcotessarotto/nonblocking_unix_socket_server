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
		//close(fd);
		srv->close(fd);

		break;
	case AVAILABLE_FOR_WRITE:
		TEST_LOG(info)	<< "[listener_echo_server] AVAILABLE_FOR_WRITE fd=" << fd;
		// TODO: check if there are buffers to write to this socket
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
