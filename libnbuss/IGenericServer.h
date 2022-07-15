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
#include "FileDescriptor.h"
#include "UnixPipe.h"

namespace nbuss_server {

// TODO: consider moving to enum class
enum job_type_t {
	DATA_REQUEST, // incoming data is present
	CLOSE_SOCKET // socket must be closed
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
		std::function<void(IGenericServer *)> func;
		IGenericServer *server;

		RunOnReturn(IGenericServer * server, std::function<void(IGenericServer *)> func) : server{server}, func{func} {
		}
		~RunOnReturn() {
			std::cout << "~RunOnReturn" << std::endl;
			func(server);
		}
	};

	/// declared as fields so that RunOnReturn instance in listen method can operate on them
	FileDescriptor listen_sock;
	FileDescriptor epollfd;

	/// flag used to notify that server must stop listening and close sockets
	std::atomic<bool> stop_server;

	/// flag which signals that server is listening for incoming connections
	/// we use bool (and not a std::atomic) because we synchronize through a condition variable
	bool is_listening;

	/// mutex is used to synchronize access to is_listening
	std::mutex mtx;
	/// used to wait for is_listening to become false
	std::condition_variable cv;

	//	/// pipe used for signaling to listening thread that it must terminate
	UnixPipe commandPipe;


	/// backlog for listening server socket
	unsigned int backlog;

	std::atomic<int> activeConnections;

public:

	IGenericServer(unsigned int backlog = 10);
	virtual ~IGenericServer();
	

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
	void listen(std::function<void(IGenericServer * srv, int, enum job_type_t )> callback_function);


    /**
     * terminate server; when the method returns, the server has terminated operations
     *
     */
	void terminate();

	/**
	 * wait for server to be ready i.e. listening to incoming connections
	 */
	void waitForServerReady();


	/**
	 * write data to the socket
	 */
	static int write(int fd, std::vector<char> item);


	/**
	 * read all available data from socket and return vector of vectors
	 */
	static std::vector<std::vector<char>> read(int fd);


	/**
	 * set socket as non blocking
	 */
	int setFdNonBlocking(int fd);

	/**
	 * close listening socket and epoll socket
	 */
	void closeSockets();

	/**
	 * get number of active connections to server
	 */
	int getActiveConnections() { return activeConnections.load(); }

	/**
	 * close socket and decrease counter of active connections
	 */
	void close(int fd) {

		//std::cout << "IGenericServer::close " << fd;
		if (fd >= 0) {
			::close(fd);
			activeConnections--;
		}
	}

};

}

#endif /* IGENERICSERVER_H_ */
