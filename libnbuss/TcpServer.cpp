//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <netdb.h>

#include <sys/sendfile.h>

#include <sys/epoll.h>

#include <pthread.h>
#include <errno.h>

#include "TcpServer.h"

namespace nbuss_server {

TcpServer::TcpServer(unsigned int port, const std::string &address, unsigned int backlog) :
		address{address}, port{port}, IGenericServer(backlog)  {
}


TcpServer::~TcpServer() {

}

void TcpServer::setup() {

	// listen_sock will be auto closed when returning from listen
	listen_sock.fd = open_listening_socket();

	if (listen_sock.fd == -1) {
		throw std::runtime_error("cannot open server socket");
	}

}

int tcp_proto;


void set_tcp_socket_option(int fd, int opt, int optval) {
	socklen_t sl_len = sizeof(optval);

	if (setsockopt(fd, tcp_proto, opt, &optval, sl_len) == -1) {
		perror("setsockopt");
	}
}

void set_tcp_cork(int fd, int optval) {

	socklen_t sl_len = sizeof(optval);

	if (setsockopt(fd, tcp_proto, TCP_CORK, &optval, sl_len) == -1) {
		perror("setsockopt");
	}
}


int is_tcp_cork_enabled(int fd) {

	int optval;
	socklen_t sl_len = sizeof(optval);
	if (getsockopt(fd, tcp_proto, TCP_CORK, &optval, &sl_len) == -1) {
		perror("getsockopt");
	}

	return optval;
}


void init_socket_env() {
	struct protoent * pe;

	pe = getprotobyname("TCP");
	tcp_proto = pe->p_proto;
}


void check_nonblock_fd(int fd) {
	int res;

	res = fcntl(fd, F_GETFL, 0);

	if (res == -1) {
		perror("fcntl");
	} else {
		if (res & O_NONBLOCK)  {
			printf("fd %d has O_NONBLOCK attr\n", fd);
		} else {
			printf("fd %d does not have O_NONBLOCK attr! trying to enable flag... ", fd);

			if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
				perror("fcntl - setting O_NONBLOCK");
			} else {
				printf("OK\n");
			}
		}
	}
}



#define BUF_SIZE 500

/**
 * find the network interface, open socket, bind it to the address
 *
 * return socket or -1 on error
 */
int TcpServer::open_listening_socket() {
   struct addrinfo hints;
   struct addrinfo *result, *rp;
   int sfd, s;
   struct sockaddr_storage peer_addr;
   socklen_t peer_addr_len;
   ssize_t nread;
   char buf[BUF_SIZE];

   char port_str[6];

   if (port > 0xFFFF) {
	   printf("port error: %d\n", port);
	   //exit(EXIT_FAILURE);
	   return -1;
   }

   sprintf(port_str, "%u", port);

   printf("open_listening_socket port=%s\n", port_str);

   // PF_INET 2
   // SOCK_STREAM 1
   // TCP 6

   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_UNSPEC; //AF_UNSPEC; // AF_UNSPEC; AF_INET   /* Allow IPv4 or IPv6 */
   hints.ai_socktype = SOCK_STREAM; // SOCK_DGRAM; /* Datagram socket */
   hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;    /* For wildcard IP address */
   // AI_NUMERICSERV : getaddrinfo service parameter contains a port number
   hints.ai_protocol = 0;          /* Any protocol */
   hints.ai_canonname = NULL;
   hints.ai_addr = NULL;
   hints.ai_next = NULL;

   s = getaddrinfo(/*"0.0.0.0"*/ address.c_str(), port_str, &hints, &result); // does not support SOCK_NONBLOCK in ai_socktype
   if (s != 0) {
	   fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
	   //exit(EXIT_FAILURE);
	   return -1;
   }

   /* getaddrinfo() returns a list of address structures.
	  Try each address until we successfully bind(2).
	  If socket(2) (or bind(2)) fails, we (close the socket
	  and) try the next address. */

   for (rp = result; rp != NULL; rp = rp->ai_next) {

	   sfd = socket(rp->ai_family, rp->ai_socktype | SOCK_NONBLOCK, // SOCK_NONBLOCK must be specified here
			   rp->ai_protocol);

	   if (sfd == -1)
		   continue;

//	   printf("rp->ai_family=%d rp->ai_socktype=%d rp->ai_protocol = %d\n", rp->ai_family, rp->ai_socktype, rp->ai_protocol);

	   if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
		   break;                  /* Success */

	   close(sfd);
   }

   freeaddrinfo(result);           /* No longer needed */

//   printf("out of for loop\n");

   if (rp == NULL) {               /* No address succeeded */
	   fprintf(stderr, "Could not bind\n");
	   //exit(EXIT_FAILURE);
	   return -1;
   }

   //if (CHECK_NONBLOCK_SOCKET) check_nonblock_fd(sfd);

   if (::listen(sfd, backlog) == -1) {
	   perror("listen");
	   exit(EXIT_FAILURE);
   }

//   char buffer[INET_ADDRSTRLEN] = { 0 };
//
//   //char * str =  inet_ntoa(rp->ai_addr);
//
//   inet_ntop(AF_INET, rp->ai_addr, buffer, INET_ADDRSTRLEN);
//
//   printf("%s\n",buffer);

   return sfd;

}


} /* namespace nbuss_server */
