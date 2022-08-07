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

	auto f = [this](auto&& x){
		this->internalCallback(std::forward<decltype(x)>(x));
	};

	server.listen(f);

	// listen returns when another thread calls terminate
//	server.listen([this](ListenEvent &&listen_event) {
//		this->internalCallback(listen_event);
//	});

	// thread ends
	LIB_LOG(info) << "ThreadedServer2::mainLoopWorker end";
}

SocketData &ThreadedServer2::getSocketData(int fd, bool use_mutex) {
	// operator[] returns a reference to the value that is mapped to a key equivalent to key,
	// performing an insertion if such key does not already exist.
	// https://en.cppreference.com/w/cpp/container/map/operator_at
	if (use_mutex) {
		std::unique_lock<std::mutex> lk(internalSocketDataMutex);
		return internalSocketData[fd];
	} else {
		return internalSocketData[fd];
	}
}

void ThreadedServer2::internalCallback(IGenericServer::ListenEvent &&listen_event) {



	switch (listen_event.job) {
	case AVAILABLE_FOR_READ_AND_WRITE: {
			LIB_LOG(info) << "[ThreadedServer2][internalCallback] AVAILABLE_FOR_READ_AND_WRITE fd=" << listen_event.fd;
			// add socket to ready to write list

			std::unique_lock<std::mutex> lk(readyToWriteMutex);
			readyToWriteSet.insert(listen_event.fd);
			lk.unlock();

			// notify worker thread waiting on condition variable
			readyToWriteCv.notify_one();

			listen_event.srv = this;
			listen_event.job = AVAILABLE_FOR_READ;

			//LIB_LOG(info) << "[ThreadedServer2][internalCallback] callback_function";
			//callback_function(IGenericServer::ListenEvent{this, listen_event.fd, AVAILABLE_FOR_READ, listen_event.events});

			// https://stackoverflow.com/questions/42799208/perfect-forwarding-in-a-lambda
			callback_function(std::forward<decltype(listen_event)>(listen_event));
		}
		break;
	case NEW_SOCKET:
		{
			// create new SocketData instance and map fd to it in internalSocketData

			// SocketData: cannot copy or move because has mutex
			//internalSocketData.insert(std::pair<int, SocketData>{fd, SocketData(fd)});
			//internalSocketData.emplace(fd);


			// this creates instance of SocketData, if no value is associated to key
			getSocketData(listen_event.fd);

			callback_function(std::forward<decltype(listen_event)>(listen_event));

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

		callback_function(std::forward<decltype(listen_event)>(listen_event));
		break;
	case AVAILABLE_FOR_WRITE: {
			LIB_LOG(info)	<< "[ThreadedServer2][internalCallback] AVAILABLE_FOR_WRITE fd=" << listen_event.fd;
			// add socket to ready to write list

			std::unique_lock<std::mutex> lk(readyToWriteMutex);
			readyToWriteSet.insert(listen_event.fd);
			lk.unlock();

			// notify worker thread waiting on condition variable
			readyToWriteCv.notify_one();
		}
		break;
	case AVAILABLE_FOR_READ:
		LIB_LOG(info)	<< "[ThreadedServer2][internalCallback] AVAILABLE_FOR_READ fd=" << listen_event.fd;

		callback_function(std::forward<decltype(listen_event)>(listen_event));

		break;
	case SOCKET_IS_CLOSED:
		LIB_LOG(info)	<< "[ThreadedServer2][internalCallback] SOCKET_IS_CLOSED fd=" << listen_event.fd;

		cleanup(listen_event.fd);

		callback_function(std::forward<decltype(listen_event)>(listen_event));

		break;
	}

	//LIB_LOG(debug)	<< "[server][my_listener] finished - fd=" << fd;
}

void ThreadedServer2::start(std::function<void(ListenEvent &&listen_event)> callback_function) {

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

	// start listenWorker
	try {
		listenWorkerThread = std::thread{&ThreadedServer2::listenWorker, this};
	} catch (const std::exception &e) {
		LIB_LOG(error)	<< "[ThreadedServer2::start] listenWorker exception: " << e.what();
	}

	// return when server is ready i.e. listening for incoming connections
	server.wait_for_server_ready();

}

void ThreadedServer2::stop() {

	LIB_LOG(info) << "ThreadedServer2::stop";


	//LIB_LOG(info) << "ThreadedServer2::stop before stopServer = false;";
	// we use readyToWriteMutex because writerWorkerThread uses this with the condition variable
	std::unique_lock<std::mutex> lk(readyToWriteMutex);
	stopServer = true;
	lk.unlock();
	// wake up thread, in case it is waiting on cv
	readyToWriteCv.notify_one();

	// terminate the server thread
	//LIB_LOG(info) << "ThreadedServer2::stop before server.terminate()";
	server.terminate();

	readyToWriteCv.notify_one();

	// join threads

	LIB_LOG(trace) << "ThreadedServer2::stop before listenWorkerThread.join()";
	listenWorkerThread.join();

	LIB_LOG(trace) << "ThreadedServer2::stop before writerWorkerThread.join()";
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

	LIB_LOG(debug) << "ThreadedServer2::write() isWriteQueueEmpty=" << isWriteQueueEmpty;

	if (isWriteQueueEmpty) {
		// write queue is empty, let's try to write to fd
		ssize_t bytesWritten = IGenericServer::write(fd, data, data_size);

		if (bytesWritten == data_size) {
			LIB_LOG(trace) << "ThreadedServer2::write() write: ALL data written! fd=" << fd << " bytesWritten=" << bytesWritten;
			// ok! all data has been written
			return bytesWritten;
		} else if (bytesWritten == -1) {
			// EAGAIN or EWOULDBLOCK (fd not available to write)
			LIB_LOG(trace) << "ThreadedServer2::write() write: NO data written! fd=" << fd;

			goto add_to_write_queue;
		} else if (bytesWritten < data_size) {
			// partially successful, add data which has not been written to write queue

			LIB_LOG(trace) << "ThreadedServer2::write() write: PARTIAL data written! fd=" << fd << " bytesWritten=" << bytesWritten;

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

	LIB_LOG(trace) << "ThreadedServer2::write() added buffer to write queue fd=" << fd << " write queue size=" << wqsize;

	return original_data_size;
}

void ThreadedServer2::cleanup(int fd) {

	LIB_LOG(info) << "ThreadedServer2::cleanup() fd=" << fd;

	std::unique_lock<std::mutex> lk0(internalSocketDataMutex);
	// get internal data associated to the socket
	SocketData &sd = internalSocketData[fd];
	lk0.unlock();

	// get lock on internal data associated to the socket
	sd.lock();

	// cleanup internal data associated to fd
	sd.cleanup(false);

	sd.release();

	LIB_LOG(trace) << "ThreadedServer2::cleanup() finished fd=" << fd;
}

// IGenericServer::close
void ThreadedServer2::close(int fd) {

	LIB_LOG(info) << "ThreadedServer2::close() fd=" << fd;


	// no more events from fd
	server.remove_from_epoll(fd);

	// each SocketData instance has a semaphore

	// we need to be sure that no other thread is working with fd
	// while we remove and destroy the internal data associated to fd

	// 1 - remove fd from epoll list, so that no more events are received
	// 2 - get lock on internalSocketDataMutex
	// 3 - get internal structure associated to fd
	// 4 - release lock on internalSocketDataMutex

	// 5 - lock internal structure associated to fd
	// 6 - cleanup internal structure
	// 7 - close fd
	// 7 - release lock on internal structure (which remains associated to fd, for reuse)

	std::unique_lock<std::mutex> lk0(internalSocketDataMutex);
	// get internal data associated to the socket
	SocketData &sd = internalSocketData[fd];
	lk0.unlock();

	// get lock on internal data associated to the socket
	sd.lock();

	// cleanup internal data associated to fd
	sd.cleanup(false);

	// close socket while sd is locked
	server.close(fd);

	sd.release();

	LIB_LOG(info) << "ThreadedServer2::close() complete fd=" << fd;


}

void ThreadedServer2::writeQueueWorker() {
	LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] starting";

	std::unique_lock<std::mutex> lk(readyToWriteMutex, std::defer_lock);

	while (true) {

		lk.lock();

		// check if thread has to stop
		if (stopServer) {
			goto end;
//			LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] terminated";
//			return;
		}

		// while server is running and readyToWriteSet is empty, wait on condition variable
		while (!stopServer && readyToWriteSet.empty()) {
			readyToWriteCv.wait(lk);
		}

		// check if thread has to stop
		if (stopServer) {
			goto end;
//			LIB_LOG(info) << "[ThreadedServer2::writeQueueWorker()] terminated";
//			return;
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

		LIB_LOG(trace) << "[ThreadedServer2::writeQueueWorker()] fd available to write: " << fd;

		// get internal data associated to the socket
		SocketData &sd = getSocketData(fd);

		sd.lock();

		// get write queue
		auto &writeQueue = sd.getWriteQueue();

		LIB_LOG(trace) << "[ThreadedServer2::writeQueueWorker()] fd=" << fd << " writeQueue.size()=" << writeQueue.size();

		// we can work on socket data, including its write queue, because we have the lock on sd
		while (writeQueue.size() > 0) {
			// try to write buffer on write queue:

			// get buffer on the back of the queue
			SocketData::WriteItem item = writeQueue.back();
			writeQueue.pop_back(); // remove item from back of queue

			// try to write buffer
			LIB_LOG(trace) << "[ThreadedServer2::writeQueueWorker()] write2 on fd=" << fd;
			ssize_t res = write_holding_lock(fd, sd, item, writeQueue);

			// socket is no more available to write
			if (res == -1) {
				break;
			}
		}

		sd.release();

	}

end:
	LIB_LOG(info) << "ThreadedServer2::writeQueueWorker() terminated";
}

/**
 * when called, we must hold the lock to sd (with sd.lock)
 *
 * item can be reused i.e. inserted again at the back of the queue
 *
 * return -1 when socket is no more available to write
 */
ssize_t ThreadedServer2::write_holding_lock(int fd, SocketData &sd, SocketData::WriteItem &item, std::deque<SocketData::WriteItem> &writeQueue /*int fd, const char * data, ssize_t data_size*/) {


	//    call write:
	//      if write is completely successful, ok! return value (>= 0)
	//      if write is partially successful, add remaining data to write queue; return -1;
	//      if write is not successful (EAGAIN or EWOULDBLOCK), add data to write queue; return -1;

	// write queue is empty, let's try to write to fd
	ssize_t bytesWritten = IGenericServer::write(fd, item.data, item.data_size);

	if (bytesWritten == item.data_size) {
		// ok! all data has been written
		// no need to add to write queue
		return bytesWritten;
	} else if (bytesWritten == -1) {
		// EAGAIN or EWOULDBLOCK (fd not available to write)
		// add buffer to write queue
	} else if (bytesWritten < item.data_size) {
		// partially successful, add data which has not been written to write queue

		item.data += bytesWritten;
		item.data_size -= bytesWritten;
	}

	// add buffer to the back of write queue

	writeQueue.push_back(item);

	ssize_t wqsize = writeQueue.size();

	LIB_LOG(trace) << "ThreadedServer2::write_holding_lock() added buffer to back of write queue fd=" << fd << " write queue size=" << wqsize;

	return -1;
}


} /* namespace nbuss_server */
