#include <unistd.h>

#include <poll.h>
#include <sys/epoll.h>
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
#include "configuration.h"

#include "Logger.h"

namespace nbuss_server {


IGenericServer::IGenericServer(unsigned int backlog) :
		stop_server{false},
		is_listening{false},
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

	LIB_LOG(info) << "IGenericServer::waitForServerReady() server is_listening=" << is_listening;
}

/**
 * terminate server instance
 */
void IGenericServer::terminate() {

	LIB_LOG(info) << "IGenericServer::terminate()";

	stop_server.store(true);

	// send a char to the pipe; on the other side, the listening thread listens with epoll.
	// this will wake up the thread (if sleeping) and then it will check for termination flag
	commandPipe.write('.');

	// wait for server thread to stop listening for incoming connections
	std::unique_lock<std::mutex> lk(mtx);
	while (is_listening)
		cv.wait(lk);
	lk.unlock();

	LIB_LOG(info) << "IGenericServer::terminate() has completed";

	// restore state so server can be started again
	stop_server.store(false);
	is_listening = false;
}


int IGenericServer::setFdNonBlocking(int fd) {
	int res;

	res = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (res == -1) {
		//syslog(LOG_ERR,"fcntl - error setting O_NONBLOCK");
		LIB_LOG(error) <<  "fcntl - error setting O_NONBLOCK " << strerror(errno);
	}

	return res;
}

//int IGenericServer::write(int fd, std::vector<char> &buffer) {
//	// TODO: manage non blocking write calls
//	// possible solution: implement a write queue
//
//	char * p;
//	int c;
//
//	p = buffer.data();
//	int buffer_size = buffer.size();
//
//	c = ::write(fd, p, buffer_size);
//
//	if (c == -1) {
//		LIB_LOG(error) <<  "write error " << strerror(errno);
//	} else {
//		LIB_LOG(trace)  << "[IGenericServer] write: " << c << " bytes have been written to socket";
//	}
//
//	return c;
//}


/**
 * read all data available on socket and return it as a vector of vector<char>
 *
 * compiler performs return value optimization (RVO)
 *
 * https://stackoverflow.com/questions/1092561/is-returning-a-stdlist-costly
 *
 * alternatives: return pointer to container; pass a reference to container as parameter;
 * or https://stackoverflow.com/a/1092572/974287
 */
