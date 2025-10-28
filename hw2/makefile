.PHONY: all clean

all: server client
server: server.c
	g++ -Wall -o server server.c
client: client.c
	g++ -Wall -o client client.c
clean:
	@rm -rf server client 
