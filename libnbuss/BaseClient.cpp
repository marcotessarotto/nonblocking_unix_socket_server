#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <string>

#include "BaseClient.h"


namespace nbuss_client {

BaseClient::BaseClient(bool nonBlockingSocket) : nonBlockingSocket{nonBlockingSocket}, data_socket{-1} {

}

BaseClient::~BaseClient() {

}


void BaseClient::write(const char * data, ssize_t data_size) {
	if (data_socket == -1) {
		throw std::invalid_argument("invalid socket descriptor");
	}

	int c;

	//int data_size = data.size() * sizeof(T);
	const char * p = data;

	while (true) {
		c = ::write(data_socket, p, data_size);

		if (c > 0 && c < data_size) {
			p += c;
			data_size -= c;
		} else {
			break;
		}
	}

	// TODO: if socket is in non-blocking mode, buffer could be full
	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		// can this happen? yes, if client socket is in non blocking mode
		LIB_LOG(warning) << "[BaseClient::write] errno == EAGAIN || errno == EWOULDBLOCK";
	} else if (c == -1) {
		LIB_LOG(error) << "[BaseClient::write] write error " << strerror(errno);
		throw std::runtime_error("write error");
	}
}


//void BaseClient::write(std::vector<char> &data) {
//	if (data_socket == -1) {
//		throw std::invalid_argument("invalid socket descriptor");
//	}
//
//	int c;
//
//	int data_size = data.size() * sizeof(T);
//	char * p = data.data();
//
//	// TODO: implement while
//	c = ::write(data_socket, p, data_size);
//
//	// TODO: if socket is in non-blocking mode, buffer could be full
//	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
//		// can this happen? yes, because client socket is in non blocking mode
//	}
//
//	// TODO: check
//	if (c == -1) {
//		LIB_LOG(error) << "[BaseClient::write] write error " << strerror(errno);
//		throw std::runtime_error("write error");
//	}
//}

void BaseClient::write(std::string &data) {
	if (data_socket == -1) {
		throw std::invalid_argument("invalid socket descriptor");
	}

	int c;

	int data_size = data.size();
	const char * p = data.c_str();

	// TODO: implement while, when a partial write is done
	c = ::write(data_socket, p, data_size);

	if (c == -1) {
		LIB_LOG(error) << "[BaseClient::write] write error " << strerror(errno);
		throw std::runtime_error("write error");
	}
}


std::vector<char> BaseClient::read(int buffer_size) {
	if (buffer_size <= 0) {
		throw std::invalid_argument("invalid buffer_size");
	}

	std::vector<char> buffer(buffer_size);

	int c;
	char * p;

	p = buffer.data();

	// read from blocking socket
	c = ::read(data_socket, p, buffer_size);

//	std::cout << "[TcpClient::read] read returns: " << c << std::endl;

	// no data available to read
	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		LIB_LOG(error) << "[BaseClient::read] errno == EAGAIN || errno == EWOULDBLOCK";

		buffer.resize(0);

		return buffer;
	} else if (c == -1) {
		// error returned by read syscall
		LIB_LOG(error) << "[BaseClient::read] errno == " << strerror(errno);

		//perror("read");
		throw std::runtime_error("read error");
	}

	buffer.resize(c);

	return buffer;
}

void BaseClient::close() {
	LIB_LOG(debug) << "BaseClient::close " << data_socket;
	if (data_socket >= 0) {
		::close(data_socket);
		data_socket = -1;
	}
}

} /* namespace nbuss_client */
