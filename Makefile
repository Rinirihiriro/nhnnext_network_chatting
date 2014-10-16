ODIR = bin

all: server client

server : server.cpp
	g++ -o $(ODIR)/$@ server.cpp

client : client.cpp
	g++ -o $(ODIR)/$@ client.cpp

clean: 
	rm -f bin/*
