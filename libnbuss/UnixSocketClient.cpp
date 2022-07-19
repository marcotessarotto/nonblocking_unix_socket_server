#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "UnixSocketClient.h"

#include "Logger.h"

namespace nbuss_client {

UnixSocketClient::UnixSocketClient(bool nonBlockingSocket) : sockname{}, BaseClient(nonBlockingSocket) {
}

UnixSocketClient::~UnixSocketClient() {
	close();
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
        //perror("socket");
        LIB_LOG(error) << "socket error: " << strerror(errno);
        throw std::runtime_error("socket error");
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));


    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockname.c_str(), sizeof(addr.sun_path) - 1);

    ret = ::connect (data_socket, (const struct sockaddr *) &addr,
                   sizeof(struct sockaddr_un));
    if (ret == -1) {
    	//perror("connect");
    	LIB_LOG(error) << "connect error: " << strerror(errno);
        throw std::runtime_error("connect error");
    }


}



} /* namespace nbuss_client */
