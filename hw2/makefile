.PHONY: all clean

all: server client
setup:
	@mkdir -p web/tmp
	@mkdir -p web/videos
	@mkdir -p web/tmp
server: server.cpp utils/base64.cpp utils/base64.h
	g++ -Wall -std=c++11 -o server server.cpp utils/base64.cpp
client: client.cpp
	g++ -Wall -o client client.cpp
clean:
	@rm -rf server client
