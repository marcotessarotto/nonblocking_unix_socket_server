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

#include "IGenericServer.h"
#include "IThreadable.h"

namespace nbuss_server {

/**
 * this class should implement both a dedicated thread for the listen method of server
 * and a dedicated thread for managing AVAILABLE_FOR_WRITE (EPOLLOUT) events
 */
class ThreadedServer2  : public virtual IThreadable {
protected:
	IGenericServer &server;

	/// if true, the server is running
	bool running;

	/// if true, the server must stop
	bool stopServer;

	/// thread which calls listen method of IGenericServer
	std::thread listenWorkerThread;

	/// thread which runs the write queue worker
	std::thread writerWorkerThread;

	std::function<void(IGenericServer *,int, enum job_type_t )> callback_function;

	void listenWorker();

	void writeQueueWorker();

	void internalCallback(IGenericServer * srv, int fd, enum job_type_t job_type);

	/// set of file descriptors ready to write
	std::set<int> readyToWrite;
	/// mutex for access to readyToWrite
	std::mutex readyToWriteMutex;


	struct WriteItem {
		int fd;
		char * data;
		ssize_t data_size;
	};

	/// queue of write items waiting to be written (when respective fd becomes available to write)
	std::deque<WriteItem> writeQueue;

	std::mutex writeQueueMutex;

	/// caller must hold lock writeQueueMutex when calling this function
	/// check if fd is present in writeQueue
	bool isFdInWriteQueue(int fd);


public:
	ThreadedServer2(IGenericServer &server);
	virtual ~ThreadedServer2();

    /**
     * starts a new thread which will listen for incoming connections and process them
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are: socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	virtual void start(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function);

	/**
	 * terminate server instance and waits for thread to stop
	 */
	virtual void stop();


	/**
	 * read all available data from socket and return vector of vectors
	 */
	//static std::vector<std::vector<char>> read(int fd, size_t readBufferSize = 4096);

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

		LIB_LOG(info) << "ThreadedServer2::Write<> data_size = " << data_size << " sizeof(T)=" << sizeof(T);

		return write(fd, p, data_size);
	}


};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_THREADEDSERVER2_H_ */
