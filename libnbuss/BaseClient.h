#ifndef LIBNBUSS_BASECLIENT_H_
#define LIBNBUSS_BASECLIENT_H_

#include <vector>

#include "Logger.h"

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

	// template functions cannot be virtual, because of vtable size
	template <class T>
	ssize_t write(const std::vector<T> &data, int * _errno = nullptr) {
		ssize_t data_size = data.size() * sizeof(T);
		const char * p =  reinterpret_cast<const char*>(data.data());

		LIB_LOG(debug) << "BaseClient::Write<> data_size = " << data_size << " sizeof(T)=" << sizeof(T);

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
	virtual ssize_t write(const char * data, ssize_t data_size, int * _errno = nullptr) noexcept;

	ssize_t write(const std::string &data, int * _errno = nullptr) noexcept;

	/**
	 * read from socket, up to buffer_size bytes.
	 * if socket is set in blocking mode, read blocks waiting for data
	 * if socket is set in non blocking mode: read returns data or a buffer of size 0 if no data is available
	 *
	 * @throws std::runtime_error in case of error on read syscall
	 */
	std::vector<char> read(int buffer_size = 1024);

	int getFd() { return data_socket; }
};

} /* namespace nbuss_client */

#endif /* LIBNBUSS_BASECLIENT_H_ */
