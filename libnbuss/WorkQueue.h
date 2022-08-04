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
#include "ThreadedServer2.h"

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
	/// instance of server (with async write support)
	ThreadedServer2 &threaded_server;

	/// number of consumer threads
	unsigned int number_consumer_threads;

	std::deque<Item> deque;
	std::mutex deque_mutex;
	std::condition_variable deque_cv;

	std::atomic<bool> consumers_must_terminate;

	std::vector<std::thread> consumer_threads;

	std::function<void(WorkQueue *,int, enum job_type_t )> callback_function;

	/// callback (invoked by threadedServer) which is the events producer
	void producer_callback(IGenericServer * srv, int fd, enum job_type_t job);

	/// consumer
	void consumer();

public:
	WorkQueue(ThreadedServer2 & threadedServer, unsigned int num_consumer_threads = 1);
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

	ThreadedServer2 &getServer() { return threaded_server; }

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_WORKQUEUE_H_ */
