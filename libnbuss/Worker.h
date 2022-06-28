#ifndef LIBNBUSS_WORKER_H_
#define LIBNBUSS_WORKER_H_

#include <thread>
#include <mutex>
#include <condition_variable>

#include "IGenericServer.h"

namespace nbuss_server {

/**
 * encapsulate an IGenericServer instance, creating a new thread which calls listen method
 */
class Worker {
	IGenericServer &server;

	bool terminate_worker;
	bool worker_is_running;

	std::mutex m;
	std::condition_variable cv;

	std::thread workerThread;

	std::function<void(int, enum job_type_t )> callback_function;

	void mainLoopWorker();

public:
	Worker(IGenericServer &server, std::function<void(int, enum job_type_t )> callback_function);
	virtual ~Worker();

    /**
     * starts a new thread which will listen for incoming connections and process them
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are: socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	virtual void start();

	/**
	 * terminate server instance
	 */
	virtual void terminate();
};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_WORKER_H_ */
