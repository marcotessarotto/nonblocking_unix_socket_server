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

	// use writeQueueMutex to synchronize
	bool doNotUse; // if true, this instance is marked for destruction

	/// number of uses; 0 = not used
	int uses; // >= 0
	std::mutex usesMutex;
	std::condition_variable usesCv;

	// in the future, when it becomes available:
	// std::binary_semaphore uses_semaphore;


	/// queue of write items waiting to be written (when respective fd becomes available to write)
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
	 * if doNotUse is false, increase uses and return true;
	 * else return false
	 */
	bool incUse();

	/**
	 * if doNotUse is false: if uses > 0, decrease uses and return uses, else return 0
	 * if doNotUse is true return -1
	 */
	int decUse();

	/**
	 * if doNotUse is false, return uses
	 * else return -1
	 */
	int getUses();


	/**
	 * set instance as do not use; wait until operation can be done
	 */
	void setDoNotUse();

	bool isDoNotUser();

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_SOCKETDATA_H_ */
