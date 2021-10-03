#include <unordered_map>
#include "custom_TCP.h"

using namespace std;

// helper, compute 10 ^ power
uint32_t power_of_ten(u_int8_t power) {
	uint32_t result = 1;
	while (power > 0) {
		result *= 10;
		power--;
	}
	return result;
}

// helper, print a float number in the required format
void print_float(uint32_t initial, u_int8_t exponent, u_int8_t sign) {
	uint32_t pow = power_of_ten(exponent), before_dot, after_dot;
	char after_dot_string[10];
	
	before_dot = initial / pow;
	after_dot = initial % pow;

	if (sign)
		printf("-");
	printf("%u", before_dot);

	// don't print "." if we have an intege
	if (after_dot == 0) {
		printf("\n");
		return;
	}
	else {
		printf(".");
		sprintf(after_dot_string, "%u", after_dot);
		int i = 0;
		while (i < exponent - strlen(after_dot_string)) {
			printf("0");
			i++;
		}
		printf("%u\n", after_dot);
	}    
}

/**
	@brief Function that creates a subscribe message (used by clients).

	@param SF 1 for store-and-forward activated, 0 otherwise.
	@param id The ID of the client that generates this message. 
	@param topic Name of the subscribed topic.
	@param msg Pointer to the structure that will store this message.
**/ 
void create_subscribe_msg(u_int8_t SF, char *id, char* topic, struct TCP_msg *msg) {
	memset(msg, 0, sizeof(struct TCP_msg));
	msg->length = htonl(HEADER_SIZE + strlen(topic));
	if (SF == 0)
		msg->type = SUBSCRIBE;
	else
		msg->type = SUBSCRIBE_SF;
	memcpy(msg->id, id, strlen(id));
	memcpy(msg->payload, topic, strlen(topic));
}

/**
	@brief Function that creates a subscribe message (used by clients).

	@param id The ID of the client that generates this message. 
	@param topic Name of the subscribed topic.
	@param msg Pointer to the structure that will store this message.
**/ 
void create_unsubscribe_msg(char *id, char* topic, struct TCP_msg *msg) {
	memset(msg, 0, sizeof(struct TCP_msg));
	msg->length = htonl(HEADER_SIZE + strlen(topic));
	msg->type = UNSUBSCRIBE;
	memcpy(msg->id, id, strlen(id));
	memcpy(msg->payload, topic, strlen(topic));
}

/**
	@brief Function that creates an ID type message (used by clients
		   imediately after connecting to a server).
	
	@param id The ID of the client that generates this message. 
	@param msg Pointer to the structure that will store this message.
**/ 
void create_id_msg(char *id, struct TCP_msg *msg) {
	msg->length = htonl(18);
	msg->type = ID;
	memcpy(msg->id, id, strlen(id));
}

/**
	@brief Function for sending a message on the specified socket, using TCP.
	
	@param socket The socket used for message sending.
	@param msg Pointer to the structure containing the message to be sent.
**/
void send_msg(int socket, struct TCP_msg *msg) {
	int total_sent = 0, bytes_sent;

	// make sure we send exactly the required number of bytes
	while (total_sent < ntohl(msg->length)) {
		bytes_sent = send(socket, msg + total_sent, ntohl(msg->length) - total_sent, 0);
		total_sent += bytes_sent;
	}
}

