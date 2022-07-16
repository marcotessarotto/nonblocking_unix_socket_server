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

#include "IGenericServer.h"
#include "IThreadable.h"
#include "ThreadDecorator.h"

namespace nbuss_server {

/**
 * this is not a real decorator; constructor takes an instance of ThreadDecorator as a parameter
 * WorkQueue implements a work queue where incoming traffic events from server are stored
 * to be processed by one or more consumers
 *
 */

class WorkQueue {

	ThreadDecorator &threadDecorator;
	unsigned int numberOfThreads;


	void callback(IGenericServer * srv, int fd, enum job_type_t job);


public:
	WorkQueue(ThreadDecorator & threadDecorator, unsigned int numberOfThreads = 1);
	virtual ~WorkQueue();


	void start();

	void stop();

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_WORKQUEUE_H_ */
