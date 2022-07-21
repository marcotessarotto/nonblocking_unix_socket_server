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

//
///**
// * if doNotUse is false, increase uses and return true;
// * else return false
// */
//bool SocketData::incUse() {
//	std::unique_lock<std::mutex> lk(usesMutex);
//	if (doNotUse) return false;
//	uses++;
//	return true;
//}
//
///**
// * if doNotUse is false: if uses > 0, decrease uses and return uses, else return 0
// * if doNotUse is true return -1
// */
//int SocketData::decUse() {
//	std::unique_lock<std::mutex> lk(usesMutex);
//	if (doNotUse) return -1;
//	if (uses > 0) --uses;
//	if (uses == 0) {
//		usesCv.notify_one();
//	}
//	return uses;
//}
//
///**
// * if doNotUse is false, return uses
// * else return -1
// */
//int SocketData::getUses() {
//	std::unique_lock<std::mutex> lk(usesMutex);
//	if (doNotUse) return -1;
//	return uses;
//}
//
//
///**
// * set as do not use; wait until operation can be done
// */
//void SocketData::setDoNotUse() {
//	std::unique_lock<std::mutex> lk(usesMutex);
//	while (uses > 0) {
//		usesCv.wait(lk);
//	}
//	doNotUse = true;
//}
//
//bool SocketData::isDoNotUser() {
//	std::unique_lock<std::mutex> lk(usesMutex);
//	return doNotUse;
//}


} /* namespace nbuss_server */