/**
	@brief Function for printing a message. Used by client when receiving
		   PUBLISH type messages from the server,
	
	@param msg Pointer to the structure containing the message to be printed.
**/
void print_message(struct TCP_msg *msg) {
	char topic[50], data_type, content[1500];

	// Print address and port of the UDP client that generated this message.
	printf("%s:%d - ", inet_ntoa(msg->UDP_addr), ntohs(msg->UDP_port));

	// Parse and print message content in the specified format.
	strcpy(topic, msg->payload);
	memcpy(&data_type, msg->payload + 50, 1);

	switch (data_type) {
		case(0):
			uint32_t received_uint;

			memcpy(&received_uint, msg->payload + 52 , 4);
			if (*(msg->payload + 51) == 0)
				printf("%s - INT - %d\n", topic, ntohl(received_uint));
			else
				printf("%s - INT - %d\n", topic, -ntohl(received_uint));
				break;
		case(1):
			printf("%s - SHORT_REAL - ", topic);
			uint16_t received_short;
			char string_number[10];
			char before_dot[10], after_dot[10];

			memcpy(&received_short, msg->payload + 51, 2);
			received_short = ntohs(received_short);
			print_float(received_short, 2, 0);
			break;
		case(2):
			printf("%s - FLOAT - ", topic);
			uint32_t received_int;
			uint8_t power, sign;

			memcpy(&received_int, msg->payload + 52, 4);
			received_int = ntohl(received_int);
			memcpy(&power, msg->payload + 56, 1);
			sign = *(msg->payload + 51);
			print_float(received_int, power, sign);
			break;
		case(3):
			printf("%s - STRING - %s\n", topic, msg->payload + 51);
		default: break;
	}
}

/**
	@brief Function that receives a message and performs required actions,
		   according to it's type.

	@param msg The message to be interpreted.
	@param topics_table Topics hashtable. Used to register new subscriptions when needed.
	@param clients_table Subscribers hashtable.
	@param unsent_table Unsent messages hashtable (the key is the id of each subscriber).

	@return int DUPLICATE_CLIENT if a duplicate connection is identified, 0 otherwise.
**/ 
int interpret_message(struct TCP_msg *msg, unordered_map <string, list <Subscription>> &topics_table,
	unordered_map <string, Subscriber> &clients_table, unordered_map <string, list<struct TCP_msg *>> &unsent_table, int socket) {

	if (msg->type == SUBSCRIBE || msg->type == SUBSCRIBE_SF) {
		// add a new subscription to the corresponding topic in the topics table
		Subscription new_subscription = (Subscription) malloc(sizeof(struct subscription));
		if (msg->type == SUBSCRIBE) {
			new_subscription->fs = false;
		} else {
			new_subscription->fs = true;
		}
		strcpy(new_subscription->subscriber_id, msg->id);
		std:: string topic_name(msg->payload);
		topics_table[topic_name].push_front(new_subscription);
	} else if (msg->type == UNSUBSCRIBE) {
		// find and remove the corresponding topic subscription in the topics table
		std:: string topic_name(msg->payload);
		list<Subscription>:: iterator iter;
		for (iter = topics_table[topic_name].begin(); iter != topics_table[topic_name].end(); iter++) {
			if (strcmp((*iter)->subscriber_id, msg->id) == 0) {
				topics_table[topic_name].erase(iter);
				break;
			}         
		}
	} else if (msg->type == ID){ // ID message received by server immediately after client connection
		string s(msg->id);
		if (clients_table.find(s) == clients_table.end()) {
			/*
			 * if the subscriber with this id wasn't connected before, we need
			 * to add it to the subscribers table
			 */
			Subscriber new_subscriber = (Subscriber) calloc(1, sizeof(struct subscriber));
			new_subscriber->connected = true;
			new_subscriber->socket = socket;
			clients_table[s] = new_subscriber;
		} else if (clients_table[s]->connected == 0) {
			// modify a subscriber that was connected before
			clients_table[s]->socket = socket;
			clients_table[s]->connected = 1;

			// send sf subscribed topics lost while this subscriber was disconnected
			while (!unsent_table[s].empty()) {
				send_msg(socket, unsent_table[s].front());
				free(unsent_table[s].front()); // free message memory
				unsent_table[s].pop_front();
			}
		} else {
			// duplicate connection found
			printf("Client %s already connected.\n", msg->id);
			close(socket);
			return DUPLICATE_CLIENT;
		}
	}
	else if (msg->type == PUBLISH) {
		print_message(msg);
	}
	return 0;
}

