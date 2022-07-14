#ifndef LIBNBUSS_TCPCLIENT_H_
#define LIBNBUSS_TCPCLIENT_H_

#include <iostream>
#include <vector>
#include <string>

namespace nbuss_server {

class TcpClient {

	unsigned int port;

	/// socket address
	std::string sockname;

	/// socket descriptor
	int data_socket;

public:
	TcpClient();
	virtual ~TcpClient();
};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_TCPCLIENT_H_ */
