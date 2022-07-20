#include "ThreadedServer2.h"
#include <iostream>
#include <string>
#include <thread>

#include "Logger.h"


namespace nbuss_server {

ThreadedServer2::ThreadedServer2(IGenericServer &server) :
		running{false},
		stopServer{false},
		listenWorkerThread{},
		writerWorkerThread{},
		readyToWriteSet{},
		writeQueue{},
		server{server} {

}

ThreadedServer2::~ThreadedServer2() {
	LIB_LOG(info) << "ThreadedServer2::~ThreadedServer2";
}

void ThreadedServer2::listenWorker() {
	LIB_LOG(info) << "ThreadedServer2::mainLoopWorker start";

	// internalCallback

	// listen returns when another thread calls terminate
	server.listen([this](IGenericServer * srv, int fd, enum job_type_t job_type) {
		this->internalCallback(srv, fd, job_type);
	});

	// thread ends
	LIB_LOG(info) << "ThreadedServer2::mainLoopWorker end";
}

void ThreadedServer2::internalCallback(IGenericServer * srv, int fd, enum job_type_t job_type) {

	switch (job_type) {
	case NEW_SOCKET:
		// TODO: init structure associated to socket
		// structure contains: write buffer


		break;
	case CLOSE_SOCKET:

		callback_function(this, fd, job_type);
		break;

//		LIB_LOG(info)	<< "[server][my_listener] CLOSE_SOCKET " << fd;
//		//close(fd);
//		srv->close(fd);

		break;
	case AVAILABLE_FOR_WRITE: {
			LIB_LOG(info)	<< "[ThreadedServer2][internalCallback] AVAILABLE_FOR_WRITE fd=" << fd;
			// TODO: check if there are buffers to write to this socket

			std::unique_lock<std::mutex> lk(readyToWriteMutex);
			readyToWriteSet.insert(fd);
			lk.unlock();

			// notify thread waiting on condition variable
			readyToWriteCv.notify_one();
		}
		break;
	case AVAILABLE_FOR_READ:
		LIB_LOG(info)	<< "[ThreadedServer2][internalCallback] AVAILABLE_FOR_READ fd=" << fd;

		callback_function(this, fd, job_type);

		break;

	}

	LIB_LOG(debug)	<< "[server][my_listener] ending - fd=" << fd;
}

void ThreadedServer2::start(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function) {

	std::unique_lock<std::mutex> lk(mtx);
	bool _running = running;
	lk.unlock();

	if (_running) {
		LIB_LOG(error) << "server is already running";
		throw std::runtime_error("server is already running");
	}

	readyToWriteSet.clear();

	this->callback_function = callback_function;

	// start writeQueueWorker
	try {
		writerWorkerThread = std::thread{&ThreadedServer2::writeQueueWorker, this};
	} catch (const std::exception &e) {
		LIB_LOG(error)	<< "[ThreadedServer2::start] writeQueueWorker exception: " << e.what();
	}

	try {
		listenWorkerThread = std::thread{&ThreadedServer2::listenWorker, this};
	} catch (const std::exception &e) {
		LIB_LOG(error)	<< "[ThreadedServer2::start] listenWorker exception: " << e.what();
	}

	// return when server is ready i.e. listening for incoming connections
	server.waitForServerReady();

}

void ThreadedServer2::stop() {

	LIB_LOG(info) << "ThreadedServer2::stop";

	std::unique_lock<std::mutex> lk(mtx);
	stopServer = false;
	lk.unlock();

	// terminate the server thread
	server.terminate();

	// join threads
	listenWorkerThread.join();

	writerWorkerThread.join();

}


ssize_t ThreadedServer2::write(int fd, const char * data, ssize_t data_size) {

	// are there buffers waiting to be written on fd?
	// if yes, add this buffer to the work queue

	// if not: call write:
	//    if partially successful, add remaining data to work queue;
	//    if not successful (EAGAIN or EWOULDBLOCK), add data to work queue;

	ssize_t original_data_size = data_size;

	std::unique_lock<std::mutex> lk(writeQueueMutex);

	bool fdIsInWriteQueue = isFdInWriteQueue(fd);

	LIB_LOG(info) << "ThreadedServer2::write() fdIsInWriteQueue=" << fdIsInWriteQueue;

	if (!fdIsInWriteQueue) {
		// fd is not present in write queue, let's try to write to
		ssize_t bytesWritten = IGenericServer::write(fd, data, data_size);

		if (bytesWritten == data_size) {
			// ok! all data has been written
			return bytesWritten;
		} else if (bytesWritten == -1) {
			// EAGAIN or EWOULDBLOCK (fd not available to write)

			// remove fd from readyToWrite

			goto add_to_write_queue;
		} else if (bytesWritten < data_size) {
			// partially successful, add data which has not been written to write queue

//			WriteItem wi;
//			wi.fd = fd;
//			wi.data_size = data_size - bytesWritten;
//			wi.data = (char *) malloc(wi.data_size);
//
//			memcpy(wi.data, &data[bytesWritten], wi.data_size);
//
//			writeQueue.push_front(wi);

			data += bytesWritten;
			data_size -= bytesWritten;

			goto add_to_write_queue;
		}
	}
	//else {
		// fd is present in write queue, add buffer (see below)
	//}

add_to_write_queue:
	// add buffer to write queue (copy data)

	WriteItem wi;
	wi.fd = fd;
	wi.data_size = data_size;
	wi.data = (char *) malloc(data_size);

	memcpy(wi.data, data, data_size);

	writeQueue.push_front(wi);

	lk.unlock();

	LIB_LOG(info) << "ThreadedServer2::write() added buffer to write queue";

	return original_data_size;
}

/**
 * check if fd is present in writeQueue
 */
bool ThreadedServer2::isFdInWriteQueue(int fd) {

    for (auto it = writeQueue.cbegin(); it != writeQueue.cend(); ++it) {
    	auto item = *it;

    	if (item.fd == fd) {
    		return true;
    	}
    }
    return false;
}

void ThreadedServer2::writeQueueWorker() {
	LIB_LOG(info) << "ThreadedServer2::writeQueueWorker() starting";


	// wait on condition variable
	std::unique_lock<std::mutex> lk(readyToWriteMutex);
	while (readyToWriteSet.empty()) {
		readyToWriteCv.wait(lk);
	}
	lk.unlock();

	// enumerate writeQueue and check if fd is in readyToWrite set
	std::unique_lock<std::mutex> lk2(writeQueueMutex);

    for (auto it = writeQueue.cbegin(); it != writeQueue.cend(); ++it) {

    	auto item = *it;

    	// checl if item.fd is in readyToWrite set
    	lk.lock();
    	bool isFdInReadyToWriteSet = readyToWriteSet.count(item.fd) > 0;

    	if (isFdInReadyToWriteSet) {
    		// try to write buffer
    	}

    	lk.unlock();



    }

	lk2.unlock();
	//readyToWrite.insert(fd);


	// notify thread waiting on condition variable
	readyToWriteCv.notify_one();


	LIB_LOG(info) << "ThreadedServer2::writeQueueWorker() ending";
}


} /* namespace nbuss_server */
