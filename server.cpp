#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unordered_map>
#include <iterator>
#include "custom_TCP.h"

using namespace std;

#define MAX_QUEUE_CLIENTS 10
#define TCP_CONNECT_IDX 0
#define UDP_IDX 1
#define STDIN_IDX 2

/**
	@brief Function that creates a new socket for UDP communication.

	@param port Port for UDP communication.
	@return int The socket created.
**/ 
int create_UDP_socket(char *port) {
	int socket_UDP, errors;
	struct sockaddr_in UDP_addr;
	int enable = 1;

	socket_UDP = socket(PF_INET, SOCK_DGRAM, 0);
	ABORT(socket_UDP == -1, "UDP socket: CREATE error\n");
	errors = setsockopt(socket_UDP, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &enable, sizeof(int));
	ABORT(errors == -1, "UDP socket: SET SOCKET OPTIONS error\n");
	memset((char *) &UDP_addr, 0, sizeof(UDP_addr));
	UDP_addr.sin_family = AF_INET;
	UDP_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	UDP_addr.sin_port = htons(atoi(port));
	errors = bind(socket_UDP, (struct sockaddr *) &UDP_addr, sizeof(UDP_addr));
	ABORT(errors == -1, "UDP socket: BIND error\n");

	return socket_UDP;
}


/**
	@brief Function that creates a new socket that will listen for new TCP connections.

	@param port Port for TCP listening.
	@return int The socket created.
**/ 
int create_TCP_passive_socket(char *port) {
	int socket_TCP, errors;
	struct sockaddr_in TCP_addr;
	int enable = 1;

	socket_TCP = socket(PF_INET, SOCK_STREAM, 0);
	ABORT(socket_TCP == -1, "TCP socket: CREATE error\n");
	errors = setsockopt(socket_TCP, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &enable, sizeof(int));
	ABORT(errors == -1, "TCP socket: SET SOCKET OPTIONS error\n");
	memset((char *) &TCP_addr, 0, sizeof(TCP_addr));
	TCP_addr.sin_family = AF_INET;
	TCP_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	TCP_addr.sin_port = htons(atoi(port));
	errors = bind(socket_TCP, (struct sockaddr *) &TCP_addr, sizeof(TCP_addr));
	ABORT(errors == -1, "TCP socket: BIND error\n");
	errors = listen(socket_TCP, MAX_QUEUE_CLIENTS);
	ABORT(errors == -1, "TCP socket: LISTEN error\n");

	return socket_TCP;
}


/**
	@brief Function that creates a new socket for communication with a TCP client.

	@param socket_TCP The TCP passive socket that detected the connection request.
	@param clients_table Subscribers hashtable. Used to register the new client.
	@param unsent_table Unsent messages table. If the an old client connects again,
						we will use this table to send the messages he lost while
						being disconnected.
	@return int The socket created.
**/ 
int create_TCP_client_socket(int socket_TCP, unordered_map <string, Subscriber> &clients_table,
							unordered_map <string, list<struct TCP_msg *>> unsent_table) {
	int TCP_cli_len, enable, errors, new_socket, check_duplicate;
	struct sockaddr_in TCP_cli_addr;
	struct TCP_msg *msg = (struct TCP_msg *) calloc(1, sizeof(struct TCP_msg));
	Subscriber new_subscriber = (Subscriber) calloc(1, sizeof(struct subscriber));

	// accept connection and disable Neagle algorithm
	TCP_cli_len = sizeof(TCP_cli_addr);
	new_socket = accept(socket_TCP, (struct sockaddr*) &TCP_cli_addr, (socklen_t *) &TCP_cli_len);
	WARNING(new_socket == -1, "TCP client: ACCEPT error\n");
	enable = 1;
	errors = setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	ABORT(errors == -1, "TCP client: SET SOCKET OPTIONS error\n");

	/*
	 * Receive the ID of the new subscriber. If we have a duplicate (we already
	 * have a connection with a same-id client), set the new socket to -1
	 * and print corresponding message.
	 */
	unordered_map <string, list <Subscription>> null_table;  // empty table, used for valid receive_msg call
	
	check_duplicate = receive_msg(new_socket, msg, null_table, clients_table, unsent_table);
	if (check_duplicate != DUPLICATE_CLIENT) {
		printf("New client %s connected from %s:%d\n", msg->id, inet_ntoa(TCP_cli_addr.sin_addr), TCP_cli_addr.sin_port);
	} else {
		new_socket = -1;
	}

	return new_socket;
}


