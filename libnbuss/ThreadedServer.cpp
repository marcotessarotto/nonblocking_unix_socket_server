#include <ThreadedServer.h>
#include <iostream>
#include <string>
#include <thread>

#include "Logger.h"

namespace nbuss_server {

ThreadedServer::ThreadedServer(IGenericServer &server) :
		worker_is_running{false},
		callback_function{},
		server{server},
		workerThread{}
		{
	LIB_LOG(info) << "ThreadedServer::ThreadedServer(IGenericServer &server)";
}

ThreadedServer::~ThreadedServer() {
	LIB_LOG(info) << "ThreadedServer::~ThreadedServer";
}


void ThreadedServer::listenWorker() {
	LIB_LOG(info) << "ThreadedServer::mainLoopWorker start";

	// listen returns when another thread calls terminate
	server.listen(callback_function);

	// thread ends
	LIB_LOG(info) << "ThreadedServer::mainLoopWorker end";
}

void ThreadedServer::start(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function) {

	std::unique_lock<std::mutex> lk(mtx);
	bool _is_listening = is_listening;
	lk.unlock();

	if (_is_listening) {
		LIB_LOG(error) << "server is already listening";
		throw std::runtime_error("server is already listening");
	}

	this->callback_function = callback_function;

	try {
		// std::thread is not CopyConstructible or CopyAssignable, although it is MoveConstructible and MoveAssignable.
		// a temporary object is created and then moveAssigned to workerThread
		workerThread = std::thread{&ThreadedServer::listenWorker, this};

	} catch (const std::exception &e) {
		LIB_LOG(error)	<< "[ThreadedServer::start] exception: " << e.what();
	}

	// return when server is ready i.e. listening for incoming connections
	server.waitForServerReady();
}


void ThreadedServer::stop() {

	LIB_LOG(info) << "ThreadedServer::stop";

	// terminate the server thread
	server.terminate();

	// join thread
	workerThread.join();
}

} /* namespace nbuss_server */
