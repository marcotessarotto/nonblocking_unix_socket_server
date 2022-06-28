#include <syslog.h>

#include <iostream>

#include "UnixSocketServer.h"

using namespace std;
using namespace nbuss_server;


void my_listener(int fd, enum job_type_t job_type) {

	switch(job_type) {
	case CLOSE_SOCKET:

		cout << "closing socket " << fd << endl;
		close(fd);

		break;
	case DATA_REQUEST:
		cout << "incoming data fd=" << fd  << endl;

		// read all data from socket
		auto data = UnixSocketServer::readAllData(fd);

		cout << "size of data: " << data.size() << endl;

		int counter = 0;
		for (std::vector<char> item: data) {
			cout << counter << ": " << item.data() << endl;

			UnixSocketServer::writeToSocket(fd, item);
		}

	}

}


int main(int argc, char *argv[])
{
	openlog("nbuss_server", LOG_CONS | LOG_PERROR, LOG_USER);

	string socketName = "/tmp/mysocket.sock";

	cout << "starting non blocking unix socket server on socket " << socketName << endl;

	UnixSocketServer server(socketName, 10);

	server.setup();

	// this call does not return
	server.listen(my_listener);

	return 0;
}
