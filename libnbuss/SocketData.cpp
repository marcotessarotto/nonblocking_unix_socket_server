#include "SocketData.h"

namespace nbuss_server {

//SocketData::SocketData(int fd) : fd{fd} {
SocketData::SocketData()  {

}

SocketData::~SocketData() {

	writeQueue.clear();
}

} /* namespace nbuss_server */
