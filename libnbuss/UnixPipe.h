#ifndef LIBNBUSS_UNIXPIPE_H_
#define LIBNBUSS_UNIXPIPE_H_

#include <unistd.h>
#include <syslog.h>
#include <errno.h>

#include "Logger.h"

namespace nbuss_server {

class UnixPipe {

	int fd[2];

public:
	UnixPipe() : fd{-1, -1} {
		int c;
		c = pipe(fd);

		if (c == -1) {
			//syslog(LOG_ERR, "error on pipe syscall");
			LIB_LOG(error) << "error on pipe syscall " << strerror(errno);
		}
	}

	virtual ~UnixPipe() {
		close(fd[0]);
		close(fd[1]);
	}

	int getReadFd() {
		return fd[0];
	}

	int getWriteFd() {
		return fd[1];
	}

	int write(char ch) {
		int c;

		c = ::write(fd[1], &ch, sizeof(ch));

		if (c == -1) {
			//syslog(LOG_ERR, "error on write syscall");
			LIB_LOG(error) << "error on write syscall " << strerror(errno);
		}

		return c;
	}

	int read() {
		char ch;
		int c;

		c = ::read(fd[0], &ch, sizeof(ch));

		if (c == -1) {
			//syslog(LOG_ERR, "error on read syscall");
			LIB_LOG(error) << "error on read syscall " << strerror(errno);

			return c;
		} else {
			return ch;
		}
	}
};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_UNIXPIPE_H_ */
