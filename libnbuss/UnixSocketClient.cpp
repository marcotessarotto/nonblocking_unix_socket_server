#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "UnixSocketClient.h"


namespace nbuss_client {

UnixSocketClient::UnixSocketClient() : sockname{}, data_socket{-1} {

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


    data_socket = socket(AF_UNIX, SOCK_STREAM, 0);  //  SOCK_SEQPACKET
    if (data_socket == -1) {
        perror("socket");
        throw std::runtime_error("socket error");
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));


    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockname.c_str(), sizeof(addr.sun_path) - 1);

    ret = ::connect (data_socket, (const struct sockaddr *) &addr,
                   sizeof(struct sockaddr_un));
    if (ret == -1) {
    	perror("connect");
        throw std::runtime_error("connect error");
    }


}


void UnixSocketClient::write(std::vector<char> data) {

	if (data_socket == -1) {
		throw std::invalid_argument("invalid socket descriptor");
	}

	int c;

	int data_size = data.size();
	char * p = data.data();

	// TODO: implement while
	c = ::write(data_socket, p, data_size);
}

void UnixSocketClient::write(std::string data) {
	throw std::runtime_error("not implemented yet! sorry :|");
}

void UnixSocketClient::close() {
	if (data_socket >= 0) {
		::close(data_socket);
		data_socket = -1;
	}
}


} /* namespace nbuss_client */
