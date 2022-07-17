#ifndef LIBNBUSS_UNIXSOCKETCLIENT_H_
#define LIBNBUSS_UNIXSOCKETCLIENT_H_

#include <iostream>
#include <vector>
#include <string>

#include "BaseClient.h"

namespace nbuss_client {

class UnixSocketClient : public BaseClient {

	/// socket name on file system
	std::string sockname;

	/// socket descriptor
	//int data_socket;

	//bool nonBlockingSocket;

public:
	UnixSocketClient(bool nonBlockingSocket = false);
	virtual ~UnixSocketClient();


	void connect(const std::string &sockname);

//	void close();
//
//	void write(std::vector<char> data);
//	void write(std::string data);

	/**
	 * read from socket, up to buffer_size bytes.
	 * if socket is set in blocking mode, read blocks waiting for data
	 * if socket is set in non blocking mode: read returns data or a buffer of size 0 if no data is available
	 *
	 * @throws std::runtime_error in case of error on read syscall
	 */
//	std::vector<char> read(int buffer_size);

};

} /* namespace nbuss_client */

#endif /* LIBNBUSS_UNIXSOCKETCLIENT_H_ */
