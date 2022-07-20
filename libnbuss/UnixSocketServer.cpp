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

#include "Logger.h"

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
	LIB_LOG(info) << "UnixSocketServer::UnixSocketServer(const std::string &sockname, unsigned int backlog)";
	checkParameters();

	setup();
}

UnixSocketServer::UnixSocketServer(const std::string &&sockname, unsigned int backlog) :
		sockname(std::move(sockname)), IGenericServer(backlog) {
	LIB_LOG(info) << "UnixSocketServer::UnixSocketServer(std::string &&sockname, unsigned int backlog)";
	checkParameters();

	setup();
}

UnixSocketServer::~UnixSocketServer() {
	LIB_LOG(info) << "UnixSocketServer::~UnixSocketServer()";
}

/**
 * unlink socket file on file system, create and bind socket;
 * return server socket if successful else return -1 on error
 *
 */
int UnixSocketServer::open_unix_socket() {

	int sfd;
	struct sockaddr_un addr;

	unlink(sockname.c_str());

	sfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0); // | SOCK_SEQPACKET

	if (sfd == -1) {
		LIB_LOG(error) << "cannot open socket " << strerror(errno);

		//throw std::runtime_error("cannot open server socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	strncpy(addr.sun_path, sockname.c_str(), sizeof(addr.sun_path) - 1);

	if (bind(sfd, (struct sockaddr*) &addr, sizeof(struct sockaddr_un)) == -1) {
		LIB_LOG(error) << "bind error " << strerror(errno);
		return -1;
	}

	return sfd;
}



void UnixSocketServer::setup() {
	// listen_sock will be auto closed when returning from listen
	listen_sock.fd = open_unix_socket();

	if (listen_sock.fd == -1) {
		throw std::runtime_error("cannot open server socket");
	}
}





} /* namespace nbuss_server */
