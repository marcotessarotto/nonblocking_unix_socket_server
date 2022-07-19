#ifndef LIBNBUSS_FILEDESCRIPTOR_H_
#define LIBNBUSS_FILEDESCRIPTOR_H_

#include <iostream>
#include <unistd.h>

#include "Logger.h"

// class for making file descriptors auto-closeable; use FileDescriptor as type of local variable
class FileDescriptor {
public:
	volatile int fd;

	FileDescriptor() {
		fd = -1;
		// LIB_LOG(info) << "FileDescriptor::FileDescriptor()";
	}

	void close() {
		if (fd >= 0) {
			LIB_LOG(info) << "FileDescriptor::close fd=" << fd;
			::close(fd);
			fd = -1;
		}
	}

	~FileDescriptor() {
		//LIB_LOG(info) << "~FileDescriptor " << fd;
		close();
	}
};

#endif /* LIBNBUSS_FILEDESCRIPTOR_H_ */
