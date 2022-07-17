#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>

#include <netdb.h>

#include "TcpClient.h"

namespace nbuss_client {

TcpClient::TcpClient(bool nonBlockingSocket) : hostname{""}, port{0}, BaseClient(nonBlockingSocket) {
	std::cout << "TcpClient::TcpClient()\n";

}

TcpClient::~TcpClient() {
	close();
}

//struct sockaddr_in getipa(const char* hostname, int port){
//	struct sockaddr_in ipa;
//	ipa.sin_family = AF_INET;
//	ipa.sin_port = htons(port);
//
//	struct hostent* localhost = gethostbyname(hostname);
//	if(!localhost){
//		//perror("resolving localhost");
//
//		return ipa;
//	}
//
//	char* addr = localhost->h_addr_list[0];
//	memcpy(&ipa.sin_addr.s_addr, addr, sizeof addr);
//
//	return ipa;
//}


void TcpClient::connect(const std::string &hostname, unsigned int port) {
	//struct sockaddr_un addr;
	int ret;

	if (hostname.empty()) {
		throw std::invalid_argument("missing hostname");
	}

	if (port < 0) {
		throw std::invalid_argument("wrong port");
	}

	struct protoent* tcp;
	tcp = getprotobyname("tcp");

	data_socket = ::socket(PF_INET, SOCK_STREAM, tcp->p_proto);

	if (data_socket == -1) {
		throw std::runtime_error("socket: error");
	}


	struct sockaddr_in ipa;
	ipa.sin_family = AF_INET;
	ipa.sin_port = htons(port);

	struct hostent* localhost = gethostbyname(hostname.c_str());
	if(!localhost){
		throw std::runtime_error("gethostbyname: error resolving hostname");
	}

	char* addr = localhost->h_addr_list[0];
	memcpy(&ipa.sin_addr.s_addr, addr, sizeof addr);



	if(::connect(data_socket, (struct sockaddr*)&ipa, sizeof ipa) == -1){
		throw std::runtime_error("connect: error connecting to server");
	}


	this->hostname = hostname;

}


//void TcpClient::write(std::vector<char> data) {
//
//	if (data_socket == -1) {
//		throw std::invalid_argument("invalid socket descriptor");
//	}
//
//	int c;
//
//	int data_size = data.size();
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
//		perror("write");
//		throw std::runtime_error("write error");
//	}
//}
//
//void TcpClient::write(std::string data) {
//	if (data_socket == -1) {
//		throw std::invalid_argument("invalid socket descriptor");
//	}
//
//
//	int c;
//
//	int data_size = data.size();
//	const char * p = data.c_str();
//
//	// TODO: implement while
//	c = ::write(data_socket, p, data_size);
//
//	if (c == -1) {
//		perror("write");
//		throw std::runtime_error("write error");
//	}
//}
//
//std::vector<char> TcpClient::read(int buffer_size) {
//	//return nbuss_server::UnixSocketServer::read(data_socket);
//
//	if (buffer_size <= 0) {
//		throw std::invalid_argument("invalid buffer_size");
//	}
//
//	std::vector<char> buffer(buffer_size);
//
//	int c;
//	char * p;
//
//	p = buffer.data();
//
//	// read from blocking socket
//	c = ::read(data_socket, p, buffer_size);
//
////	std::cout << "[TcpClient::read] read returns: " << c << std::endl;
//
//	// no data available to read
//	if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
//		std::cout << "[TcpClient::read] errno == EAGAIN || errno == EWOULDBLOCK" << std::endl;
//
//		buffer.resize(0);
//
//		return buffer;
//	} else if (c == -1) {
//		// error returned by read syscall
//		std::cout << "[TcpClient::read] errno == " << errno << std::endl;
//
//		perror("read");
//		throw std::runtime_error("read error");
//	}
//
//	buffer.resize(c);
//
//	return buffer;
//}
//
//void TcpClient::close() {
//	if (data_socket >= 0) {
//		::close(data_socket);
//		data_socket = -1;
//	}
//}


} /* namespace nbuss_client */
