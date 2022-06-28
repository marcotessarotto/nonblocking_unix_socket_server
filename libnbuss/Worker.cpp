#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "Worker.h"

namespace nbuss_server {

Worker::Worker(IGenericServer &server) :
		terminate_worker(false), worker_is_running(false), callback_function(),
		server(server), workerThread() {
}

Worker::~Worker() {
}

void Worker::setup() {
	server.setup();
}

void Worker::mainLoopWorker() {

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

void Worker::listen(std::function<void(int, enum job_type_t )> callback_function) {
	server.listen(callback_function);
}

void Worker::start(std::function<void(int, enum job_type_t )> callback_function) {
	this->callback_function = callback_function;

	workerThread = std::thread(&Worker::mainLoopWorker, this);
}

void Worker::terminate() {
	server.terminate();
}

void Worker::stop() {

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
