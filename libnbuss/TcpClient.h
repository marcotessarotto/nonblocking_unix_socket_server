#ifndef LIBNBUSS_TCPCLIENT_H_
#define LIBNBUSS_TCPCLIENT_H_

#include <iostream>
#include <vector>
#include <string>

namespace nbuss_server {

class TcpClient {

	/// tcp port
	unsigned int port;

	/// network hostname to connect to
	std::string hostname;

	/// socket descriptor
	int data_socket;

public:
	TcpClient();
	virtual ~TcpClient();

	void connect(const std::string &address, unsigned int port);

	void close();

	void write(std::vector<char> data);
	void write(std::string data);

	/**
	 * read synchronously from socket, up to buffer_size bytes
	 *
	 * @throws std::runtime_error in case of error on read syscall
	 */
	std::vector<char> read(int buffer_size);

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_TCPCLIENT_H_ */
