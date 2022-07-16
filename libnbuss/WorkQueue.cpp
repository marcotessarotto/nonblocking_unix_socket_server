#include <WorkQueue.h>

namespace nbuss_server {

static std::atomic<int> producedItems;
static std::atomic<int> consumedItems;

WorkQueue::WorkQueue(ThreadDecorator & threadDecorator, unsigned int numberOfThreads) :
		threadDecorator{threadDecorator},
		numberOfThreads(numberOfThreads),
		deque{},
		stopConsumers{false} {
	if (numberOfThreads < 1) {
		throw std::invalid_argument("numberOfThreads must be > 0");
	}
}

WorkQueue::~WorkQueue() {

}

void WorkQueue::callback(IGenericServer * srv, int fd, enum job_type_t job) {
	int num;

	Item i{srv, fd, job};

	std::unique_lock<std::mutex> lk(dequeMutex);

	deque.push_front(i);
	num = deque.size();

	lk.unlock();

	//producedItems++;

	std::cout << "[WorkQueue::callback] [Producer] items in deque=" << num << std::endl;

}

void WorkQueue::start() {

	// TODO: start consumer thread(s)

	stopConsumers = false;

	threadDecorator.start(
			[this](IGenericServer * srv, int fd, enum job_type_t job) {
					this->callback(srv, fd, job);
				}
			);
}

void WorkQueue::stop() {
	// TODO: stop consumer thread(s)

	stopConsumers = true;

	threadDecorator.stop();
}

} /* namespace nbuss_server */
