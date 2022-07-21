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
		//writeQueue{},
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

SocketData &ThreadedServer2::getSocketData(int fd, bool usemutex) {
	if (usemutex) {
		std::unique_lock<std::mutex> lk(internalSocketDataMutex);
		return internalSocketData[fd];
	} else {
		return internalSocketData[fd];
	}
}

/*
 * allocate internal structure for socket
 * cleanup internal structure for socket (for successive reuse)
 *
 *
 */

void ThreadedServer2::internalCallback(IGenericServer * srv, int fd, enum job_type_t job_type) {


	switch (job_type) {
	case NEW_SOCKET:
		{
			// create new SocketData instance and map fd to it in internalSocketData

			// SocketData: cannot copy or move because has mutex
			//internalSocketData.insert(std::pair<int, SocketData>{fd, SocketData(fd)});
			//internalSocketData.emplace(fd);


			// this creates instance of SocketData, if no value is associated to key
			getSocketData(fd);

//			// this works too
//			std::unique_lock<std::mutex> lk(internalSocketDataMutex);
//			// not necessary, [] operator creates new instance in case
//			// https://stackoverflow.com/a/42001375/974287
//			internalSocketData.emplace(std::piecewise_construct, std::make_tuple(fd), std::make_tuple());
//
//			lk.unlock();
		}

		break;
	case CLOSE_SOCKET:

		callback_function(this, fd, job_type);
//		break;

//		LIB_LOG(info)	<< "[server][my_listener] CLOSE_SOCKET " << fd;
//		//close(fd);
//		srv->close(fd);

		break;
	case AVAILABLE_FOR_WRITE: {
			LIB_LOG(info)	<< "[ThreadedServer2][internalCallback] AVAILABLE_FOR_WRITE fd=" << fd;
			// add socket to ready to write list

			std::unique_lock<std::mutex> lk(readyToWriteMutex);
			readyToWriteSet.insert(fd);
			lk.unlock();

			// notify worker thread waiting on condition variable
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

	// setup internalSocketData

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

	// we use readyToWriteMutex because writerWorkerThread uses this with the condition variable
	std::unique_lock<std::mutex> lk(readyToWriteMutex);
	stopServer = false;
	lk.unlock();
	// wake up thread, in case it is waiting on cv
	readyToWriteCv.notify_one();

	// terminate the server thread
	server.terminate();

	// join threads

	LIB_LOG(info) << "ThreadedServer2::stop before listenWorkerThread.join()";
	listenWorkerThread.join();

	LIB_LOG(info) << "ThreadedServer2::stop before writerWorkerThread.join()";
	writerWorkerThread.join();

}


ssize_t ThreadedServer2::write(int fd, const char * data, ssize_t data_size) {

	// are there buffers waiting to be written on fd?
	// if yes, add this buffer to the write queue

	// if not:
	//    call write:
	//      if write is partially successful, add remaining data to write queue;
	//      if write is not successful (EAGAIN or EWOULDBLOCK), add data to write queue;

	ssize_t original_data_size = data_size;

	SocketData &sd = getSocketData(fd);

	// get lock on write queue;
	// all the subsequent operations on write queue are atomic thanks to the critical section
	std::unique_lock<std::mutex> lk(sd.getWriteQueueMutex());

	// get write queue
	auto writeQueue = sd.getWriteQueue();

	bool isWriteQueueEmpty = writeQueue.size() == 0; // is write queue empty?

	LIB_LOG(info) << "ThreadedServer2::write() isWriteQueueEmpty=" << isWriteQueueEmpty;

	if (isWriteQueueEmpty) {
		// write queue is empty, let's try to write to fd
		ssize_t bytesWritten = IGenericServer::write(fd, data, data_size);

		if (bytesWritten == data_size) {
			LIB_LOG(info) << "ThreadedServer2::write() write: ALL data written! fd=" << fd << " bytesWritten=" << bytesWritten;
			// ok! all data has been written
			return bytesWritten;
		} else if (bytesWritten == -1) {
			// EAGAIN or EWOULDBLOCK (fd not available to write)
			LIB_LOG(info) << "ThreadedServer2::write() write: NO data written! fd=" << fd;

			goto add_to_write_queue;
		} else if (bytesWritten < data_size) {
			// partially successful, add data which has not been written to write queue

			LIB_LOG(info) << "ThreadedServer2::write() write: PARTIAL data written! fd=" << fd << " bytesWritten=" << bytesWritten;

			data += bytesWritten;
			data_size -= bytesWritten;

			goto add_to_write_queue;
		}
	}
	//else {
		// write queue is non zero, add buffer (see below)
	//}

add_to_write_queue:
	// add buffer to write queue (copy data)

	SocketData::WriteItem wi;
	//wi.fd = fd;
	wi.data_size = data_size;
	wi.data = (char *) malloc(data_size);

	memcpy(wi.data, data, data_size);

	writeQueue.push_front(wi);

	ssize_t wqsize = writeQueue.size();

	// release lock on writeQueue; end of critical section
	lk.unlock();

	LIB_LOG(info) << "ThreadedServer2::write() added buffer to write queue fd=" << fd << " write queue size=" << wqsize;

	return original_data_size;
}



// IGenericServer::close
void ThreadedServer2::close(int fd) {

	LIB_LOG(info) << "ThreadedServer2::close() fd=" << fd;

	// TODO: cleanup internal data associated to fd

	// maybe do we need a general mutex for each socket, not stored in the internal structure?

	// we need to be sure that no other thread is working with fd
	// while we remove and destroy the internal data associated to fd

	// 0 - get lock on internalSocketData
	// 1 - get and lock internal structure associated to fd
	// 2 - while holding lock to internal structure:
	//     get lock on readyToWriteSet and remove fd from set
	// 3 - destroy internal structure

	std::unique_lock<std::mutex> lk0(internalSocketDataMutex);
	// get internal data associated to the socket
	SocketData &sd = internalSocketData[fd];
	lk0.unlock();

	// lock internal data associated to the socket
	sd.lock();

	sd.cleanup(false);

	// close socket while sd is locked
	server.close(fd);

	sd.release();

	LIB_LOG(info) << "ThreadedServer2::close() complete fd=" << fd;


}

void ThreadedServer2::writeQueueWorker() {
	LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] starting";

	while (true) {

		// wait on condition variable
		std::unique_lock<std::mutex> lk(readyToWriteMutex);

		// check if thread has to stop
		if (stopServer) {
			LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] ending";
			return;
		}

		LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] before readyToWriteCv.wait";

		// while server is running and readyToWriteSet is empty, wait on condition variable
		while (!stopServer && readyToWriteSet.empty()) {
			readyToWriteCv.wait(lk);
		}

		LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] after readyToWriteCv.wait";

		// check if thread has to stop
		if (stopServer) {
			LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] ending";
			return;
		}

		// we are still holding the lock on the set;
		// extract (and remove) node from set
		auto node = readyToWriteSet.extract(readyToWriteSet.begin());

		// release lock on the set; we can work on socket without holding readyToWriteMutex,
		// so that other threads can add items to the set
		lk.unlock();

		// get file descriptor of socket which is available to write
		int fd = node.value();

		// at this point, we could have a fd which has been closed, with an associated internal
		// structure (SocketData) which has an empty write list;
		// the write list is empty because it has been emptied by the close method.
		// so we can proceed even if fd is closed.

		LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] fd available to write: " << fd;

		// get internal data associated to the socket
		SocketData &sd = getSocketData(fd);

		sd.lock();

		// get write queue
		auto writeQueue = sd.getWriteQueue();

		LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] fd=" << fd << " writeQueue.size()=" << writeQueue.size();

		while (writeQueue.size() > 0) {
			// try to write buffer on write queue:

			// get buffer on the back of the queue
			SocketData::WriteItem item = writeQueue.back();
			writeQueue.pop_back(); // remove item from back of queue

			// try to write buffer
			LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] write2 on fd=" << fd;
			ssize_t res = write2(fd, sd, item, writeQueue);

			// socket is no more available to write
			if (res == -1) {
				break;
			}
		}

		sd.release();

	}




	LIB_LOG(info) << "ThreadedServer2::writeQueueWorker() ending";
}

