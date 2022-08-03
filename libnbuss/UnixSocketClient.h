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

public:
	UnixSocketClient(bool nonBlockingSocket = false);
	virtual ~UnixSocketClient();


	void connect(const std::string &sockname);

	template <class T>
	ssize_t write(const std::vector<T> &data, int * _errno = nullptr) {
		ssize_t data_size = data.size() * sizeof(T);
		const char * p =  reinterpret_cast<const char*>(data.data());

		LIB_LOG(debug) << "UnixSocketClient::Write<> data_size = " << data_size << " sizeof(T)=" << sizeof(T);

		return write(p, data_size, _errno);
	}

	/**
	 * call write syscall; data to write is at address data, length data_size.
	 * if _errno is specified, errno from syscall is written at *_errno
	 *
	 * if write_blocking_mode is true and socket is in non blocking mode, then the socket will be
	 * temporarily set in blocking mode for the write syscall, and when the syscall returns
	 * the socket will be set in non blocking mode again
	 */
	virtual ssize_t write(const char * data, ssize_t data_size, int * _errno = nullptr) noexcept override;

};

} /* namespace nbuss_client */

#endif /* LIBNBUSS_UNIXSOCKETCLIENT_H_ */
