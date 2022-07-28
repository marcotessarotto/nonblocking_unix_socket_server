#include <unistd.h>

#include <poll.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include "IGenericServer.h"
#include <iostream>
#include "configuration.h"

#include "Logger.h"

namespace nbuss_server {


IGenericServer::IGenericServer(unsigned int backlog) :
		stop_server{false},
		is_listening{false},
		socketList{},
		backlog{backlog},
		activeConnections{0} {

	LIB_LOG(info) << "IGenericServer::IGenericServer backlog=" << backlog;

	if (backlog <= 0) {
		throw std::invalid_argument("backlog must be > 0");
	}

}

IGenericServer::~IGenericServer() {

	LIB_LOG(info) << "IGenericServer::~IGenericServer()";

}

/**
 * wait for server starting to listen for incoming connections
 */
void IGenericServer::waitForServerReady() {
	std::unique_lock<std::mutex> lk(mtx);
	while (!is_listening)
		cv.wait(lk);
	lk.unlock();

	LIB_LOG(info) << "IGenericServer::waitForServerReady() server is ready";
}

/**
 * terminate server instance
 */
void IGenericServer::terminate() {

	std::unique_lock<std::mutex> lk(mtx);
	bool _is_listening = is_listening;
	lk.unlock();

	if (!_is_listening) {
		LIB_LOG(info) << "IGenericServer::terminate() : server has already stopped (due to exception?)";
		// server has already stopped i.e. throwing an exception
		goto end;
	}

	LIB_LOG(info) << "IGenericServer::terminate() is_listening=" << _is_listening;

	stop_server.store(true);

	// send a char to the pipe; on the other side, the listening thread listens with epoll.
	// this will wake up the thread (if sleeping) and then it will check for termination flag
	commandPipe.write('.');

	// wait for server thread to stop listening for incoming connections
	lk.lock();
	while (is_listening)
		cv.wait(lk);
	lk.unlock();

	LIB_LOG(info) << "IGenericServer::terminate() has completed";

end:
	// restore state so server can be started again
	stop_server.store(false);
	is_listening = false;
}


int IGenericServer::setFdNonBlocking(int fd) noexcept {
	int res;

	res = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (res == -1) {
		LIB_LOG(error) <<  "fcntl - error setting O_NONBLOCK " << strerror(errno);
	}

	return res;
}


/**
 * read all data available on socket and return it as a vector of vector<char>
 * readBufferSize is the size of each std::vector<char>
 *
 * compiler performs return value optimization (RVO)
 *
 * https://stackoverflow.com/questions/1092561/is-returning-a-stdlist-costly
 *
 * alternatives: return pointer to container; pass a reference to container as parameter;
 * or https://stackoverflow.com/a/1092572/974287
 */
std::vector<std::vector<char>> IGenericServer::read(int fd, size_t readBufferSize, int * _errno) noexcept {
	std::vector<std::vector<char>> result;

	while (true) {

		// skip creation of temporary object and copy of temp object (item 42)
		std::vector<char> &buffer = result.emplace_back(readBufferSize);

		//std::vector<char> buffer(readBufferSize);

		int c;
		char * p;

		p = buffer.data();
		int buffer_size = buffer.capacity();

		// LIB_LOG(info) << "read #" << counter++;

		// read from non blocking socket
		c = ::read(fd, p, buffer_size);

		if (_errno != nullptr) *_errno = errno;

		// no data available to read
		if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			result.pop_back();
			break;
		}

		if (c == -1) {
			// error returned by read syscall
			LIB_LOG(error) <<  "read error " << strerror(errno);

			//throw std::runtime_error(strerror(errno));
			break;
		} else if (c == 0) {
			// eof, should not happen when using non blocking sockets
			result.pop_back();
			break;
		} else {
			// data has been read
			buffer.resize(c);

			// LIB_LOG(info) << "adding buffer to result";

			//result.push_back(std::move(buffer));
		}
	}

	return result;
}

