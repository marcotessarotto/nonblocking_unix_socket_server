#ifndef LIBNBUSS_LOGGER_H_
#define LIBNBUSS_LOGGER_H_


#include <iostream>

#include "boost/log/trivial.hpp"



void init_log();

#define LIB_LOG(lvl) BOOST_LOG_TRIVIAL(lvl)



#endif /* LIBNBUSS_LOGGER_H_ */
