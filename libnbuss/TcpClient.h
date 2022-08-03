#ifndef LIBNBUSS_TCPCLIENT_H_
#define LIBNBUSS_TCPCLIENT_H_

#include <iostream>
#include <vector>
#include <string>

#include "BaseClient.h"

namespace nbuss_client {

class TcpClient : public BaseClient {

	/// tcp port
	unsigned int port;

	/// network hostname to connect to
	std::string hostname;

public:
	TcpClient(bool nonBlockingSocket = false);
	virtual ~TcpClient();

	void connect(const std::string &address, unsigned int port);

};

} /* namespace nbuss_client */

#endif /* LIBNBUSS_TCPCLIENT_H_ */
