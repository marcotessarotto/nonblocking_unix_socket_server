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
    data_socket = socket(AF_UNIX, SOCK_STREAM , 0);  //  SOCK_SEQPACKET  | SOCK_NONBLOCK
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

	// TODO: if socket is in non-blocking mode, buffer could be full
	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		// can this happen? yes, because client socket is in non blocking mode
		std::cout << "[UnixSocketClient::writ] write: errno == EAGAIN || errno == EWOULDBLOCK " << std::endl;
	} else if (c == -1) {
		perror("write");
		throw std::runtime_error("write error");
	} else {
		std::cout << "[UnixSocketClient::writ] write returns: " << c << std::endl;
	}
}

void UnixSocketClient::write(std::string data) {
	if (data_socket == -1) {
		throw std::invalid_argument("invalid socket descriptor");
	}


	int c;

	int data_size = data.size();
	const char * p = data.c_str();

	// TODO: implement while
	c = ::write(data_socket, p, data_size);

	if (c == -1) {
		perror("write");
		throw std::runtime_error("write error");
	}
}

std::vector<char> UnixSocketClient::read(int buffer_size) {
	//return nbuss_server::UnixSocketServer::read(data_socket);

	if (buffer_size <= 0) {
		throw std::invalid_argument("invalid buffer_size");
	}

	std::vector<char> buffer(buffer_size);

	int c;
	char * p;

	p = buffer.data();

	// read from blocking socket
	c = ::read(data_socket, p, buffer_size);

//	// no data available to read
//	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
//		break;
//	}

	if (c == -1) {
		// error returned by read syscall
		perror("read");
		throw std::runtime_error("read error");
	}
//	else if (c == 0) {
//		// eof
//	} else {
//		// data has been read
//	}

	buffer.resize(c);

	return buffer;
}

void UnixSocketClient::close() {
	if (data_socket >= 0) {
		::close(data_socket);
		data_socket = -1;
	}
}


} /* namespace nbuss_client */
