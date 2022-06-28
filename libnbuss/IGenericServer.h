#ifndef IGENERICSERVER_H_
#define IGENERICSERVER_H_


#include <functional>

namespace nbuss_server {

enum job_type_t {
	DATA_REQUEST, // incoming data is present
	CLOSE_SOCKET // socket must be closed
};

class IGenericServer {
public:
	IGenericServer();
	virtual ~IGenericServer();
	
	// pure virtual methods

    /**
     * setup server and bind socket to listening address
     *
     * @throws std::runtime_error
     */
	virtual void setup() = 0;
	
    /**
     * starts a new thread which will listen for incoming connections and process them
     *
     * callback function is called when incoming data is ready.
     * callback function parameters are: socket file descriptor and job type
     *
     * @throws std::runtime_error
     */
	virtual void listen(std::function<void(int, enum job_type_t )> callback_function) = 0;


    /**
     * terminate server
     *
     * @throws std::runtime_error
     */
	virtual void terminate() = 0;

};

}

#endif /* IGENERICSERVER_H_ */
