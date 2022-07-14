#ifndef UNIXSOCKETSERVER_H_
#define UNIXSOCKETSERVER_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>

#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "IGenericServer.h"
#include "FileDescriptor.h"
#include "configuration.h"
#include "UnixPipe.h"

namespace nbuss_server {

#define INIT_BUF_LEN 1024

/**
 * implement a non blocking unix socket server (although it blocks on epoll_wait syscall)
 *
 * constructor needs the name of the socket on file system and backlog size.
 *
 * incoming data is passed using a callback.
 *
 * uses linux-specific system calls.
 */
class UnixSocketServer : public virtual IGenericServer {

	/// socket name on file system
	std::string sockname;

	/// backlog for listening server socket
	unsigned int backlog;

	/// flag used to notify that server must stop listening and close sockets
	std::atomic<bool> stop_server;

	/// flag which signals that server is listening for incoming connections
	/// we use bool (and not a std::atomic) because we synchronize through a condition variable
	bool is_listening;

	/// mutex is used to synchronize access to is_listening
	std::mutex mtx;
	/// used to wait for is_listening to become false
	std::condition_variable cv;


	/// inner class for running a function when a method ends (see listen method)
	class RunOnReturn {
	public:
		std::function<void(UnixSocketServer *)> func;
		UnixSocketServer *server;

		RunOnReturn(UnixSocketServer * server, std::function<void(UnixSocketServer *)> func) : server{server}, func{func} {
		}
		~RunOnReturn() {
			std::cout << "~RunOnReturn" << std::endl;
			func(server);
		}
	};

	/// declared as fields so that RunOnReturn instance in listen method can operate on them
	FileDescriptor listen_sock;
	FileDescriptor epollfd;


	/**
	 * open the unix socket on which the server listens; socket descriptor is stored in listen_sock field
	 * returns fd if successful, -1 in case of error
	 */
	int open_unix_socket();

	/// used by constructors to check parameters
	void checkParameters();

	/// pipe used for signaling to listening thread that it must terminate
	UnixPipe commandPipe;


    /**
     * setup server and bind socket to listening address (but do not call listen syscall)
     *
     * @throws std::runtime_error
     */
	virtual void setup();

public:
	/**
	 * create instance; parameters are the name of the unix socket on the file system and backlog size
	 */
	UnixSocketServer(const std::string &sockname, unsigned int backlog);
	UnixSocketServer(const std::string &&sockname, unsigned int backlog);
	virtual ~UnixSocketServer();


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
	virtual void listen(std::function<void(int, enum job_type_t )> callback_function);


	/**
	 * terminate server instance
	 */
	virtual void terminate();

	/**
	 * wait for server to start listening for incoming connections
	 */
	virtual void waitForServerReady();

	/**
	 * read all available data from socket and return vector of vectors
	 */
	//static std::vector<std::vector<char>> read(int fd);

	/**
	 * write data to the socket
	 */
	//static int write(int fd, std::vector<char> item);


	/**
	 * set file descriptor attribute to O_NONBLOCK (moved to IGenericServer)
	 */
	//static int setFdNonBlocking(int fd);

};

} /* namespace mt_cpp_server */

#endif /* UNIXSOCKETSERVER_H_ */
