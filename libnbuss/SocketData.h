#ifndef LIBNBUSS_SOCKETDATA_H_
#define LIBNBUSS_SOCKETDATA_H_

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

namespace nbuss_server {

/**
 *
    a FIFO list of buffers waiting to be written to socket (when it becomes available to be written)

    a list of buffers which have been read and are being stored here temporary i.e. before being parsed by the application using this library
 *
 */
class SocketData {
public:
	struct WriteItem {
		//int fd;
		char * data;
		ssize_t data_size;
	};



private:

	/// number of uses; 0 = not used
	int uses; // >= 0
	std::mutex usesMutex;
	std::condition_variable usesCv;


	/// queue of write items waiting to be written (when owner fd becomes available to write)
	std::deque<WriteItem> writeQueue;
	std::mutex writeQueueMutex;

	void _cleanup();
public:
	SocketData();
	virtual ~SocketData();

	std::mutex &getWriteQueueMutex() { return writeQueueMutex; }

	std::deque<WriteItem> &getWriteQueue() { return writeQueue; }

	void cleanup(bool useLock = true);

	/**
	 * get lock on instance; if locked by another thread,
	 * blocks until lock is successful
	 */
	void lock();

	/**
	 * release lock on instance
	 */
	void release();

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_SOCKETDATA_H_ */
