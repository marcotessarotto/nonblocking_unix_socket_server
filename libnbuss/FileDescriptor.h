#ifndef LIBNBUSS_FILEDESCRIPTOR_H_
#define LIBNBUSS_FILEDESCRIPTOR_H_

#include <iostream>
#include <unistd.h>
#include <errno.h>

#include "Logger.h"

// class for making file descriptors auto-closeable; use FileDescriptor as type of local variable
class FileDescriptor {

	std::string fd_name;
public:

	//TODO: consider to replace volatile with mutex and getter/setter methods
	volatile int fd;

	FileDescriptor(std::string fd_name = "") : fd_name{fd_name} {
		fd = -1;
		// LIB_LOG(info) << "FileDescriptor::FileDescriptor()";
	}

	int close() {
		if (fd >= 0) {
			LIB_LOG(info) << "FileDescriptor::close fd=" << fd << " " << fd_name;
			int res = ::close(fd);

			if (res == -1) {
				LIB_LOG(error) << "FileDescriptor::close error fd=" << fd  << " " << fd_name << " " << strerror(errno);
			} else {
				fd = -1;
			}

			return res;
		} else {
			LIB_LOG(warning) << "FileDescriptor::close already closed fd=" << fd << " " << fd_name;

			return -1;
		}
	}

	~FileDescriptor() {
		//LIB_LOG(info) << "~FileDescriptor " << fd;
		close();
	}
};

#endif /* LIBNBUSS_FILEDESCRIPTOR_H_ */
