/*
 * QueueDecorator.cpp
 *
 *  Created on: Jul 16, 2022
 *      Author: marco
 */

#include <WorkQueue.h>

namespace nbuss_server {


WorkQueue::WorkQueue(ThreadDecorator & threadDecorator, unsigned int numberOfThreads) :
		threadDecorator{threadDecorator}, numberOfThreads(numberOfThreads) {
	if (numberOfThreads < 1) {
		throw std::invalid_argument("numberOfThreads must be > 0");
	}
}

WorkQueue::~WorkQueue() {

}

void WorkQueue::callback(IGenericServer * srv, int fd, enum job_type_t job) {

}

void WorkQueue::start() {
	threadDecorator.start(
			[this](IGenericServer * srv, int fd, enum job_type_t job) {
					this->callback(srv, fd, job);
				}
			);
}

void WorkQueue::stop() {
	threadDecorator.stop();
}

} /* namespace nbuss_server */
