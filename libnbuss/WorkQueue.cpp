#include "WorkQueue.h"

#include "Logger.h"

namespace nbuss_server {

static std::atomic<int> producedItems;
static std::atomic<int> consumedItems;

WorkQueue::WorkQueue(ThreadedServer2 & threadedServer, unsigned int numberOfThreads) :
		threaded_server{threadedServer},
		number_consumer_threads(numberOfThreads),
		callback_function{},
		deque{},
		consumers_must_terminate{false} {
	if (numberOfThreads < 1) {
		throw std::invalid_argument("numberOfThreads must be > 0");
	}
}

WorkQueue::~WorkQueue() {

}

void WorkQueue::producer_callback(IGenericServer::ListenEvent &&listen_event) {
	int num;

	Item i{listen_event.fd, listen_event.job};

	std::unique_lock<std::mutex> lk(deque_mutex);

	deque.push_front(i);
	num = deque.size();

	lk.unlock();

	deque_cv.notify_one();
	//producedItems++;

	LIB_LOG(trace) << "[WorkQueue::callback] [Producer] items in deque size=" << num;

}

void WorkQueue::consumer() {

	LIB_LOG(info) << "[WorkQueue::consumer] starting consumer thread";

	std::unique_lock<std::mutex> lk{deque_mutex, std::defer_lock};

	while (consumers_must_terminate == false) {

		//std::unique_lock<std::mutex> lk(dequeMutex);
		lk.lock();

        while (deque.size() == 0) {
        	deque_cv.wait(lk);

        	if (consumers_must_terminate) {
        		goto end;
        	}

        	LIB_LOG(trace) << "[WorkQueue::consumer] itemsInDeque=" << deque.size();
        }

        Item &i = deque.back();

    	deque.pop_back();

		lk.unlock();

		LIB_LOG(trace) << "[WorkQueue::consumer] processing item - fd=" << i.fd << " process=" << i.process;

		if (i.process) {
			// process Item i
			callback_function(IGenericServer::ListenEvent{this, i.fd, i.job, 0});
		}

		/*
		 * note: if fd is closed, subsequent events relative to fd should be removed (before closing fd)
		 */

	}
end:
LIB_LOG(info) << "[WorkQueue::consumer] stopping consumer thread";

}


void WorkQueue::start(std::function<void(IGenericServer::ListenEvent &&listen_event)> callback_function) {

	LIB_LOG(info) << "WorkQueue::start()";

	consumers_must_terminate = false;

	this->callback_function = callback_function;

	consumer_threads.resize(number_consumer_threads);
	for (int i = 0; i < number_consumer_threads; i++) {
		consumer_threads[i] = std::thread(&WorkQueue::consumer, this);
	}

	// https://stackoverflow.com/questions/42799208/perfect-forwarding-in-a-lambda
	auto f = [this](auto&& x){
		this->producer_callback(std::forward<decltype(x)>(x));
	};

	threaded_server.start(f);

//	threaded_server.start(
//		[this](IGenericServer::ListenEvent && _listen_event) {
//				this->producer_callback(std::forward<IGenericServer::ListenEvent &&>(_listen_event));
//			}
//		);


	LIB_LOG(info) << "WorkQueue::start() complete";
}

void WorkQueue::stop() {

	LIB_LOG(info) << "WorkQueue::stop()";

	consumers_must_terminate = true;

	threaded_server.stop();

	for (int i = 0; i < number_consumer_threads; i++) {
		deque_cv.notify_one();
	}

	for (int i = 0; i < number_consumer_threads; i++) {
		consumer_threads[i].join();
	}

	// cleanup queue
	deque.clear();
	consumer_threads.clear();

	LIB_LOG(info) << "WorkQueue::stop() complete";
}


void WorkQueue::close(int fd) {
	LIB_LOG(info) << "WorkQueue::close() fd=" << fd;

	std::unique_lock<std::mutex> lk(deque_mutex);

	// enumerate items in deque and mark as do not process items belonging to fd
    for (auto it = deque.rbegin(); it != deque.rend(); ++it) {
        if (it->fd == fd) {
        	it->process = false;
        }
    }

	lk.unlock();

	threaded_server.close(fd);
}

void WorkQueue::remove_from_epoll(int fd) {
	threaded_server.getServer().remove_from_epoll(fd);
}


} /* namespace nbuss_server */
