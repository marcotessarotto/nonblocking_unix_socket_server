#ifndef LIBNBUSS_BASECLIENT_H_
#define LIBNBUSS_BASECLIENT_H_

#include <vector>

namespace nbuss_client {

class BaseClient {

protected:
	/// socket descriptor
	int data_socket;

	bool nonBlockingSocket;
public:
	BaseClient(bool nonBlockingSocket = false);
	virtual ~BaseClient();

	void close();

	void write(std::vector<char> data);
	void write(std::string data);

	/**
	 * read from socket, up to buffer_size bytes.
	 * if socket is set in blocking mode, read blocks waiting for data
	 * if socket is set in non blocking mode: read returns data or a buffer of size 0 if no data is available
	 *
	 * @throws std::runtime_error in case of error on read syscall
	 */
	std::vector<char> read(int buffer_size = 1024);
};

} /* namespace nbuss_client */

#endif /* LIBNBUSS_BASECLIENT_H_ */