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
#include "Logger.h"

namespace nbuss_client {

TcpClient::TcpClient(bool nonBlockingSocket) : hostname{""}, port{0}, BaseClient(nonBlockingSocket) {
	LIB_LOG(info) << "TcpClient::TcpClient()";

}

TcpClient::~TcpClient() {
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

	if (port > 65535) {
		throw std::invalid_argument("wrong port");
	}

	this->hostname = hostname;

	struct protoent* tcp;
	tcp = getprotobyname("tcp");

	data_socket = ::socket(PF_INET, SOCK_STREAM, tcp->p_proto);

	if (data_socket == -1) {
		throw std::runtime_error("socket: error");
	}

	LIB_LOG(debug) << "TcpClient::connect *** 1";

	struct sockaddr_in ipa;
	ipa.sin_family = AF_INET;
	ipa.sin_port = htons(port);

	struct hostent* host = gethostbyname(this->hostname.c_str());
	if(!host){
		throw std::runtime_error("gethostbyname: error resolving hostname");
	}

	LIB_LOG(debug) << "TcpClient::connect *** 2";

	char* addr = host->h_addr_list[0];
	memcpy(&ipa.sin_addr.s_addr, addr, sizeof addr);

	LIB_LOG(debug) << "TcpClient::connect *** 3";

	if(::connect(data_socket, (struct sockaddr*)&ipa, sizeof ipa) == -1){
		throw std::runtime_error("connect: error connecting to server");
	}


}



} /* namespace nbuss_client */
