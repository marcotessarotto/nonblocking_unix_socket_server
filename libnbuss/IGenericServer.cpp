
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <syslog.h>
#include <errno.h>

#include "IGenericServer.h"
#include <iostream>

namespace nbuss_server {



//IGenericServer::IGenericServer() : stop_server{false}, is_listening{false}, backlog{0} {
//
//	std::cout << "IGenericServer::IGenericServer backlog=" << backlog << std::endl;
//
//}

IGenericServer::IGenericServer(unsigned int backlog) : stop_server{false}, is_listening{false}, backlog{backlog} {

	std::cout << "IGenericServer::IGenericServer backlog=" << backlog << std::endl;

	if (backlog <= 0) {
		throw std::invalid_argument("backlog must be > 0");
	}

}

IGenericServer::~IGenericServer() {

	std::cout << "IGenericServer::~IGenericServer()" << std::endl;

}

/**
 * wait for server starting to listen for incoming connections
 */
void IGenericServer::waitForServerReady() {

	std::cout << "waitForServerReady is_listening=" << is_listening << std::endl;
	std::unique_lock<std::mutex> lk(mtx);
	while (!is_listening)
		cv.wait(lk);

	lk.unlock();
}

/**
 * terminate server instance
 */
void IGenericServer::terminate() {

	std::cout << "IGenericServer::terminate()" << std::endl;

	stop_server.store(true);

	// send a char to the pipe; on the other side, the listening thread listens with epoll.
	// this will wake up the thread (if sleeping) and then it will check for termination flag
	commandPipe.write('.');

	// wait for server thread to stop listening for incoming connections
	std::unique_lock<std::mutex> lk(mtx);
	while (is_listening)
		cv.wait(lk);
	lk.unlock();

	std::cout << "IGenericServer::terminate() finished" << std::endl;
}


int IGenericServer::setFdNonBlocking(int fd) {
	int res;

	res = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (res == -1) {
		syslog(LOG_ERR,"fcntl - error setting O_NONBLOCK");
	}

	return res;
}

int IGenericServer::write(int fd, std::vector<char> buffer) {
	// TODO: manage non blocking write calls
	// possible solution: implement a write queue

	char * p;
	int c;

	p = buffer.data();
	int buffer_size = buffer.size();

	c = ::write(fd, p, buffer_size);

	if (c == -1) {
		perror("read");
	} else {
		std::cout << "write: " << c << " bytes have been written to socket" << std::endl;
	}

	return c;
}


/**
 * read all data available on socket and return it as a vector of vector<char>
 *
 */
std::vector<std::vector<char>> IGenericServer::read(int fd) {
	std::vector<std::vector<char>> result;

	//int counter = 0;
	while (true) {
		std::vector<char> buffer(1024);

		int c;
		char * p;

		p = buffer.data();
		int buffer_size = buffer.capacity();

		//std::cout << "read #" << counter++ << std::endl;

		// read from non blocking socket
		c = ::read(fd, p, buffer_size);

		// no data available to read
		if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			break;
		}

		if (c == -1) {
			// error returned by read syscall
			perror("read");
			break;
		} else if (c == 0) {
			// eof
			break;
		} else {
			// data has been read
			buffer.resize(c);

			// std::cout << "adding buffer to result" << std::endl;

			result.push_back(std::move(buffer));
		}
	}

	return result;
}

void IGenericServer::closeSockets() {
	// close listening socket and epoll socket
	listen_sock.close();
	epollfd.close();

	std::unique_lock<std::mutex> lk(mtx);
	is_listening = false;
	lk.unlock();

	std::cout << "listen cleanup finished" << std::endl;

	cv.notify_one();
}

}
