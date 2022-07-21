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

	// use writeQueueMutex to synchronize
	bool doNotUse; // if true, this instance is marked for destruction

private:

	/// queue of write items waiting to be written (when respective fd becomes available to write)
	std::deque<WriteItem> writeQueue;
	std::mutex writeQueueMutex;

public:
	SocketData();
	virtual ~SocketData();

	std::mutex &getWriteQueueMutex() { return writeQueueMutex; }

	std::deque<WriteItem> &getWriteQueue() { return writeQueue; }

	//void cleanup();

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_SOCKETDATA_H_ */
