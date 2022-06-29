#include "IGenericServer.h"
#include <iostream>

namespace nbuss_server {

IGenericServer::IGenericServer() {

	std::cout << "IGenericServer::IGenericServer" << std::endl;

}

IGenericServer::~IGenericServer() {

	std::cout << "IGenericServer::~IGenericServer()" << std::endl;

}

}
