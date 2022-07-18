# nonblocking_unix_socket_server
sample project for C++ unix non blocking socket server, using epoll syscalls

has support for tcp non blocking socket server

# example

    static void my_listener(IGenericServer * srv, int fd, enum job_type_t job_type) {

	    switch (job_type) {
          case CLOSE_SOCKET:
              cout << "closing socket " << fd << endl;
              srv->close(fd);
              break;
	        case DATA_REQUEST:
              // read all data from socket
              auto data = UnixSocketServer::read(fd);
              .....		 
	    }
    }

	string socketName = "/tmp/mysocket_test.sock";
	UnixSocketServer uss{socketName, 10};
	ThreadDecorator threadedServer(uss);

	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);
	....
	threadedServer.stop();
  
  

# build

git clone https://github.com/marcotessarotto/nonblocking_unix_socket_server/

cd nonblocking_unix_socket_server

mkdir build

cd build 

cmake ..

cmake --build .

# debug build

cd build

mkdir Debug

cd Debug

cmake -DCMAKE_BUILD_TYPE=Debug ../..

cmake --build .

# requirements

Boost:log
