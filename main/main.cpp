#include <syslog.h>

#include <iostream>

#include "UnixSocketServer.h"
#include "ThreadDecorator.h"
#include "UnixSocketClient.h"

using namespace std;
using namespace nbuss_server;
using namespace nbuss_client;

static void my_listener(int fd, enum job_type_t job_type) {

	switch(job_type) {
	case CLOSE_SOCKET:

		cout << "[server] closing socket " << fd << endl;
		close(fd);

		break;
	case DATA_REQUEST:
		cout << "[server] incoming data fd=" << fd  << endl;

		// read all data from socket
		auto data = UnixSocketServer::read(fd);

		cout << "[server] size of data: " << data.size() << endl;

		int counter = 0;
		for (std::vector<char> item: data) {
			cout << "[server] item " << counter << ": " << item.size() << " bytes" << endl;
			// cout << item.data() << endl;


			UnixSocketServer::write(fd, item);
		}

		cout << "[server] incoming data - finished elaboration\n";

	}

}

/*
 * test with valgrind:
 *
 * valgrind -s --leak-check=yes build/cmake.debug.linux.x86_64/main/nbuss_server
 */


//const bool USE_THREAD_DECORATOR = true;
#define USE_THREAD_DECORATOR

int main(int argc, char *argv[])
{
	openlog("nbuss_server", LOG_CONS | LOG_PERROR, LOG_USER);

	string socketName = "/tmp/mysocket.sock";

	cout << "starting non blocking unix socket server on socket " << socketName << endl;

#ifdef USE_THREAD_DECORATOR

	// see also ServerClientReadWriteTest in testlibnbuss.cpp

	UnixSocketServer uss(socketName, 10);

	// listen method is called in another thread
	ThreadDecorator threadedServer(uss);

	// ThreadDecorator threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));


	// when start returns, server has started listening for incoming connections
	threadedServer.start(my_listener);

	UnixSocketClient usc;

	// so we can create a client instance and make it connect to the server
	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	cout << "[client] writing to socket\n";
	usc.write(v);

	cout << "[client] reading from socket\n";
	// read server response
	auto response = usc.read(1024);

	cout << "[client] received data size: " << response.size() << endl;

	usc.close();

	threadedServer.stop();

	cout << "test finished!" << endl;

#else
	// UnixSocketServer server(socketName, 10); // calls constructor with lvalue

	UnixSocketServer server("/tmp/mysocket.sock", 10); // calls constructor which moves string instance (rvalue)

	server.setup();

	// this call does not return
	server.listen(my_listener);

#endif



	return 0;
}
