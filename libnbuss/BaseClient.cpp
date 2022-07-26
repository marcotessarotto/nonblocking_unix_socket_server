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


ssize_t BaseClient::write(const char * data, ssize_t data_size) {
	if (data_socket == -1) {
		throw std::invalid_argument("invalid socket descriptor");
	}

	ssize_t c;

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

	// if socket is in non-blocking mode, buffer could be full
	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		LIB_LOG(warning) << "[BaseClient::write] errno == EAGAIN || errno == EWOULDBLOCK";

		// example: write syscall is called two times, the first successful butbut partial,
		// the second returns -1 because it would block
		if ((ssize_t)(p - data) > 0) {
			return (ssize_t)(p - data);
		} else {
			return -1;
		}

	} else if (c == -1) {
		LIB_LOG(error) << "[BaseClient::write] write error " << strerror(errno);
		throw std::runtime_error("write error");
	}

	return (int)(p - data);
}


ssize_t BaseClient::write(const std::string &data) {
	ssize_t data_size = data.size() * sizeof(char);
	const char * p =  reinterpret_cast<const char*>(data.data());

	LIB_LOG(debug) << "BaseClient::Write data_size = " << data_size;

	return write(p, data_size);
}


std::vector<char> BaseClient::read(int buffer_size) {
	if (buffer_size <= 0) {
		throw std::invalid_argument("invalid buffer_size");
	}

	std::vector<char> buffer(buffer_size);

	ssize_t c;
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
