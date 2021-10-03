# TCP broker-subscriber app #

## Description ##
This application represents a basic simulation for a publisher -> broker -> subscriber system.
It works on debian distributions (tested on ubuntu 18.04)

The architecture of the application contains a server and 2 kinds of clients:
- **publishers**: UDP clients that can publish messages on different kind of topics
- **subscriber**: TCP clients that can suscribe to topics and receive related messages
The *server* acts like a broker, forwarding the messages from publishers to subscribers.

The messages that are published can fit into the following data types:
INT, SHORT_REAL, FLOAT, STRING.

The TCP communication between the server and the subscribers is made trough a
custom application level protocol, described in the **Behind the scenes** section
below.

## Usage ##
First of all, you have to compile the app components:
```
make server && make subscriber
```

For starting the server, use:
```
./server <PORT>
```

For starting the subscriber, use:
```
./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>
```
The client id will be used by te server to identify the same client in two different
sessions.

The only command accepted by the server is `exit`, which will close the program.
This is also accepted by the subscriber,

On subscriber, you can subscribe to a topic by using:
```
subscribe <TOPIC_NAME> <SF>
```
where SF can be 0 or 1. If SF is set to one, the server will store messages related
to this topic even if the subscriber is offline at the publish moment and will
forward the messages after the subscriber is started again.
To unsubscribe from a topic, use:
```
unsubscribe <TOPIC>
```

After starting a server and some subscribers, you can use `pcom_hw2_udp_client/udp_client.py` to see
how things work (this script doesn't represent my contribution to the project):
```
python3 udp_client.py 127.0.0.1 8080
```
Some of the topics you can subscribe to for seeing the results when using this script are:
`a_non_negative_int, a_non_negative_int, that_is_big_short_real, a_strange_float, huge_string`.


## Behind the scenes: implementation details ##
The application level protocol used for the communication between the TCP server and
the TCP clients has 2 main goals:
- providing a communication that is both reliable over TCP (so that we don't 
    have message concatenation / truncation issues) and efficient (not sending more
    bytes than needed)
- making data sending as easy as possible.

This being said, the structure of a message (the `TCP_msg` structure from "custom_TCP.h") looks like this:
```
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+------------
   |                           Length                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Type      |                    Id                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              Id                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   HEADER
   |                              Id                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           Id                  |         UDP_port              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                            UDP_addr                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+------------
   |                             Payload                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **Length**: 32 bits
Specifies the total size of the message (nr. of bytes). It is used for any kind of
message to provide a reliable transmission over TCP.

- **Type**: 8 bits
The message can have one of the following types: 
    - SUBSCRIBE_SF: sent from the client to the server, it represents a subscribe
                request to a specific topic with SF option enabled (the name
                of the topic is stored in the payload)
    - SUBSCRIBE: same as SUBSCRIBE_SF, but with SF disabled
    - UNSUBSCRIBE:  sent from the client to the server, it represents an unsubscribe
                request for a certain topic (the name of the topis is stored in the payload)
    - PUBLISH:  sent from the server to clients, it represents a published message
            received from an UDP client. The content of the message is stored in
            the payload
    - ID: sent from client to server, immediately after the TCP connection was realized;
        it is used to send to the server the ID of the newly connected client

- **Id**: 13 bytes => 104 bits (in order to keep the header 32 bits aligned)
Used in messages sent from the clients to the server, it contains the ID of the client
that sends the message. Null content in messages sent by the server.

- **UDP_port**: 16 bits
Used in PUBLISH messages sent by the server. It contains the port from the UDP client
that published the current message. Used by the subscriber in the text that is
finally displayed.

- **UDP_addr**: 32 bits
Used in PUBLISH messages sent by the server. It contains the IP of the UDP client
that published the current message. Used by the subscriber in the text that is
finally displayed.

- **Payload**: variable size, max. 1556 bytes
    - In PUBLISH messages, it stores the actual message received by the server from
an UDP client.
    - In SUBSCRIBE / UNSUBSCRIBE messages, it stores the name of the topic.
    - In ID messages, it has null content.
    
`custom_TCP.cpp` contains the implementation of the functions used for creating
every type of message.
