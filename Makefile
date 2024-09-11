CC=g++ -g

all: build

build: server subscriber

server:	server.o common.o
	$(CC) $^ -o $@

server.o: server.cpp common.cpp
	$(CC) $^ -c

subscriber: subscriber.o common.o
	$(CC) $^ -o $@

subscriber.o: subscriber.cpp common.cpp
	$(CC) $^ -c

clean:
	rm -rf *.o server subscriber
