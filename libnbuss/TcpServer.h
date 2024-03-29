#ifndef LIBNBUSS_TCPSERVER_H_
#define LIBNBUSS_TCPSERVER_H_


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

class TcpServer :  public IGenericServer {

	std::string address;
	unsigned int port;

	/**
	 * open the socket on which the server listens; socket descriptor is stored in listen_sock field
	 * returns fd if successful, -1 in case of error
	 */
	int open_listening_socket();

    /**
     * setup server and bind socket to listening address (but do not call listen syscall)
     *
     * setup invokes open_listening_socket and implements a retry mechanism if the server socket cannot be opened
     *
     * @throws std::runtime_error
     */
	void setup();

public:
	TcpServer(unsigned int port, const std::string &address = "0.0.0.0", unsigned int backlog = 10);

	virtual ~TcpServer();

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_TCPSERVER_H_ */