/**
 * when called, we must hold the lock to sd (with sd.lock)
 *
 * item can be reused i.e. inserted again at the back of the queue
 *
 * return -1 when socket is no more available to write
 */
ssize_t ThreadedServer2::write2(int fd, SocketData &sd, SocketData::WriteItem &item, std::deque<SocketData::WriteItem> &writeQueue /*int fd, const char * data, ssize_t data_size*/) {


	//    call write:
	//      if write is completely successful, ok! return value (>= 0)
	//      if write is partially successful, add remaining data to write queue; return -1;
	//      if write is not successful (EAGAIN or EWOULDBLOCK), add data to write queue; return -1;

	// write queue is empty, let's try to write to fd
	ssize_t bytesWritten = IGenericServer::write(fd, item.data, item.data_size);

	if (bytesWritten == item.data_size) {
		// ok! all data has been written
		return bytesWritten;
	} else if (bytesWritten == -1) {
		// EAGAIN or EWOULDBLOCK (fd not available to write)

	} else if (bytesWritten < item.data_size) {
		// partially successful, add data which has not been written to write queue

		item.data += bytesWritten;
		item.data_size -= bytesWritten;
	}

	// add buffer to the back of write queue

	writeQueue.push_back(item);

	ssize_t wqsize = writeQueue.size();

	LIB_LOG(info) << "ThreadedServer2::write2() added buffer to back of write queue fd=" << fd << " write queue size=" << wqsize;

	return -1;
}


} /* namespace nbuss_server */
