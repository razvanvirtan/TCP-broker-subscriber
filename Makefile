server:
	g++ -o server server.cpp custom_TCP.cpp

subscriber:
	g++ -o subscriber subscriber.cpp custom_TCP.cpp

clean:
	rm server subscriber
