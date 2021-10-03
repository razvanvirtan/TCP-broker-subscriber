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
#include "custom_TCP.h"


int main(int argc, char **argv) {
	int server_socket, errors, enable, message_result;
	char id[13];
	char buffer[BUFLEN], command[15], *token, topic[52];
	u_int8_t SF;
	struct sockaddr_in serv_addr;
	struct TCP_msg msg, received_msg;

	/*
	 * These tables will never be used. Their only purpose is having a valid
	 * receive_msg() call.
	 */
	unordered_map <string, Subscriber> clients_table;
	unordered_map <string, list <Subscription>> topics_table;
	unordered_map <string, list<TCP_msg *>> unsent_table;

	ABORT(argc != 4, "Invalid number of arguments.\n \
					./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n");

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// create socket for server communication
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	ABORT(server_socket < 0, "Server socket: Socket CREATE error");
	enable = 1;

	// disable Neagle algorithm
	errors = setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	ABORT(errors == -1, "Server socket: SET SOCKET OPTIONS error");

	// parse command line arguments and create connection configuration
	ABORT(strlen(argv[1]) > 10, "Invalid ID\n");
	strcpy(id, argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = atoi(argv[3]);
	ABORT((atoi(argv[3]) > 65535) || (atoi(argv[3]) < 0), "Invalid port number\n");
	serv_addr.sin_port = htons(serv_addr.sin_port);
	errors = inet_aton(argv[2], &serv_addr.sin_addr);
	ABORT(errors < 0, "Invalid IP address\n");

	// create connection and send subscriber ID to server
	errors = connect(server_socket, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	ABORT(errors < 0, "Connection to server failed.\n");
	create_id_msg(id, &msg);
	send_msg(server_socket, &msg);
	ABORT(errors < 0, "Server HANDSHAKE error.\n");

	struct pollfd descriptors[2];
	descriptors[0].fd = STDIN_FILENO; descriptors[0].events = POLLIN;
	descriptors[1].fd = server_socket; descriptors[1].events = POLLIN;

	// start listening for incoming data from stdin or server
	memset(&received_msg, 0, sizeof(received_msg));
	while (1) {
		errors = poll(descriptors, 2, -1);
		ABORT(errors == -1, "Polling errors: failed stdin or server communication.\n");

		if (descriptors[0].revents & POLLIN) { // stdin commands case
			descriptors[0].revents = 0;
			fgets(buffer, 70, stdin);
			/*
			 * Parse the command and perform subscriber actions as required.
			 * Print problem message on invalid command.
			 */
			buffer[strlen(buffer) - 1] = '\0';
			token = strtok(buffer, " ");
			strcpy(command, token);
			if (!strcmp(command, "exit")) { // disconnect and stop subscriber
				close(server_socket);
				exit(0);
			} else {
				token = strtok(NULL, " ");
				WARNING(token == NULL, "Invalid command. Try 'subscribe <TOPIC> <SF>' or 'unsubscribe <TOPIC>'.\n");
				strcpy(topic, token);
				if (!strcmp(command, "subscribe")) { // subscribe to topic
					token = strtok(NULL, " ");
					SF = atoi(token);
					ABORT(SF != 0 && SF != 1, "Invalid SF option\n");
					create_subscribe_msg(SF, id, topic, &msg);
					send_msg(server_socket, &msg);
					printf("Subscribed to topic.\n");
				} else if (!strcmp(command, "unsubscribe")) { // unsubscribe from topic
					create_unsubscribe_msg(id, topic, &msg);
					send_msg(server_socket, &msg);      
					printf("Unsubscribed from topic.\n");              
				} else {
					WARNING(1, "Invalid command. Try exit, subscribe or unsubscribe.\n")
				}
			}
		} else { // receive message from server case
			message_result = receive_msg(server_socket, &received_msg, topics_table, clients_table, unsent_table);
			// If the server is disconnected, close the subscriber.
			if (message_result == SOCKET_CLOSED) {
				close(server_socket);
				exit(0);
			}
		}

	}

	return 0;
}

