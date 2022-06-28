#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "Worker.h"

namespace nbuss_server {

Worker::Worker(IGenericServer &server,
		std::function<void(int, enum job_type_t)> callback_function) :
		terminate_worker(false), worker_is_running(false), callback_function(
				callback_function), server(server), workerThread() {
}

Worker::~Worker() {
	// TODO Auto-generated destructor stub
}

void Worker::mainLoopWorker() {

	std::cout << "mainLoopWorker starts" << std::endl;

	std::unique_lock<std::mutex> lk(m);
	worker_is_running = true;
	lk.unlock();

	server.listen(callback_function);

	lk.lock();
	worker_is_running = false;
	lk.unlock();

	cv.notify_one();

	// thread ends
}

void Worker::start() {
	workerThread = std::thread(&Worker::mainLoopWorker, this);
}

void Worker::terminate() {
	// wait for the worker to end

	server.terminate();

	std::unique_lock<std::mutex> lk(m);
	while (worker_is_running)
		cv.wait(lk);

	workerThread.join();

}

} /* namespace nbuss_server */
