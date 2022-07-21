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
		break;
	case AVAILABLE_FOR_READ:
		TEST_LOG(info)	<< "[listener_echo_server] AVAILABLE_FOR_READ fd=" << fd;

		// read all data from socket
		auto data = IGenericServer::read(fd, 256);

		TEST_LOG(debug)
		<< "[listener_echo_server] number of vectors returned: " << data.size();

		int counter = 0;
		for (std::vector<char> item : data) {
			TEST_LOG(trace)
			<< "[listener_echo_server] buffer " << counter++ << ": "
					<< item.size() << " bytes";



			// TODO: if write returns -1, wait 1 ms and retry
			while (IGenericServer::write(fd, item) == -1) {
				struct timespec ts { 0, 1000000 };

				nanosleep(&ts, NULL);
			}
		}
		break;


	}

	TEST_LOG(debug)	<< "[listener_echo_server] ending - fd=" << fd;

}