std::vector<char> IGenericServer::read_one(int fd, size_t buffer_size, int * _errno) noexcept {

	std::vector<char> buffer(buffer_size);

	ssize_t c;
	char * p;

	p = buffer.data();

	// read from socket
	c = ::read(fd, p, buffer_size);

	if (_errno != nullptr) *_errno = errno;

	if (c >= 0) {
		// ok we received data

		buffer.resize(c);
	} else if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		// no data available to read
		LIB_LOG(warning) << "[IGenericServer::read_one] errno == EAGAIN || errno == EWOULDBLOCK";

		buffer.resize(0);
	} else if (c == -1) {
		// some other error returned by read syscall
		LIB_LOG(error) << "[IGenericServer::read_one] errno: " << strerror(errno);

		buffer.resize(0);
	}

	return buffer;
}

void IGenericServer::closeSockets() {
	// close listening socket and epoll socket
	listen_sock.close();
	epollfd.close();

	std::unique_lock<std::mutex> lk(mtx);
	is_listening = false;
	lk.unlock();

	LIB_LOG(info) << "listen cleanup finished";

	cv.notify_one();
}

// this works if only one thread calls this method
static const char* interpret_event(int event) {
	static char buffer[256];

	buffer[0] = 0;

	if (event & EPOLLIN)
		strcat(buffer, "EPOLLIN ");
	if (event & EPOLLOUT)
		strcat(buffer, "EPOLLOUT ");
	if (event & EPOLLRDHUP)
		strcat(buffer, "EPOLLRDHUP ");
	if (event & EPOLLHUP)
		strcat(buffer, "EPOLLHUP ");
	if (event & EPOLLERR)
		strcat(buffer, "EPOLLERR ");
	if (event & EPOLLPRI)
		strcat(buffer, "EPOLLPRI ");

	return buffer;
}

