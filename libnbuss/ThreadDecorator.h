#ifndef LIBNBUSS_THREADDECORATOR_H_
#define LIBNBUSS_THREADDECORATOR_H_

#include <thread>

#include "IGenericServer.h"
#include "IThreadable.h"

namespace nbuss_server {

/**
 * decorator which encapsulates an IGenericServer instance; start method creates a new thread which then calls listen method
 *
 * IThreadable extends IGenericServer by adding start and stop method (for controlling the server thread)
 *
 * the idea is to extend the same virtual class so to maintain the same methods but at the same
 * time to extend the behavior (start and stop methods).
 *
 */
class ThreadDecorator : public virtual IThreadable {
	IGenericServer &server;

	//bool terminate_worker;
	bool worker_is_running;

	std::thread workerThread;

	std::function<void(int, enum job_type_t )> callback_function;

	void mainLoopWorker();

public:
	ThreadDecorator(IGenericServer &server);

	virtual ~ThreadDecorator();

    /**
     * listen for incoming connections; on incoming data, call callback_function
     *
     * returns only when another thread calls terminate method
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are: socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	virtual void listen(std::function<void(int, enum job_type_t )> callback_function);


    /**
     * starts a new thread which will listen for incoming connections and process them
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are: socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	virtual void start(std::function<void(int, enum job_type_t )> callback_function);

	/**
	 * terminate server instance
	 */
	virtual void terminate();

	/**
	 * wait for server to start listening for incoming connections
	 */
	virtual void waitForServerReady();

	/**
	 * terminate server instance and waits for thread to stop
	 */
	virtual void stop();
};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_THREADDECORATOR_H_ */
