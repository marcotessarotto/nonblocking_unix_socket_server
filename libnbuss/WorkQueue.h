#ifndef LIBNBUSS_WORKQUEUE_H_
#define LIBNBUSS_WORKQUEUE_H_

#include <iostream>
#include <atomic>
#include <thread>
#include <queue>
#include <deque>
#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "IGenericServer.h"
#include "IThreadable.h"
#include "ThreadedServer.h"

namespace nbuss_server {

/**
 * this is not a real decorator; constructor takes an instance of ThreadDecorator as a parameter
 * WorkQueue implements a work queue where incoming traffic events from server are stored
 * to be processed by one or more consumers
 *
 */

class WorkQueue {
public:

	struct Item {
		IGenericServer * srv;
		int fd;
		enum job_type_t job;
	};

private:
	ThreadedServer &threadedServer;
	unsigned int numberOfThreads;

	std::deque<Item> deque;
	std::mutex dequeMutex;
	std::condition_variable dequeCv;

	std::atomic<bool> stopConsumers;

	std::vector<std::thread> consumerThreads;

	std::function<void(IGenericServer *,int, enum job_type_t )> callback_function;

	/// callback used as producer
	void producerCallback(IGenericServer * srv, int fd, enum job_type_t job);

	/// consumer
	void consumer();

public:
	WorkQueue(ThreadedServer & threadDecorator, unsigned int numberOfThreads = 1);
	virtual ~WorkQueue();


	void start(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function);

	void stop();

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_WORKQUEUE_H_ */
