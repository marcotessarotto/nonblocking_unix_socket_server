#ifndef LIBNBUSS_THREADEDSERVER2_H_
#define LIBNBUSS_THREADEDSERVER2_H_

#include <atomic>
#include <thread>
#include <queue>
#include <deque>
#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <set>
#include <map>

#include "IGenericServer.h"
#include "IThreadable.h"

#include "SocketData.h"

namespace nbuss_server {

/**
 * this class implements both a dedicated thread for the listen method of server
 * and a dedicated thread for managing AVAILABLE_FOR_WRITE (EPOLLOUT) events
 */
class ThreadedServer2  : public virtual IThreadable {
protected:
	IGenericServer &server;

	/// if true, the server is running
	bool running;

	/// if true, the server must stop
	std::atomic<bool> stopServer;

	/// thread which calls listen method of IGenericServer
	std::thread listenWorkerThread;

	/// thread which runs the write queue worker (see function writeQueueWorker)
	std::thread writerWorkerThread;

	std::function<void(IGenericServer::ListenEvent &&listen_event)> callback_function;

	void listenWorker();

	void writeQueueWorker();

	void internalCallback(IGenericServer::ListenEvent &&listen_event);


	std::map<int, SocketData> internalSocketData;
	std::mutex internalSocketDataMutex;


	SocketData & getSocketData(int fd, bool use_mutex = true);


	/// set of file descriptors ready to write
	std::set<int> readyToWriteSet;
	/// mutex for access to readyToWrite
	std::mutex readyToWriteMutex;
	std::condition_variable readyToWriteCv;

	/// caller must hold lock writeQueueMutex when calling this function
	ssize_t write_holding_lock(int fd, SocketData &sd, SocketData::WriteItem &item, std::deque<SocketData::WriteItem> &writeQueue);

	// cleanup internal data associated to fd
	void cleanup(int fd);

public:
	ThreadedServer2(IGenericServer &server);
	virtual ~ThreadedServer2();

    /**
     * starts a new thread which will listen for incoming connections and process them
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are:
     *    pointer to server instance of type ThreadedServer2, socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	virtual void start(std::function<void(IGenericServer::ListenEvent && listen_event)> callback_function) override;

	/**
	 * terminate server instance and waits for thread to stop
	 */
	virtual void stop() override;

	/**
	 * invoke the write system call; return the number of bytes written on success,
	 * or std::runtime_error in case of error
	 */
	ssize_t write(int fd, const char * data, ssize_t data_size);

	/**
	 * write data to the socket
	 */
	template <class T>
	ssize_t write(int fd, std::vector<T> &data) {
		ssize_t data_size = data.size() * sizeof(T);
		const char * p =  reinterpret_cast<char*>(data.data());

		LIB_LOG(debug) << "ThreadedServer2::Write<> data_size = " << data_size << " sizeof(T)=" << sizeof(T);

		return write(fd, p, data_size);
	}

	// read method is inherited by IGenericServer

	/**
	 * close socket and remove internal structures
	 */
	void close(int fd);

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_THREADEDSERVER2_H_ */
