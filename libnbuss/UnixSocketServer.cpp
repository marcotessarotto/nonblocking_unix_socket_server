#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <syslog.h>
#include <errno.h>

#include <exception>
#include <stdexcept>

#include "configuration.h"

#include "UnixSocketServer.h"

namespace nbuss_server {

void UnixSocketServer::init() {
	if (sockname.empty()) {
		throw std::invalid_argument("missing sockname");
	}

	if (backlog <= 0) {
		throw std::invalid_argument("backlog must be > 0");
	}
}

UnixSocketServer::UnixSocketServer(const std::string &sockname, unsigned int backlog) :
		sockname(sockname), backlog(backlog), stop_server(false) {
	std::cout << "UnixSocketServer::UnixSocketServer(const std::string &sockname, unsigned int backlog)" << std::endl;
	init();
}

UnixSocketServer::UnixSocketServer(std::string &&sockname, unsigned int backlog) :
		sockname(std::move(sockname)), backlog(backlog), stop_server(false) {
	std::cout << "UnixSocketServer::UnixSocketServer(std::string &&sockname, unsigned int backlog)" << std::endl;
//	std::cout << this->sockname << std::endl;

	init();
}

UnixSocketServer::~UnixSocketServer() {
	std::cout << "UnixSocketServer::~UnixSocketServer()" << std::endl;
}

/**
 * unlink socket, create and bind socket; returns server socket
 *
 */
int UnixSocketServer::open_unix_socket() {

	int sfd;
	struct sockaddr_un addr;

	unlink(sockname.c_str());

	syslog(LOG_DEBUG, "using non blocking socket\n");

	sfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0); // | SOCK_SEQPACKET

	if (sfd == -1) {
		syslog(LOG_ERR, "cannot open socket");

		//throw std::runtime_error("cannot open server socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	strncpy(addr.sun_path, sockname.c_str(), sizeof(addr.sun_path) - 1);

	if (bind(sfd, (struct sockaddr*) &addr, sizeof(struct sockaddr_un)) == -1) {
		syslog(LOG_ERR, "bind");
		return -1;
	}

	return sfd;
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


void UnixSocketServer::setup() {

	// listen_sock will be auto closed when returning
	listen_sock.fd = open_unix_socket();

	if (listen_sock.fd == -1) {
		throw std::runtime_error("cannot open server socket");
	}

}


void UnixSocketServer::listen(std::function<void(int, enum job_type_t)> callback_function) {

	RunOnReturn runOnReturn(this, [](UnixSocketServer * srv) { srv->listen_sock.close(); srv->epollfd.close(); });

	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];

	if (listen_sock.fd == -1) {
		throw std::runtime_error("server socket is not open");
	}

	// call man 2 listen
	if (::listen(listen_sock.fd, backlog) == -1) {
		syslog(LOG_ERR, "listen");
		throw std::runtime_error("error returned by listen syscall");
	}

	std::unique_lock<std::mutex> lk(m);
	is_listening = true;
	lk.unlock();

	syslog(LOG_DEBUG, "listen_sock=%d\n", listen_sock.fd);

	// note: epoll is linux specific
	// epollfd will be auto closed when returning
	epollfd.fd = epoll_create1(0);

	if (epollfd.fd == -1) {
		syslog(LOG_ERR, "epoll_create1 error (%s)", strerror(errno));

		throw std::runtime_error("epoll_create1 error");
	}

	syslog(LOG_DEBUG, "epoll_create1 ok: epollfd=%d\n", epollfd.fd);

	ev.events = EPOLLIN; // EPOLLIN: The associated file is available for read(2) operations.
	ev.data.fd = listen_sock.fd;
	if (epoll_ctl(epollfd.fd, EPOLL_CTL_ADD, listen_sock.fd, &ev) == -1) {
		syslog(LOG_ERR, "epoll_ctl: listen_sock error (%s)", strerror(errno));

		throw std::runtime_error("epoll_ctl error");
	}

	int n, nfds, conn_sock;

	while (!stop_server.load()) {

		// this syscall will block, waiting for events
		nfds = epoll_wait(epollfd.fd, events, MAX_EVENTS, -1);

		if (stop_server.load()) {
			return;
		}

		if (nfds == -1 && errno == EINTR) { // system call has been interrupted
			continue;
		} else if (nfds == -1) {

			syslog(LOG_ERR, "epoll_wait error (%s)", strerror(errno));

			throw std::runtime_error("epoll_wait error");
		}

		for (n = 0; n < nfds; ++n) {
			int fd;

			fd = events[n].data.fd;

			syslog(LOG_DEBUG,
					" n=%d events[n].events=%d events[n].data.fd=%d msg=%s", n,
					events[n].events, fd, interpret_event(events[n].events));

			if (fd == listen_sock.fd) {

				// new incoming connections
				if (events[n].events & EPOLLIN) {

					conn_sock = accept4(listen_sock.fd, NULL, NULL,	SOCK_NONBLOCK);

					//conn_sock = accept(listen_sock.fd, NULL, NULL);

					if (conn_sock == -1) {
						if (stop_server.load()) {
							return;
						}
						syslog(LOG_ERR, "accept error (%s)", strerror(errno));

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
							return;
						}
						syslog(LOG_ERR, "epoll_ctl error (%s)",
								strerror(errno));
						throw std::runtime_error("epoll_ctl error");
					}

					syslog(LOG_DEBUG, "accept ok, fd=%d\n", conn_sock);
				}

			} else {

				if ((events[n].events & EPOLLIN)
						&& (events[n].events & EPOLLRDHUP)
						&& (events[n].events & EPOLLHUP)) {

					syslog(LOG_DEBUG, "fd=%d EPOLLIN EPOLLRDHUP EPOLLHUP", fd);

					callback_function(events[n].data.fd, CLOSE_SOCKET);

				} else if (events[n].events & EPOLLIN) {
					syslog(LOG_DEBUG, "fd=%d EPOLLIN", fd);

					callback_function(fd, DATA_REQUEST);

				} else if (events[n].events & EPOLLRDHUP) {
					syslog(LOG_DEBUG,
							" fd=%d EPOLLRDHUP: has been closed by remote peer",
							events[n].data.fd);
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

					callback_function(events[n].data.fd, CLOSE_SOCKET);

				} else if (events[n].events & EPOLLHUP) {
					syslog(LOG_DEBUG, " fd=%d EPOLLHUP\n", events[n].data.fd);

					callback_function(events[n].data.fd, CLOSE_SOCKET);

				} else if (events[n].events & EPOLLERR) {
					syslog(LOG_DEBUG, " fd=%d EPOLLERR\n", events[n].data.fd);
					// Error condition happened on the associated file descriptor.
					// This event is also reported for the write end of a pipe when the read end has been closed.

					callback_function(events[n].data.fd, CLOSE_SOCKET);
				}

			} // else

		} // for (n = 0; n < nfds; ++n)

	} // for (;;)

}

void UnixSocketServer::terminate() {
	stop_server.store(true);

	epollfd.close();
	listen_sock.close();
}

/**
 * read all data available on socket and return it as a vector of vector<char>
 *
 */
std::vector<std::vector<char>> UnixSocketServer::read(int fd) {
	std::vector<std::vector<char>> result;

	int counter = 0;
	while (true) {
		std::vector<char> buffer(1024);

		int c;
		char * p;

		p = buffer.data();
		int buffer_size = buffer.capacity();

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


int UnixSocketServer::write(int fd, std::vector<char> buffer) {
	// TODO: manage non blocking write calls

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

} /* namespace mt_cpp_server */