std::vector<std::vector<char>> IGenericServer::read(int fd, size_t readBufferSize) {
	std::vector<std::vector<char>> result;

	//int counter = 0;
	while (true) {
		std::vector<char> buffer(readBufferSize);

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
			LIB_LOG(error) <<  "read error " << strerror(errno);
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

	LIB_LOG(info) << "IGenericServer::listen" << std::endl;

	if (listen_sock.fd == -1) {
		throw std::runtime_error("server socket is not open");
	}

	// TODO: keep a list of open sockets, so that we can close them when listen terminates

	// this lambda will be run before returning from listen function
	RunOnReturn runOnReturn([this]() {
		LIB_LOG(info) << "[IGenericServer] listen cleanup" << std::endl;

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

	//syslog(LOG_DEBUG, "[IGenericServer] listen_sock=%d\n", listen_sock.fd);
	LIB_LOG(info) << "[IGenericServer] listen_sock=" << listen_sock.fd;


	//std::cout << "IGenericServer::listen 1\n";
	// note: epoll is linux specific
	// epollfd will be auto closed when returning
	epollfd.fd = epoll_create1(0);

	if (epollfd.fd == -1) {
		//syslog(LOG_ERR, "[IGenericServer] epoll_create1 error (%s)", strerror(errno));
		LIB_LOG(error) << "[IGenericServer] epoll_create1 error" << strerror(errno);

		throw std::runtime_error("epoll_create1 error");
	}

	//syslog(LOG_DEBUG, "[IGenericServer] epoll_create1 ok: epollfd=%d\n", epollfd.fd);
	LIB_LOG(debug) << "[IGenericServer] epoll_create1 ok: epollfd=" << epollfd.fd;

	// monitor read side of pipe
	ev.events = EPOLLIN; // EPOLLIN: The associated file is available for read(2) operations.
	ev.data.fd = commandPipe.getReadFd();
	if (epoll_ctl(epollfd.fd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
		//syslog(LOG_ERR, "[IGenericServer] epoll_ctl: listen_sock error (%s)", strerror(errno));
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
			LIB_LOG(info) << "[IGenericServer] server is stopping";
			return;
		}

		if (nfds == -1 && errno == EINTR) { // system call has been interrupted
			continue;
		} else if (nfds == -1) {

			//syslog(LOG_ERR, "[IGenericServer] epoll_wait error (%s)", strerror(errno));
			LIB_LOG(error) << "[IGenericServer] epoll_wait error " << strerror(errno);

			throw std::runtime_error("epoll_wait error");
		}

		for (n = 0; n < nfds; ++n) {
			int fd;

			fd = events[n].data.fd;

//			syslog(LOG_DEBUG,
//					"[IGenericServer] n=%d events[n].events=%d events[n].data.fd=%d msg=%s", n,
//					events[n].events, fd, interpret_event(events[n].events));
			LIB_LOG(info) << "[IGenericServer] n=%d events[n].events=%d events[n].data.fd=" << fd << " " << interpret_event(events[n].events);

			if (fd == listen_sock.fd) {

				// new incoming connections
				if (events[n].events & EPOLLIN) {

					conn_sock = accept4(listen_sock.fd, NULL, NULL,	SOCK_NONBLOCK);

					if (conn_sock == -1) {
						if (stop_server.load()) {
							LIB_LOG(info) << "stop_server: true";
							return;
						}
						//syslog(LOG_ERR, "[IGenericServer] accept error (%s)", strerror(errno));
						LIB_LOG(error) << "[IGenericServer] accept error " << strerror(errno);

						throw std::runtime_error("accept error");
					}

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

					ev.events = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLRDHUP;
					ev.data.fd = conn_sock;
					if (epoll_ctl(epollfd.fd, EPOLL_CTL_ADD, conn_sock, &ev)
							== -1) {
						if (stop_server.load()) {
							LIB_LOG(info) << "stop_server: true";
							return;
						}
						//syslog(LOG_ERR, "[IGenericServer] epoll_ctl error (%s)",strerror(errno));
						LIB_LOG(error) << "[IGenericServer] epoll_ctl error: " << strerror(errno);

						throw std::runtime_error("epoll_ctl error");
					}

					activeConnections++;

					LIB_LOG(debug) << "[IGenericServer] accept ok, fd" << conn_sock;
					//syslog(LOG_DEBUG, "[IGenericServer] accept ok, fd=%d\n", conn_sock);
				}

			} else {

				if ((events[n].events & EPOLLIN)
						&& (events[n].events & EPOLLRDHUP)
						&& (events[n].events & EPOLLHUP)) {

					LIB_LOG(debug)  << "[IGenericServer] EPOLLIN EPOLLRDHUP EPOLLHUP fd=" << fd;
					//syslog(LOG_DEBUG, "[IGenericServer] fd=%d EPOLLIN EPOLLRDHUP EPOLLHUP", fd);

					callback_function(this, events[n].data.fd, CLOSE_SOCKET);

				} else if ((events[n].events & EPOLLIN)
						&& (events[n].events & EPOLLRDHUP)) {

					LIB_LOG(debug)  << "[IGenericServer] EPOLLIN EPOLLRDHUP fd=" << fd;
					// Stream  socket peer closed connection, or shut down writing half of connection.
					callback_function(this, events[n].data.fd, CLOSE_SOCKET);

				} else if (events[n].events & EPOLLIN) {
					LIB_LOG(debug)  << "[IGenericServer] EPOLLIN fd=" << fd;
					//syslog(LOG_DEBUG, "[IGenericServer] fd=%d EPOLLIN", fd);

					callback_function(this, fd, DATA_REQUEST);

				} else if (events[n].events & EPOLLRDHUP) {
					LIB_LOG(debug)  << "[IGenericServer] EPOLLRDHUP fd=" << fd;
					//syslog(LOG_DEBUG, "[IGenericServer] fd=%d EPOLLRDHUP: has been closed by remote peer", events[n].data.fd);
//					  from man 2 epoll_ctl
//					  EPOLLRDHUP: Stream socket peer closed connection, or shut down writing
//		              half of connection.  (This flag is especially useful for
//		              writing simple code to detect peer shutdown when using
//		              edge-triggered monitoring.)

//		              Hang up happened on the associated file descriptor.
//
//		              epoll_wait(2) will always wait for this event; it is not
//		              necessary to set it in events when calling epoll_ctl().
//
//		              Note that when reading from a channel such as a pipe or a
//		              stream socket, this event merely indicates that the peer
//		              closed its end of the channel.  Subsequent reads from the
//		              channel will return 0 (end of file) only after all
//		              outstanding data in the channel has been consumed.

					// what do we want to do?
					// there could be still data to be read, but we can write to socket (thus we can respond)

					callback_function(this, events[n].data.fd, CLOSE_SOCKET);

				} else if (events[n].events & EPOLLHUP) {
					LIB_LOG(debug) << "[IGenericServer] EPOLLHUP fd=" << fd;
					//syslog(LOG_DEBUG, "[IGenericServer] fd=%d EPOLLHUP\n", events[n].data.fd);

					callback_function(this, events[n].data.fd, CLOSE_SOCKET);

				} else if (events[n].events & EPOLLERR) {
					LIB_LOG(error) << "[IGenericServer] EPOLLERR fd=" << fd;
					//syslog(LOG_DEBUG, "[IGenericServer] fd=%d EPOLLERR\n", events[n].data.fd);
					// Error condition happened on the associated file descriptor.
					// This event is also reported for the write end of a pipe when the read end has been closed.

					callback_function(this, events[n].data.fd, CLOSE_SOCKET);
				}

			} // else

		} // for (n = 0; n < nfds; ++n)

	} // for (;;)

	LIB_LOG(info)  << "stop_server: " << stop_server.load();

}

void IGenericServer::close(int fd) {

	LIB_LOG(trace)  << "IGenericServer::close " << fd;
	if (fd >= 0) {
		::close(fd);
		activeConnections--;
	}
}

int IGenericServer::write(int fd, const char * data, ssize_t data_size) {
	if (fd == -1) {
		throw std::invalid_argument("invalid socket descriptor");
	}

	int c;
	int bytes_written = 0;

	const char * p = data;

	while (true) {
		c = ::write(fd, p, data_size);

		if (c > 0 && c < data_size) {
			p += c;
			data_size -= c;
		} else {
			break;
		}
	}

	// if socket is in non-blocking mode, buffer could be full
	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		// can this happen? yes, if client socket is in non blocking mode
		LIB_LOG(warning) << "[IGenericServer::write] errno == EAGAIN || errno == EWOULDBLOCK";

		// TODO: manage non blocking write calls
		// possible solution: implement a write queue

		return -1;
	} else if (c == -1) {
		LIB_LOG(error) << "[IGenericServer::write] write error " << strerror(errno);
		throw std::runtime_error("write error");
	}

	// return number of bytes written
	return (int)(p - data);
}


}
