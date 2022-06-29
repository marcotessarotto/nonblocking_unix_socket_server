#ifndef LIBNBUSS_UNIXSOCKETCLIENT_H_
#define LIBNBUSS_UNIXSOCKETCLIENT_H_

#include <iostream>
#include <vector>
#include <string>

namespace nbuss_client {

class UnixSocketClient {

	/// socket name on file system
	std::string sockname;

	/// socket descriptor
	int data_socket;

public:
	UnixSocketClient();
	virtual ~UnixSocketClient();

	void connect(const std::string &sockname);

	void close();

	void write(std::vector<char> data);
	void write(std::string data);

	std::vector<char> read();

};

} /* namespace nbuss_client */

#endif /* LIBNBUSS_UNIXSOCKETCLIENT_H_ */