int main(int argc, char **argv) {
	int errors, bytes_read, enable = 1, i;
	unsigned int UDP_cli_len;
	int socket_UDP, socket_TCP, new_socket;
	int nr_descriptors = 3, descriptors_size = 3000;
	struct pollfd* descriptors = (struct pollfd*) malloc(descriptors_size * sizeof(struct pollfd));
	struct sockaddr_in UDP_cli_addr;
	char buffer[BUFLEN];
	struct TCP_msg msg, received_msg;
	unordered_map <string, Subscriber> clients_table;
	unordered_map <string, list <Subscription>> topics_table;
	unordered_map <string, list<struct TCP_msg *>> unsent_table;

	ABORT(argc != 2, "Invalid number of arguments.\n \
					./server <PORT>\n");

	ABORT((atoi(argv[1]) > 65535) || (atoi(argv[1]) < 0), "Invalid port number.\n");


	// disable stdout buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	socket_UDP = create_UDP_socket(argv[1]);
	socket_TCP = create_TCP_passive_socket(argv[1]); // TCP socket for connections
	
	// add stdin, UDP socket and TCP connection socket to the used descriptors set
	descriptors[0].fd = socket_TCP; descriptors[0].events = POLLIN;
	descriptors[1].fd = socket_UDP; descriptors[1].events = POLLIN;
	descriptors[2].fd = STDIN_FILENO; descriptors[2].events = POLLIN;

	char final = 0;
	memset(&received_msg, 0, sizeof(received_msg));
	while(!final) {
		poll(descriptors, nr_descriptors, -1);

		for(i = 0; i < nr_descriptors; i++) {
			if (descriptors[i].revents & POLLIN) {
				descriptors[i].revents = 0;
				if (i == TCP_CONNECT_IDX) {
					/*
					 * create a new TCP connection to a subscriber and add the
					 * new socket to the descriptors set
					 */
					new_socket = create_TCP_client_socket(socket_TCP, clients_table, unsent_table);
					if (nr_descriptors + 1 > descriptors_size) {
						descriptors = (struct pollfd*) realloc(descriptors, 2 * descriptors_size * sizeof(struct pollfd));
						descriptors_size = 2 * descriptors_size;
					}
					if (new_socket != -1) {
						descriptors[nr_descriptors].fd = new_socket;
						descriptors[nr_descriptors].events = POLLIN;
						nr_descriptors++;
					}
				} else if (i == UDP_IDX) {
					// receive and publish a message from an UDP client
					memset(buffer, 0, BUFLEN);
					UDP_cli_len = sizeof(UDP_cli_addr);
					bytes_read = recvfrom(descriptors[i].fd, buffer, BUFLEN, 0, (struct sockaddr*) &UDP_cli_addr, (socklen_t *) &UDP_cli_len);
					publish_message(buffer, bytes_read, clients_table, topics_table, unsent_table, UDP_cli_addr);
					ABORT(bytes_read == -1, "UDP socket: RECEIVE error\n");
				} else if (i == STDIN_IDX) {
					// receive exit command from stdin
					scanf("%s", buffer);
					if (!strcmp(buffer, "exit")) {
						final = 1;
					} else {
						WARNING(1, "Invalid command. Only exit command allowed on server.\n");
					}
				} else {
					// receive message from a TCP client (subscribe / unsubscribe)
					receive_msg(descriptors[i].fd, &received_msg, topics_table, clients_table, unsent_table);
				}
				break;
			}
		}
		if (final)
			break;
	}

	// close all sockets before exit  
	for (i = 0; i < nr_descriptors; i++) {
		close(descriptors[i].fd);
	}

	return 0;
}