void IGenericServer::listen(std::function<void(IGenericServer *,int, enum job_type_t)> callback_function) {

	LIB_LOG(trace) << "IGenericServer::listen";

	if (listen_sock.fd == -1) {
		throw std::runtime_error("server socket is not open");
	}

	// TODO: keep a list of open sockets, so that we can close them when listen terminates

	// this lambda will be run before returning from listen function
	RunOnReturn runOnReturn([this]() {
		LIB_LOG(info) << "[IGenericServer] listen cleanup";

		this->closeSockets();

	});

	// initialization added after check with:
	// valgrind -s --leak-check=yes default/main/nbuss_server
	struct epoll_event ev = {0};
	struct epoll_event events[MAX_EVENTS];

	// start listening for incoming connections
	if (::listen(listen_sock.fd, backlog) == -1) {
		//syslog(LOG_ERR, "listen");
		LIB_LOG(error) << "listen error: " << strerror(errno);
		throw std::runtime_error("[IGenericServer] error returned by listen syscall");
	}

	// change state and notify that we are now listening for incoming connections
	std::unique_lock<std::mutex> lk(mtx);
	is_listening = true;
	lk.unlock();
	cv.notify_one();

	LIB_LOG(info) << "[IGenericServer] listen_sock=" << listen_sock.fd;


	// note: epoll is linux specific
	// epollfd will be auto closed when returning
	epollfd.fd = epoll_create1(0);

	if (epollfd.fd == -1) {
		LIB_LOG(error) << "[IGenericServer] epoll_create1 error" << strerror(errno);

		throw std::runtime_error("epoll_create1 error");
	}

	LIB_LOG(debug) << "[IGenericServer] epoll_create1 ok: epollfd=" << epollfd.fd;

	// monitor read side of pipe
	ev.events = EPOLLIN; // EPOLLIN: The associated file is available for read(2) operations.
	ev.data.fd = commandPipe.getReadFd();
	if (epoll_ctl(epollfd.fd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
		LIB_LOG(error) << "[IGenericServer] epoll_ctl: listen_sock error " << strerror(errno);

		throw std::runtime_error("epoll_ctl error");
	}

	ev.events = EPOLLIN; // EPOLLIN: The associated file is available for read(2) operations.
	ev.data.fd = listen_sock.fd;
	if (epoll_ctl(epollfd.fd, EPOLL_CTL_ADD, listen_sock.fd, &ev) == -1) {
		//syslog(LOG_ERR, "[IGenericServer] epoll_ctl: listen_sock error (%s)", strerror(errno));
		LIB_LOG(error) << "[IGenericServer] epoll_ctl: listen_sock error " << strerror(errno);

		throw std::runtime_error("epoll_ctl error");
	}

	int n, nfds, conn_sock;


	while (!stop_server.load()) {

		LIB_LOG(trace) << "[IGenericServer] before epoll_wait";
		// this syscall will block, waiting for events
		nfds = epoll_wait(epollfd.fd, events, MAX_EVENTS, -1);

		// check if server must stop
		if (stop_server.load()) {
			LIB_LOG(info) << "[IGenericServer] server must stop";
			return;
		}

		if (nfds == -1 && errno == EINTR) { // epoll_wait system call has been interrupted
			continue;
		} else if (nfds == -1) {

			LIB_LOG(error) << "[IGenericServer] epoll_wait error " << strerror(errno);

			throw std::runtime_error("epoll_wait error");
		}

		for (n = 0; n < nfds; ++n) {
			int fd;

			fd = events[n].data.fd;

			LIB_LOG(info) << "[IGenericServer] n=" << n << " events[n].data.fd=" << fd << " " << interpret_event(events[n].events);

			if (fd == listen_sock.fd) {

				// new incoming connections
				if (events[n].events & EPOLLIN) {

					conn_sock = accept4(listen_sock.fd, NULL, NULL,	SOCK_NONBLOCK);

					if (conn_sock == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
						// no pending connections in queue
						continue;
					} else if (conn_sock == -1) {

						LIB_LOG(error) << "[IGenericServer] accept error " << strerror(errno);

						throw std::runtime_error("accept error");
					}

					// test: introducing a delay (1 s) between accept4 and epoll_clt(EPOLL_CTL_ADD),
					// then the first EPOLLIN event is delivered correctly (no events are lost)
					// sleep(1);

					// EPOLLRDHUP: Stream  socket peer closed connection, or shut down writing half of connection.
					// EPOLLHUP: Hang up happened on the associated file descriptor.
					// EPOLLET: Requests EDGE-TRIGGERED notification  for the associated file descriptor.
					//          The default behavior for epoll is level-triggered.
					// EPOLLONESHOT: Requests one-shot notification for the associated file
					// descriptor.  This means that after an event notified for
					// the file descriptor by epoll_wait(2), the file descriptor
					// is disabled in the interest list and no other events will
					// be reported by the epoll interface.  The user must call
					// epoll_ctl() with EPOLL_CTL_MOD to rearm the file descriptor with a new event mask.


					LIB_LOG(info) << "[IGenericServer] epoll_ctl EPOLL_CTL_ADD  fd=" << conn_sock;

					ev.events = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLRDHUP | EPOLLOUT;
					ev.data.fd = conn_sock;
					if (epoll_ctl(epollfd.fd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {

						LIB_LOG(error) << "[IGenericServer] epoll_ctl error: " << strerror(errno);

						throw std::runtime_error("epoll_ctl error");
					}

					activeConnections++;


					callback_function(this, conn_sock, NEW_SOCKET);

				}

			} else {

				/**
				 * behaviour:
				 *
				 * on EPOLLHUP || EPOLLRDHUP events,send CLOSE_SOCKET to callback
				 *
				 * EPOLLOUT (but no EPOLLRDHUP or EPOLLHUP or EPOLLERR) => AVAILABLE_FOR_WRITE
				 *
				 * EPOLLIN (but no EPOLLRDHUP or EPOLLHUP or EPOLLERR) => AVAILABLE_FOR_READ
				 *
				 * EPOLLERR => CLOSE_SOCKET
				 *
				 * examples:
				 *
				 * EPOLLIN EPOLLRDHUP EPOLLHUP => CLOSE_SOCKET
				 *
				 * EPOLLIN EPOLLOUT => AVAILABLE_FOR_READ, AVAILABLE_FOR_WRITE
				 */
//				bool IS_EPOLLRDHUP = events[n].events & EPOLLRDHUP;
//				bool IS_EPOLLHUP = events[n].events & EPOLLHUP;
//				bool IS_EPOLLERR = events[n].events & EPOLLERR;
//				bool IS_EPOLLOUT = events[n].events & EPOLLOUT;
//				bool IS_EPOLLIN = events[n].events & EPOLLIN;

				if ((events[n].events & EPOLLRDHUP)	|| (events[n].events & EPOLLHUP)) {

					LIB_LOG(info)  << "[IGenericServer][listen] "
							<< ((events[n].events & EPOLLRDHUP) ? "EPOLLRDHUP " : "")
							<< ((events[n].events & EPOLLHUP) ? "EPOLLHUP" : "")
							<<   " fd=" << fd;

					callback_function(this, events[n].data.fd, CLOSE_SOCKET);

					continue;
				} else if (events[n].events & EPOLLERR) {
					LIB_LOG(error) << "[IGenericServer][listen] EPOLLERR fd=" << fd;
					// Error condition happened on the associated file descriptor.
					// This event is also reported for the write end of a pipe when the read end has been closed.

					callback_function(this, events[n].data.fd, CLOSE_SOCKET);

					continue;
				}

				// EPOLLIN and EPOLLOUT events can arrive at the same time
				if ((events[n].events & EPOLLIN) && (events[n].events & EPOLLOUT) ) {
					LIB_LOG(trace)  << "[IGenericServer][listen] EPOLLIN EPOLLOUT fd=" << fd;

					callback_function(this, fd, AVAILABLE_FOR_READ_AND_WRITE);

				} else if (events[n].events & EPOLLOUT) {
					LIB_LOG(trace)  << "[IGenericServer][listen] EPOLLOUT fd=" << fd;

					callback_function(this, fd, AVAILABLE_FOR_WRITE);

					// no continue here; there can be a EPOLLIN event
				} else if (events[n].events & EPOLLIN) {
					LIB_LOG(info)  << "[IGenericServer][listen] EPOLLIN fd=" << fd;

					callback_function(this, fd, AVAILABLE_FOR_READ);
					continue;
				}

			} // else

		} // for (n = 0; n < nfds; ++n)

	} // for (;;)

	LIB_LOG(info)  << "[IGenericServer] listen: terminated";

}

void IGenericServer::remove_from_epoll(int fd, int * _errno) noexcept {

	if (fd == -1 || epollfd.fd == -1 || fd == epollfd.fd || fd == commandPipe.getReadFd() || fd == listen_sock.fd) {
		LIB_LOG(error) << "[IGenericServer][remove_from_epoll] wrong fd=" << fd;
		*_errno = -1;
		return;
	}

	int res = epoll_ctl(epollfd.fd, EPOLL_CTL_DEL, fd, NULL);

	if (_errno != nullptr) *_errno = errno;

	if (res == -1) {

		LIB_LOG(error) << "[IGenericServer]][remove_from_epoll] epoll_ctl error: " << strerror(errno);

		//throw std::runtime_error("epoll_ctl error");
	} else {
		LIB_LOG(info) << "[IGenericServer]][remove_from_epoll] OK removed fd=" << fd;
	}
}

void IGenericServer::close(int fd) noexcept {

	LIB_LOG(info)  << "IGenericServer::close " << fd;
	if (fd >= 0) {
		::close(fd);
		activeConnections--;
	}
}


int IGenericServer::getActiveConnections() noexcept {
	return activeConnections.load();
}

ssize_t IGenericServer::write(int fd, const char * data, ssize_t data_size, int * _errno) noexcept {
	if (fd == -1) {
		LIB_LOG(error) << "invalid socket descriptor";
		if (_errno != nullptr) *_errno = -1;
		//throw std::invalid_argument("invalid socket descriptor");
		return -1;
	}

	ssize_t c;

	const char * p = data;

	while (true) {
		c = ::write(fd, p, data_size);

		if (c > 0 && c < data_size) {
			p += c;
			data_size -= c;
		} else {
			if (c > 0) {
				p += c;
			}
			break;
		}
	}

	if (_errno != nullptr) *_errno = errno;

	// socket not available to write
	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		LIB_LOG(info) << "[IGenericServer::write] errno == EAGAIN || errno == EWOULDBLOCK";

		// example: write syscall is called two times, the first successful but partial,
		// the second returns -1 because it would block
		if ((ssize_t)(p - data) > 0) {
			return (ssize_t)(p - data);
		} else {
			return -1;
		}

	} else if (c == -1) {
		LIB_LOG(error) << "[IGenericServer::write] write error " << strerror(errno);
		//throw std::runtime_error("write error");
		return -1;
	}

	//LIB_LOG(trace) << "[IGenericServer::write] p-data " << (p - data);

	// return number of bytes written
	return (ssize_t)(p - data);
}


}
