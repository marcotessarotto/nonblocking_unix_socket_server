#include <syslog.h>
#include <ThreadedServer.h>

#include <iostream>
#include <cassert>

#include "UnixSocketServer.h"
#include "UnixSocketClient.h"
#include "TcpServer.h"
#include "TcpClient.h"

#include "Logger.h"

using namespace std;
using namespace nbuss_server;
using namespace nbuss_client;

static void my_listener(IGenericServer::ListenEvent listen_event) {

	IGenericServer * srv = static_cast<IGenericServer *>(listen_event.srv);

	switch (listen_event.job) {
	case CLOSE_SOCKET:

		cout << "[server my_listener] closing socket " << listen_event.fd << endl;

		srv->close(listen_event.fd);
		//close(fd);

		break;
	case AVAILABLE_FOR_READ:
		cout << "[server my_listener] incoming data on socket " << listen_event.fd << endl;

		// read all data from socket
		auto data = srv->read(listen_event.fd);

		cout << "[server my_listener] number of vectors returned by read: "
				<< data.size() << endl;

		int counter = 0;
		for (std::vector<char> item : data) {
			cout << "[server my_listener] item " << counter << ": "
					<< item.size() << " bytes" << endl;
			// cout << item.data() << endl;

			srv->write<char>(listen_event.fd, item);
		}

		cout << "[server my_listener] incoming data - finished processing\n";

	}

}

/*
 * test with valgrind:
 *
 * valgrind -s --leak-check=yes build/cmake.debug.linux.x86_64/main/nbuss_server
 *
 * debug build:
 *
 * cd build/cmake.debug.linux.x86_64
 * cmake -DCMAKE_BUILD_TYPE=Debug ../..
 * cmake --build .
 *
 */

#define USE_THREAD_DECORATOR

#define USE_TCP

int main(int argc, char *argv[]) {
	openlog("nbuss_server", LOG_CONS | LOG_PERROR, LOG_USER);


	LIB_LOG(info) << "****";

#ifdef USE_TCP

	// this will leave N tcp connections in TIME_WAIT state
	// see also https://vincent.bernat.ch/en/blog/2014-tcp-time-wait-state-linux

	for (int i = 0; i < 10; i++) {

		cout << endl << endl << endl;

		TcpServer ts(10001, "0.0.0.0", 10);

		// listen method is called in another thread
		ThreadedServer threadedServer(ts);

		cout << "[server] starting server\n";
		// when start returns, server has started listening for incoming connections
		threadedServer.start(my_listener);

		TcpClient tc;
		tc.connect("0.0.0.0", 10001);

		std::string s = "test message";
		std::vector<char> v(s.begin(), s.end());

		cout << "[client] writing to socket\n";
		tc.write<char>(v);

		cout << "[client] reading from socket\n";
		// read server response
		auto response = tc.read(1024);

		cout << "[client] received data size: " << response.size() << endl;

		assert(response.size() == 12);

		cout << "[client] closing socket" << endl;
		tc.close();

		// spin... consider adding a condition variable
		while (ts.get_active_connections() > 0)
			;

		cout << "[server] stopping server" << endl;
		threadedServer.stop();

		cout << "[main] test finished! " << i << endl;

	}

#else

	for (int i = 0; i < 100; i++) {

	string socketName = "/tmp/mysocket.sock";

	cout << "starting non blocking unix socket server on socket " << socketName << endl;

#ifdef USE_THREAD_DECORATOR

	// see also ServerClientReadWriteTest in testlibnbuss.cpp

	UnixSocketServer uss(socketName, 10);

	// listen method is called in another thread
	ThreadedServer threadedServer(uss);

	// ThreadDecorator threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));


	cout << "[server] starting server\n";
	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	UnixSocketClient usc;

	cout << "[client] connect to server\n";
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	cout << "[client] writing to socket\n";
	usc.write(v);

	cout << "[client] reading from socket\n";
	// read server response
	auto response = usc.read(1024);

	cout << "[client] received data size: " << response.size() << endl;

	assert(response.size() == 12);

	cout << "[client] closing socket" << endl;
	usc.close();

	cout << "[server] stopping server" << endl;
	threadedServer.stop();

	cout << "[main] test finished! " << i << endl;

#else
	// UnixSocketServer server(socketName, 10); // calls constructor with lvalue

	UnixSocketServer server("/tmp/mysocket.sock", 10); // calls constructor which moves string instance (rvalue)

	server.setup();

	// this call does not return
	server.listen(my_listener);

#endif

	}

#endif

	return 0;
}
