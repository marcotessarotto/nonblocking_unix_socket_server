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
#include "UnixPipe.h"

namespace nbuss_server {

// TODO: consider moving to enum class
enum job_type_t {
	DATA_REQUEST, // incoming data is present
	CLOSE_SOCKET // socket must be closed
};


/**
 * IGenericServer defines an abstract class for a generic server.
 *
 * This allows to introduce different kind of servers i.e. unix socket server, TCP socket server ...
 *
 */
class IGenericServer {

protected:
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


public:
	IGenericServer();

	IGenericServer(unsigned int backlog);
	virtual ~IGenericServer();
	



    /**
     * starts a new thread which will listen for incoming connections and process them
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are: socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	virtual void listen(std::function<void(int, enum job_type_t )> callback_function) = 0;


    /**
     * terminate server; when the method returns, the server has terminated operations
     *
     */
	virtual void terminate();

	/**
	 * wait for server to be ready i.e. listening to incoming connections
	 */
	virtual void waitForServerReady();


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

};

}

#endif /* IGENERICSERVER_H_ */
