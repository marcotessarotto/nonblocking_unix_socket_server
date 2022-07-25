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
#include <set>

#include "IGenericServer.h"
#include "IThreadable.h"
#include "ThreadedServer.h"

namespace nbuss_server {

/**
 * WorkQueue implements a work queue where incoming traffic events from server are stored.
 * constructor takes an instance of ThreadedServer as a parameter.
 *
 * to be processed by one or more consumers
 *
 */

class WorkQueue {
public:

	struct Item {
		//IGenericServer * srv;
		int fd;
		enum job_type_t job;
		bool process = true;
	};

private:
	ThreadedServer &threadedServer;
	unsigned int numberOfThreads;

	std::deque<Item> deque;
	std::mutex dequeMutex;
	std::condition_variable dequeCv;

	std::atomic<bool> stopConsumers;

	std::vector<std::thread> consumerThreads;

	std::function<void(WorkQueue *,int, enum job_type_t )> callback_function;

	/// callback used as producer
	void producerCallback(IGenericServer * srv, int fd, enum job_type_t job);

	/// consumer
	void consumer();

public:
	WorkQueue(ThreadedServer & threadDecorator, unsigned int numberOfThreads = 1);
	virtual ~WorkQueue();

	/// start server
	void start(std::function<void(WorkQueue *, int, enum job_type_t )> callback_function);

	/// stop server
	void stop();

	/// remove items from work queue belonging to socket and then close it
	void close(int fd);

	/**
	 * remove socket from epoll watch list
	 */
	void remove_from_epoll(int fd);

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_WORKQUEUE_H_ */
