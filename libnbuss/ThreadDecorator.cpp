#include <ThreadDecorator.h>
#include <iostream>
#include <string>
#include <thread>


namespace nbuss_server {

ThreadDecorator::ThreadDecorator(IGenericServer &server) :
		worker_is_running{false},
		callback_function{},
		server{server},
		workerThread{} {
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

void ThreadDecorator::listen(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function) {
	server.listen(callback_function);
}

void ThreadDecorator::start(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function) {
	this->callback_function = callback_function;

	workerThread = std::thread{&ThreadDecorator::mainLoopWorker, this};

	// return when server is ready i.e. listening for incoming connections
	server.waitForServerReady();
}

void ThreadDecorator::terminate() {
	std::cout << "ThreadDecorator::terminate" << std::endl;
	server.terminate();
}

void ThreadDecorator::waitForServerReady() {
	server.waitForServerReady();
}

void ThreadDecorator::stop() {

	// terminate the server thread
	server.terminate();

	// join thread
	workerThread.join();
}

} /* namespace nbuss_server */
