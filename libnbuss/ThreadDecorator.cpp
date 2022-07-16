#include <ThreadDecorator.h>
#include <iostream>
#include <string>
#include <thread>


namespace nbuss_server {

ThreadDecorator::ThreadDecorator(IGenericServer &server) :
		worker_is_running{false},
		callback_function{},
		server{server},
		workerThread{}
		{
	std::cout << "ThreadDecorator::ThreadDecorator(IGenericServer &server)" << std::endl;
}

ThreadDecorator::~ThreadDecorator() {
	std::cout << "ThreadDecorator::~ThreadDecorator" << std::endl;
}


void ThreadDecorator::mainLoopWorker() {

	std::cout << "mainLoopWorker start" << std::endl;

	// listen returns when another thread calls terminate
	server.listen(callback_function);

	// thread ends
	std::cout << "mainLoopWorker end" << std::endl;
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

	std::cout << "ThreadDecorator::stop" << std::endl;

	// terminate the server thread
	server.terminate();

	// join thread
	workerThread.join();
}

} /* namespace nbuss_server */
