#include "configuration.h"

#include "UnixSocketServer.h"

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


namespace nbuss_server {

void UnixSocketServer::checkParameters() {
	if (sockname.empty()) {
		throw std::invalid_argument("missing sockname");
	}

	if (backlog <= 0) {
		throw std::invalid_argument("backlog must be > 0");
	}
}

UnixSocketServer::UnixSocketServer(const std::string &sockname, unsigned int backlog) :
		sockname{sockname}, IGenericServer(backlog) {
	std::cout << "UnixSocketServer::UnixSocketServer(const std::string &sockname, unsigned int backlog)" << std::endl;
	checkParameters();

	setup();
}

UnixSocketServer::UnixSocketServer(const std::string &&sockname, unsigned int backlog) :
		sockname(std::move(sockname)), IGenericServer(backlog) {
	std::cout << "UnixSocketServer::UnixSocketServer(std::string &&sockname, unsigned int backlog)" << std::endl;
	checkParameters();

	setup();
}

UnixSocketServer::~UnixSocketServer() {
	std::cout << "UnixSocketServer::~UnixSocketServer()" << std::endl;
}

/**
 * unlink socket, create and bind socket; return server socket else -1 on error
 *
 */
int UnixSocketServer::open_unix_socket() {

	int sfd;
	struct sockaddr_un addr;

	unlink(sockname.c_str());

	// syslog(LOG_DEBUG, "using non blocking socket\n");

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
		syslog(LOG_ERR, "bind error");
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

	// listen_sock will be auto closed when returning from listen
	listen_sock.fd = open_unix_socket();

	if (listen_sock.fd == -1) {
		throw std::runtime_error("cannot open server socket");
	}

}





} /* namespace nbuss_server */
