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

SocketData &ThreadedServer2::getSocketData(int fd) {
	return internalSocketData[fd];
}

void ThreadedServer2::internalCallback(IGenericServer * srv, int fd, enum job_type_t job_type) {


	switch (job_type) {
	case NEW_SOCKET:
		// create new SocketData instance and map fd to it in internalSocketData

		// SocketData: cannot copy or move because has mutex
		//internalSocketData.insert(std::pair<int, SocketData>{fd, SocketData(fd)});
		//internalSocketData.emplace(fd);


		// not necessary, [] operator creates new instance in case
		// https://stackoverflow.com/a/42001375/974287
		internalSocketData.emplace(std::piecewise_construct, std::make_tuple(fd), std::make_tuple());


		break;
	case CLOSE_SOCKET:

		//internalSocketData.erase(fd);

		callback_function(this, fd, job_type);
		break;

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
	listenWorkerThread.join();

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
			// ok! all data has been written
			return bytesWritten;
		} else if (bytesWritten == -1) {
			// EAGAIN or EWOULDBLOCK (fd not available to write)


			goto add_to_write_queue;
		} else if (bytesWritten < data_size) {
			// partially successful, add data which has not been written to write queue

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

	//
	IGenericServer::close(fd);
}

void ThreadedServer2::writeQueueWorker() {
	LIB_LOG(info) << "ThreadedServer2::writeQueueWorker() starting";

	while (true) {

		// wait on condition variable
		std::unique_lock<std::mutex> lk(readyToWriteMutex);

		// while server is running and readyToWriteSet is empty, wait on condition variable
		while (!stopServer && readyToWriteSet.empty()) {
			readyToWriteCv.wait(lk);
		}

		// check if thread has to stop
		if (stopServer) {
			LIB_LOG(info) << "ThreadedServer2::writeQueueWorker() ending";
			return;
		}

		// we are still holding the lock on the set;
		// extract (and remove) node from set
		auto node = readyToWriteSet.extract(readyToWriteSet.begin());

		// release lock on the set; we can work on socket without holding readyToWriteMutex,
		// so that other threads can add items to the set
		lk.unlock();

		// get file descriptor of socket which is ready to write to
		int fd = node.value();

		// get internal data associated to the socket
		SocketData &sd = getSocketData(fd);

		// get lock on internal data
		std::unique_lock<std::mutex> lk2(sd.getWriteQueueMutex());

		// get write queue
		auto writeQueue = sd.getWriteQueue();

		if (writeQueue.size() > 0) {
			// try to write buffer on write queue

			// get buffer on the back of the queue
			SocketData::WriteItem item = writeQueue.back();
			writeQueue.pop_back();

			// try to writebuffer

		}

		lk2.unlock();

	}




	LIB_LOG(info) << "ThreadedServer2::writeQueueWorker() ending";
}


} /* namespace nbuss_server */
