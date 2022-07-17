#include <WorkQueue.h>

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

	std::cout << "[WorkQueue::callback] [Producer] items in deque=" << num << std::endl;

}

void WorkQueue::consumer() {

	std::cout << "[WorkQueue::consumer] starting consumer thread" << std::endl;

	std::unique_lock<std::mutex> lk{dequeMutex, std::defer_lock};

	while (stopConsumers == false) {

		//std::unique_lock<std::mutex> lk(dequeMutex);
		lk.lock();

        while (deque.size() == 0) {
        	dequeCv.wait(lk);

        	if (stopConsumers) {
        		goto end;
        	}

        	std::cout << "[WorkQueue::consumer] itemsInDeque=" << deque.size() << std::endl;
        }

        Item &i = deque.back();

    	deque.pop_back();

		lk.unlock();

		std::cout << "[WorkQueue::consumer] processing item - fd=" << i.fd << std::endl;
		// process Item i

	}
end:
	std::cout << "[WorkQueue::consumer] stopping consumer thread" << std::endl;

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

	std::cout << "WorkQueue::stop" << std::endl;

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
