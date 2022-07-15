#ifndef LIBNBUSS_ITHREADABLE_H_
#define LIBNBUSS_ITHREADABLE_H_

#include "IGenericServer.h"

namespace nbuss_server {


/**
 * an interface which adds start and stop methods to IGenericServer
 */
class IThreadable: public virtual nbuss_server::IGenericServer {
public:
	IThreadable();
	virtual ~IThreadable();

	virtual void start(std::function<void(IGenericServer *, int, enum job_type_t )> callback_function) = 0;

	virtual void stop() = 0;

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_ITHREADABLE_H_ */
