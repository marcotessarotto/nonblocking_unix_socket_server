#ifndef IGENERICSERVER_H_
#define IGENERICSERVER_H_

#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include <map>
#include <set>
#include "FileDescriptor.h"
#include "UnixPipe.h"

namespace nbuss_server {

// TODO: consider moving to enum class
enum job_type_t {
	AVAILABLE_FOR_READ, // the associated file is available for read(2) operations.
	CLOSE_SOCKET, // socket must be closed (and work queue cleaned for subsequent operations on fd)
	AVAILABLE_FOR_WRITE, // the associated file is available for write(2) operations.
	NEW_SOCKET, // new connection
	AVAILABLE_FOR_READ_AND_WRITE, // both AVAILABLE_FOR_READ and AVAILABLE_FOR_WRITE
	SOCKET_IS_CLOSED // inform that socket has been closed by the server for some reason
};


/**
 * IGenericServer defines a class for a generic non blocking server.
 *
 * This allows to introduce different kind of servers i.e. non blocking unix socket server, non blocking TCP socket server ...
 *
 */
class IGenericServer {

protected:

	/// inner class for running a function when a method ends (see listen method of classes from IGenericServer)
	class RunOnReturn {
	public:
		std::function<void(void)> func;

		RunOnReturn(std::function<void(void)> func) : func{func} {
		}
		~RunOnReturn() {
			//std::cout << "~RunOnReturn" << std::endl;
			func();
		}
	};

	/// declared as fields so that RunOnReturn instance in listen method can operate on them
	FileDescriptor listen_sock{"listen_sock"};
	FileDescriptor epollfd{"epollfd"};

	/// set of file descriptors of incoming connections, kept updated by listen and used by closeSocket function
	std::set<int> incoming_conn_fd;
	/// mutex for synchronizing access to incoming_conn_fd
	// incoming_conn_fd is accessed by listen and close functions, which run in different threads
	std::mutex incoming_conn_fd_mtx;

	/// flag used to notify that server must stop listening and close sockets
	std::atomic<bool> stop_server;

	/// flag which signals that server is listening for incoming connections
	/// we use bool (and not a std::atomic) because we synchronize through a condition variable
	bool is_listening;

	/// mutex is used to synchronize access to is_listening
	std::mutex mtx;
	/// used to wait for is_listening to become false
	std::condition_variable cv;

	/// pipe used for signaling to listening thread that it must terminate
	UnixPipe commandPipe;

	/// backlog for listening server socket
	unsigned int backlog;

	std::atomic<int> activeConnections;

	/// list of open sockets
	std::vector<int> socketList;

	/// has there been I/O on socket?
	std::map<int, bool> is_socket_IO;
	std::mutex is_socket_IO_mutex;

	/// register that on fd has been performed IO
	void set_IO(int fd);

	/// register that on fd has not been performed IO
	void unset_IO(int fd);

	/// has been performed IO on fd?
	bool is_IO(int fd);

	/**
	 * close listening socket and epoll socket and set is_listening to false	 *
	 * invoked by listen function
	 */
	void closeSockets();

public:

	struct ListenEvent {
		void * srv; // pointer to server instance
		int fd;
		enum job_type_t job;
		uint32_t events;
	};

	IGenericServer(unsigned int backlog = 10);
	virtual ~IGenericServer();
	
	//using TServerCallback = std::function<void(IGenericServer *,int, enum job_type_t )>;

    /**
     * listen for incoming connections; on incoming data, call callback_function
     *
     * returns only when another thread calls terminate method
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are: socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	void listen(std::function<void(ListenEvent &&listen_event)> callback_function);


    /**
     * terminate server; when the method returns, the server has terminated operations
     *
     */
	void terminate();

	/**
	 * wait for server to be ready i.e. listening to incoming connections
	 */
	void wait_for_server_ready();


	/**
	 * invoke the write system call;
	 *
	 * return the number of bytes written on success or partial success;
	 * return -1 if write fails; errno is written in _errno if not null pointer
	 *
	 *  example: two calls to write syscall are made, the first is partially successful
	 *  but the second returns -1 (EAGAIN or EWOULDBLOCK); in this case _errno is set but the
	 *  number of bytes written is returned
	 *
	 * if the first write syscall returns -1 (EAGAIN or EWOULDBLOCK), then the function returns -1.
	 *
	 *
	 */
	ssize_t write(int fd, const char * data, ssize_t data_size, int * _errno = nullptr) noexcept;

	/**
	 * write data to the socket
	 */
	template <class T>
	ssize_t write(int fd, std::vector<T> &data, int * _errno = nullptr) noexcept {
		ssize_t data_size = data.size() * sizeof(T);
		const char * p =  reinterpret_cast<char*>(data.data());

		LIB_LOG(debug) << "IGenericServer::Write<> data_size = " << data_size << " sizeof(T)=" << sizeof(T);

		return write(fd, p, data_size, _errno);
	}


	/**
	 * read all available data from socket and return multiple buffers as a vector of vectors
	 *
	 * buffer_size is the size in bytes of the read buffers
     * if _errno is provided, the errno from the last read syscall will be stored in *_errno
	 */
	std::vector<std::vector<char>> read(int fd, size_t buffer_size = 4096, int * _errno = nullptr) noexcept;

	/**
	 * read data from socket in a buffer of size readBufferSize
	 */
	std::vector<char> read_one_buffer(int fd, size_t buffer_size = 4096, int * _errno = nullptr) noexcept;

	/**
	 * set socket as non blocking if non_blocking is true, else set as blocking
	 */
	static int set_fd_non_blocking(int fd, bool non_blocking) noexcept;

	/**
	 * get number of active connections to server
	 */
	int get_active_connections() noexcept;

	/**
	 * close socket and decrease counter of active connections
	 */
	void close(int fd) noexcept;

	/**
	 * remove socket from epoll watch list
	 */
	void remove_from_epoll(int fd, int * _errno = nullptr) noexcept;

	/**
	 * set send buffer size of socket
	 * return 0 if successful, -1 in case of error
	 */
	int set_send_buffer_size(int sockfd, int send_buffer_size) noexcept;

	/**
	 * get receive and send buffer size
	 * return 0 if successful, -1 in case of error
	 */
	int get_buffers_size(int sockfd, int &send_buffer_size, int &receive_buffer_size) noexcept;

	/**
	 * read data from socket in a buffer of size readBufferSize * sizeof(N)
	 */
	template <typename N>
	std::vector<N> read_one_buffer_t(int fd, size_t buffer_size = 4096, int * _errno = nullptr, ssize_t * bytes_read_p = nullptr) noexcept {

		set_IO(fd);

		std::vector<N> buffer(buffer_size); // size in bytes is buffer_size * sizeof(N)

		ssize_t c;
		char * p;

		p = reinterpret_cast<char*>(buffer.data());

		// read from socket
		c = ::read(fd, p, buffer_size * sizeof(N));

		if (_errno != nullptr) *_errno = errno;

		if (bytes_read_p != nullptr) *bytes_read_p = c;

		if (c >= 0) {
			// ok we received data
			buffer.resize(c / sizeof(N));
		} else if (c == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			// no data available to read
			LIB_LOG(warning) << "[IGenericServer::read_one_t] errno == EAGAIN || errno == EWOULDBLOCK";

			buffer.resize(0);
		} else if (c == -1) {
			// some other error returned by read syscall
			LIB_LOG(error) << "[IGenericServer::read_one_t] errno: " << strerror(errno);

			buffer.resize(0);
		}

		return buffer;
	}

};

}

#endif /* IGENERICSERVER_H_ */
