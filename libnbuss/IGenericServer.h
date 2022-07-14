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

namespace nbuss_server {

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
public:
	IGenericServer();
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
     * terminate server
     *
     * @throws std::runtime_error
     */
	virtual void terminate() = 0;

	/**
	 * wait for server to be ready i.e. listening to incoming connections
	 */
	virtual void waitForServerReady() = 0;


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
