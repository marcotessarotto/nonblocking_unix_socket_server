#include <WorkQueue.h>

#include "Logger.h"

namespace nbuss_server {

static std::atomic<int> producedItems;
static std::atomic<int> consumedItems;

WorkQueue::WorkQueue(ThreadDecorator & threadDecorator, unsigned int numberOfThreads) :
		threadDecorator{threadDecorator},
		numberOfThreads(numberOfThreads),
		callback_function{},
		deque{},
		stopConsumers{false} {
	if (numberOfThreads < 1) {
		throw std::invalid_argument("numberOfThreads must be > 0");
	}
}

WorkQueue::~WorkQueue() {

}

void WorkQueue::producerCallback(IGenericServer * srv, int fd, enum job_type_t job) {
	int num;

	Item i{srv, fd, job};

	std::unique_lock<std::mutex> lk(dequeMutex);

	deque.push_front(i);
	num = deque.size();

	lk.unlock();

	dequeCv.notify_one();
	//producedItems++;

	LIB_LOG(info) << "[WorkQueue::callback] [Producer] items in deque=" << num;

}

void WorkQueue::consumer() {

	LIB_LOG(info) << "[WorkQueue::consumer] starting consumer thread";

	std::unique_lock<std::mutex> lk{dequeMutex, std::defer_lock};

	while (stopConsumers == false) {

		//std::unique_lock<std::mutex> lk(dequeMutex);
		lk.lock();

        while (deque.size() == 0) {
        	dequeCv.wait(lk);

        	if (stopConsumers) {
        		goto end;
        	}

        	LIB_LOG(info) << "[WorkQueue::consumer] itemsInDeque=" << deque.size();
        }

        Item &i = deque.back();

    	deque.pop_back();

		lk.unlock();

		LIB_LOG(info) << "[WorkQueue::consumer] processing item - fd=" << i.fd;

		// TODO: process Item i

		/*
		 * note: if fd is closed, subsequent events relative to fd should be removed (before closing fd)
		 */

	}
end:
LIB_LOG(info) << "[WorkQueue::consumer] stopping consumer thread";

}

void WorkQueue::start(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function) {

	stopConsumers = false;

	consumerThreads.resize(numberOfThreads);
	for (int i = 0; i < numberOfThreads; i++) {
		consumerThreads[i] = std::thread(&WorkQueue::consumer, this);
	}


	threadDecorator.start(
		[this](IGenericServer * srv, int fd, enum job_type_t job) {
				this->producerCallback(srv, fd, job);
			}
		);
}

void WorkQueue::stop() {

	LIB_LOG(info) << "WorkQueue::stop()";

	stopConsumers = true;

	threadDecorator.stop();

	for (int i = 0; i < numberOfThreads; i++) {
		dequeCv.notify_one();
	}

	for (int i = 0; i < numberOfThreads; i++) {
		consumerThreads[i].join();
	}

	// cleanup queue
	deque.clear();
	consumerThreads.clear();

}

} /* namespace nbuss_server */
