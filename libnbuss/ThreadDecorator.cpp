#include <ThreadDecorator.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>


namespace nbuss_server {

ThreadDecorator::ThreadDecorator(IGenericServer &server) :
		terminate_worker(false), worker_is_running(false), callback_function{},
		server{server}, workerThread{} {
	std::cout << "ThreadDecorator::ThreadDecorator(IGenericServer &server)" << std::endl;
}

//ThreadDecorator::ThreadDecorator(IGenericServer &&server) :
//		terminate_worker(false), worker_is_running(false), callback_function{},
//		server(server), workerThread{} {
//
//	std::cout << "ThreadDecorator::ThreadDecorator(IGenericServer &&server) " << std::endl;
//
//	std::cout << typeid(server).name() << std::endl;
//	std::cout << typeid(this->server).name() << std::endl;
//}

ThreadDecorator::~ThreadDecorator() {
}

void ThreadDecorator::setup() {
	std::cout << "server.setup()" << std::endl;
	server.setup();
}

void ThreadDecorator::mainLoopWorker() {

	std::cout << "mainLoopWorker starts" << std::endl;

	std::unique_lock<std::mutex> lk(m);
	worker_is_running = true;
	lk.unlock();

	// listen returns when another thread calls terminate
	server.listen(callback_function);

	lk.lock();
	worker_is_running = false;
	lk.unlock();

	cv.notify_one();

	// thread ends
}

void ThreadDecorator::listen(std::function<void(int, enum job_type_t )> callback_function) {
	server.listen(callback_function);
}

void ThreadDecorator::start(std::function<void(int, enum job_type_t )> callback_function) {
	this->callback_function = callback_function;

	workerThread = std::thread(&ThreadDecorator::mainLoopWorker, this);
}

void ThreadDecorator::terminate() {
	server.terminate();
}

void ThreadDecorator::stop() {

	// tell the thread to terminate
	server.terminate();

	// wait for the worker to end
	std::unique_lock<std::mutex> lk(m);
	while (worker_is_running)
		cv.wait(lk);

	// join thread
	workerThread.join();
}

} /* namespace nbuss_server */
