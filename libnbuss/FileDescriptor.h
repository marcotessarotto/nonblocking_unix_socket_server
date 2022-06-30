#ifndef LIBNBUSS_FILEDESCRIPTOR_H_
#define LIBNBUSS_FILEDESCRIPTOR_H_

#include <iostream>
#include <unistd.h>

// class for making file descriptors auto-closeable; use FileDescriptor as type of local variable
class FileDescriptor {
public:
	volatile int fd;

	FileDescriptor() {
		fd = -1;
	}

	void close() {
		if (fd >= 0) {
			std::cout << "FileDescriptor::close fd=" << fd << std::endl;
			::close(fd);
			fd = -1;
		}
	}

	~FileDescriptor() {
		//std::cout << "~FileDescriptor " << fd << std::endl;
		close();
	}
};

#endif /* LIBNBUSS_FILEDESCRIPTOR_H_ */