/**
	@brief Function that reads a message over the TCP protocol,
		   making sure that the message is complete and correct.

	@param socket The socket where this message is received.
	@param msg Pointer to the structure that will store the new message
	@param topics_table Topics hashtable.
	@param clients_table Subscribers hashtable.
	@param unsent_table Unsent messages hashtable (the key is the id of each subscriber).

	@return int SOCKET if a closed connection is detected, 0 otherwise.
**/ 
int receive_msg(int socket, struct TCP_msg *msg, unordered_map <string, list <Subscription>> &topics_table,
			unordered_map <string, Subscriber> &clients_table, unordered_map <string, list<struct TCP_msg *>> &unsent_table) {
	
	char buffer[BUFLEN];
	int bytes_received, iter = 0, length, total_bytes = 0;
	int ret = 0;

	// first, receive the length of this message
	memset(buffer, 0, BUFLEN);
	bytes_received = recv(socket, buffer, 4, 0);
	ABORT(bytes_received == -1, "RECEIVE error\n");

	// check if the client has been disconnected
	if (bytes_received == 0) {
		/*
		 * search the closed client by socket in the clients_table (so that we
		 * can print it's ID)
		 */
		unordered_map <string, Subscriber>::iterator iter;
		for (iter = clients_table.begin(); iter != clients_table.end(); iter++) {
			if ((*iter).second->socket == socket) {
				close(socket);
				printf("Client %s disconnected.\n", (*iter).first.c_str());
				(*iter).second->connected = 0;
			}
		}
		return SOCKET_CLOSED;
	}


	memset(msg, 0, sizeof(struct TCP_msg));
	memcpy(msg, buffer, 4);
	total_bytes = 4;
	/*
	 * Read the whole message (with exact number of bytes). We will stay in this
	 * loop until we have received the whole message, being sure this will happen
	 * soon because we've already received it's first part (length)
	 */
	while (total_bytes < ntohl(msg->length)) {
		memset(buffer, 0, BUFLEN);
		bytes_received = recv(socket, buffer, ntohl(msg->length) - total_bytes, 0);
		memcpy(&(msg->type) + total_bytes - 4, buffer, bytes_received);
		total_bytes += bytes_received;
	}

	// perform specific actions acoording to the message type and content
	ret = interpret_message(msg, topics_table, clients_table, unsent_table, socket);

	return ret;
}

/**
	@brief Function that takes the content of an UDP message, creates a message
		   corresponding to our protocol and sends it to all of the interested
		   subscribers.

	@param content The content received on the UDP connection.
	@param content_size Number of conent bytes.
	@param clients_table Subscribers hashtable.
	@param topics_table Topics hashtable.
	@param unsent_table Unsent messages hashtable (the key is the id of each subscriber).
	@param UDP_cli_addr Address and port of the UDP client that generated this message.
**/ 
void publish_message(char *content, int content_size, unordered_map <string, Subscriber> &clients_table,
					unordered_map <string, list <Subscription>> &topics_table, unordered_map <string, list<struct TCP_msg *>> &unsent_table,
					struct sockaddr_in UDP_cli_addr) {
	char topic[51];
	TCP_msg msg;

	// create a PUBLISH message according to our TCP-resistent protocol
	memset(&msg, 0, sizeof(msg));
	msg.length = htonl(HEADER_SIZE + content_size);
	msg.type = PUBLISH;
	memcpy(&(msg.UDP_addr), &UDP_cli_addr.sin_addr, sizeof(UDP_cli_addr.sin_addr));
	msg.UDP_port = UDP_cli_addr.sin_port;
	memcpy(msg.payload, content, content_size);

	// iterate trough all subscribers that subscribed to the topic of this message
	sscanf(content, "%s", topic);
	std::string s(topic);
	list<Subscription>::iterator iter;
	for (iter = topics_table[s].begin(); iter != topics_table[s].end(); iter++) {
		if (clients_table[(*iter)->subscriber_id]->connected) {
			// send the message, if the current subscriber is connected
			send_msg(clients_table[(*iter)->subscriber_id]->socket, &msg);
		} else if ((*iter)->fs) {
			/*
			 * if the current subscriber is not connected, but has subscribed
			 * to this topic with sf, buffer this message in it's entry from
			 * the unsent_messages table
			 */
			string s1((*iter)->subscriber_id);
			TCP_msg *new_msg =  (TCP_msg*) malloc(sizeof (struct TCP_msg));
			memcpy(new_msg, &msg, ntohl(msg.length));
			unsent_table[s1].push_front(new_msg);
		}
	}
}

