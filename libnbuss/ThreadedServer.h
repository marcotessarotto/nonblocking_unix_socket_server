#ifndef LIBNBUSS_THREADEDSERVER_H_
#define LIBNBUSS_THREADEDSERVER_H_

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
class ThreadedServer : public virtual IThreadable {

protected:
	IGenericServer &server;

	//bool terminate_worker;
	bool worker_is_running;

	std::thread workerThread;

	std::function<void(ListenEvent && listen_event)> callback_function;

	void listen_worker();

public:
	ThreadedServer(IGenericServer &server);

	virtual ~ThreadedServer();

    /**
     * starts a new thread which will listen for incoming connections and process them
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are: socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	virtual void start(std::function<void(ListenEvent && listen_event)> callback_function) override;

	/**
	 * terminate server instance and waits for thread to stop
	 */
	virtual void stop() override;
};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_THREADEDSERVER_H_ */
