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


	/**
	 * open the unix socket on which the server listens; socket descriptor is stored in listen_sock field
	 * returns fd if successful, -1 in case of error
	 */
	int open_unix_socket();

	/// used by constructors to check parameters
	void checkParameters();


    /**
     * setup server and bind socket to listening address (but do not call listen syscall)
     *
     * @throws std::runtime_error
     */
	void setup();

public:
	/**
	 * create instance; parameters are the name of the unix socket on the file system and backlog size
	 */
	UnixSocketServer(const std::string &sockname, unsigned int backlog);
	UnixSocketServer(const std::string &&sockname, unsigned int backlog);
	virtual ~UnixSocketServer();



};

} /* namespace mt_cpp_server */

#endif /* UNIXSOCKETSERVER_H_ */
