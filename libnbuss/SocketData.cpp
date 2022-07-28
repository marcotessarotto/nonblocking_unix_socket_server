#include "SocketData.h"

#include "Logger.h"

namespace nbuss_server {

SocketData::SocketData() :
		uses(0)  {
	LIB_LOG(trace) << "SocketData::SocketData";
}

SocketData::~SocketData() {

	//writeQueue.clear();
}

void SocketData::_cleanup() {
	uses = 0;
	writeQueue.clear();
}

void SocketData::cleanup(bool useLock) {
	if (useLock) {
		std::unique_lock<std::mutex> lk(usesMutex);
		_cleanup();
	} else {
		_cleanup();
	}
}


void SocketData::lock() {
	std::unique_lock<std::mutex> lk(usesMutex);
	while (uses > 0) {
		usesCv.wait(lk);
	}
	uses++;
}

void SocketData::release() {
	std::unique_lock<std::mutex> lk(usesMutex);
	uses--;
	lk.unlock();
	usesCv.notify_one();
}


} /* namespace nbuss_server */
