#include <WorkQueue.h>

#include "Logger.h"

namespace nbuss_server {

static std::atomic<int> producedItems;
static std::atomic<int> consumedItems;

WorkQueue::WorkQueue(ThreadedServer & threadedServer, unsigned int numberOfThreads) :
		threadedServer{threadedServer},
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

	Item i{fd, job};

	std::unique_lock<std::mutex> lk(dequeMutex);

	deque.push_front(i);
	num = deque.size();

	lk.unlock();

	dequeCv.notify_one();
	//producedItems++;

	LIB_LOG(info) << "[WorkQueue::callback] [Producer] items in deque size=" << num;

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

		LIB_LOG(info) << "[WorkQueue::consumer] processing item - fd=" << i.fd << " process=" << i.process;

		if (i.process) {
			// process Item i
			callback_function(this, i.fd, i.job);
		}

		/*
		 * note: if fd is closed, subsequent events relative to fd should be removed (before closing fd)
		 */

	}
end:
LIB_LOG(info) << "[WorkQueue::consumer] stopping consumer thread";

}

void WorkQueue::start(std::function<void(WorkQueue *, int, enum job_type_t )> callback_function) {

	stopConsumers = false;

	this->callback_function = callback_function;

	consumerThreads.resize(numberOfThreads);
	for (int i = 0; i < numberOfThreads; i++) {
		consumerThreads[i] = std::thread(&WorkQueue::consumer, this);
	}


	threadedServer.start(
		[this](IGenericServer * srv, int fd, enum job_type_t job) {
				this->producerCallback(srv, fd, job);
			}
		);
}

void WorkQueue::stop() {

	LIB_LOG(info) << "WorkQueue::stop()";

	stopConsumers = true;

	threadedServer.stop();

	for (int i = 0; i < numberOfThreads; i++) {
		dequeCv.notify_one();
	}

	for (int i = 0; i < numberOfThreads; i++) {
		consumerThreads[i].join();
	}

	// cleanup queue
	deque.clear();
	consumerThreads.clear();

	LIB_LOG(info) << "WorkQueue::stop() complete";
}


void WorkQueue::close(int fd) {
	LIB_LOG(info) << "WorkQueue::close() fd=" << fd;

	std::unique_lock<std::mutex> lk(dequeMutex);

	// enumerate items in deque and mark as do not process items belonging to fd
    for (auto it = deque.rbegin(); it != deque.rend(); ++it) {
        if (it->fd == fd) {
        	it->process = false;
        }
    }

	lk.unlock();


}

void WorkQueue::remove_from_epoll(int fd) {
	threadedServer.remove_from_epoll(fd);
}


} /* namespace nbuss_server */
