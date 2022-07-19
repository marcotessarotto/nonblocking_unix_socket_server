#include <ThreadDecorator.h>
#include <iostream>
#include <string>
#include <thread>

#include "Logger.h"

namespace nbuss_server {

ThreadDecorator::ThreadDecorator(IGenericServer &server) :
		worker_is_running{false},
		callback_function{},
		server{server},
		workerThread{}
		{
	LIB_LOG(info) << "ThreadDecorator::ThreadDecorator(IGenericServer &server)";
}

ThreadDecorator::~ThreadDecorator() {
	LIB_LOG(info) << "ThreadDecorator::~ThreadDecorator";
}


void ThreadDecorator::mainLoopWorker() {

	LIB_LOG(info) << "mainLoopWorker start";

	// listen returns when another thread calls terminate
	server.listen(callback_function);

	// thread ends
	LIB_LOG(info) << "mainLoopWorker end";
}

void ThreadDecorator::start(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function) {
	this->callback_function = callback_function;

	// std::thread is not CopyConstructible or CopyAssignable, although it is MoveConstructible and MoveAssignable.
	// a temporary object is created and then moveAssigned to workerThread
	workerThread = std::thread{&ThreadDecorator::mainLoopWorker, this};

	// return when server is ready i.e. listening for incoming connections
	server.waitForServerReady();
}


void ThreadDecorator::stop() {

	LIB_LOG(info) << "ThreadDecorator::stop";

	// terminate the server thread
	server.terminate();

	// join thread
	workerThread.join();
}

} /* namespace nbuss_server */
