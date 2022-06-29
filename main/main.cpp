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

		cout << "closing socket " << fd << endl;
		close(fd);

		break;
	case DATA_REQUEST:
		cout << "incoming data fd=" << fd  << endl;

		// read all data from socket
		auto data = UnixSocketServer::read(fd);

		cout << "size of data: " << data.size() << endl;

		int counter = 0;
		for (std::vector<char> item: data) {
			cout << "item " << counter << ": " << item.size() << " bytes" << endl;
			// cout << item.data() << endl;

			UnixSocketServer::write(fd, item);
		}

	}

}


//const bool USE_THREAD_DECORATOR = true;
#define USE_THREAD_DECORATOR

int main(int argc, char *argv[])
{
	openlog("nbuss_server", LOG_CONS | LOG_PERROR, LOG_USER);

	string socketName = "/tmp/mysocket.sock";

	cout << "starting non blocking unix socket server on socket " << socketName << endl;

#ifdef USE_THREAD_DECORATOR

	UnixSocketServer u("/tmp/mysocket.sock", 10);

	ThreadDecorator threadedServer(u);

	// ThreadDecorator threadedServer(UnixSocketServer("/tmp/mysocket.sock", 10));

	threadedServer.setup();

	//threadedServer.listen(my_listener);

	threadedServer.start(my_listener);

	//cout << "before pause" << endl;

	//pause();

	sleep(1);

	UnixSocketClient usc;

	usc.connect(socketName);

	std::string s = "test message";
	std::vector<char> v(s.begin(), s.end());

	usc.write(v);
	// read server response
	//std::vector<char> response = usc.read();

	sleep(1);

	usc.close();


	threadedServer.stop();

	cout << "finished!" << endl;

#else
	// UnixSocketServer server(socketName, 10); // calls constructor with lvalue

	UnixSocketServer server("/tmp/mysocket.sock", 10); // calls constructor which moves string instance (rvalue)

	server.setup();

	// this call does not return
	server.listen(my_listener);

#endif





	return 0;
}
