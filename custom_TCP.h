#ifndef _CUSTOM_TCP_H
#define _CUSTOM_TCP_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iterator>
#include <list>
#include <unordered_map>

using namespace std;

#define HEADER_SIZE 24
#define BUFLEN 1580
#define SUBSCRIBE_SF 0
#define SUBSCRIBE 1
#define UNSUBSCRIBE 2
#define PUBLISH 3
#define ID 4

#define SOCKET_CLOSED 0x10
#define DUPLICATE_CLIENT 0x11


#define ABORT(condition, message) \
	if (condition) { \
		fprintf(stderr, "%s", message); \
		exit(EXIT_FAILURE); \
	}

#define WARNING(condition, message) \
	if (condition) { \
		fprintf(stderr, "%s", message); \
	}

struct TCP_msg {
	u_int32_t length;
	u_int8_t type;
	char id[13];
	u_int16_t UDP_port;
	struct in_addr UDP_addr;
	char payload[1556];
};

typedef struct subscriber {
	int socket;
	bool connected;
} *Subscriber;

typedef struct subscription {
	char subscriber_id[13];
	bool fs;
} *Subscription;


void create_unsubscribe_msg(char *id, char* topic, struct TCP_msg *msg);
void create_subscribe_msg(u_int8_t SF, char* id, char* topic, struct TCP_msg *msg);
void create_id_msg(char *id, struct TCP_msg *msg);

void send_msg(int socket, struct TCP_msg *msg);
int receive_msg(int socket, struct TCP_msg *msg, unordered_map <string, list <Subscription>> &topics_table,
			unordered_map <string, Subscriber> &clients_table, unordered_map <string, list<struct TCP_msg *>> &unsent_table);
void publish_message(char *content, int content_size, unordered_map <string, Subscriber> &clients_table,
		 unordered_map <string, list <Subscription>> &topics_table, unordered_map <string, list<struct TCP_msg *>> &unsent_table,
		 struct sockaddr_in UDP_cli_addr);

#endif

