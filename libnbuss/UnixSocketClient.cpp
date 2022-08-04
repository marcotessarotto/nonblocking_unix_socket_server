#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "UnixSocketClient.h"
#include "IGenericServer.h"

#include "Logger.h"

namespace nbuss_client {

UnixSocketClient::UnixSocketClient(bool nonBlockingSocket) : sockname{}, BaseClient(nonBlockingSocket) {
}

UnixSocketClient::~UnixSocketClient() {
}

void UnixSocketClient::connect(const std::string &sockname) {
	struct sockaddr_un addr;
	int ret;

	if (sockname.empty()) {
		throw std::invalid_argument("missing sockname");
	}

	this->sockname = sockname;


	// open socket in blocking mode
	if (nonBlockingSocket) {
		data_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK , 0);  //  SOCK_SEQPACKET  |
	} else {
		data_socket = socket(AF_UNIX, SOCK_STREAM , 0);  //  SOCK_SEQPACKET  | SOCK_NONBLOCK
	}


    if (data_socket == -1) {
        LIB_LOG(error) << "socket error: " << strerror(errno);
        throw std::runtime_error("socket error");
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));


    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockname.c_str(), sizeof(addr.sun_path) - 1);

    ret = ::connect (data_socket, (const struct sockaddr *) &addr,
                   sizeof(struct sockaddr_un));
    if (ret == -1) {
    	LIB_LOG(error) << "connect error: " << strerror(errno);
        throw std::runtime_error("connect error");
    }


}

ssize_t UnixSocketClient::write(const char * data, ssize_t data_size, int * _errno) noexcept {

	if (data_size <= 4096) {
		return BaseClient::write(data, data_size, _errno);
	}

	int res;

	if (nonBlockingSocket) {
		// set data_socket as blocking
		res = nbuss_server::IGenericServer::set_fd_non_blocking(data_socket, false);
		if (res == -1) {
			// cannot set socket to blocking, fail
			if (_errno != nullptr) *_errno = -1;
			return -1;
		}
	}

	int retval = BaseClient::write(data, data_size, _errno);

	if (nonBlockingSocket) {
		// set data_socket as non blocking
		res = nbuss_server::IGenericServer::set_fd_non_blocking(data_socket, true);
		if (res == -1) {
			// cannot set socket to non blocking, fail
			if (_errno != nullptr) *_errno = -1;
			return -1;
		}
	}

	return retval;

}



} /* namespace nbuss_client */
