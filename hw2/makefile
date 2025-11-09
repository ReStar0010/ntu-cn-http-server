.PHONY: all clean

all: setup server client
setup:
	@mkdir -p web/tmp
	@mkdir -p web/videos
	@mkdir -p web/files
server: server.cpp utils/base64.cpp utils/base64.h utils/responses.cpp utils/responses.h utils/client_state.cpp utils/client_state.h utils/auth.cpp utils/auth.h utils/http_utils.cpp utils/http_utils.h utils/video.cpp utils/video.h utils/request_handler.cpp utils/request_handler.h
	g++ -Wall -std=c++11 -o server server.cpp utils/base64.cpp utils/responses.cpp utils/client_state.cpp utils/auth.cpp utils/http_utils.cpp utils/video.cpp utils/request_handler.cpp
client: client.cpp utils/base64.cpp utils/base64.h utils/http_utils.cpp utils/http_utils.h
	g++ -Wall -std=c++11 -o client client.cpp utils/base64.cpp utils/http_utils.cpp
clean:
	@rm -rf server client
	@rm -rf web/tmp
	@rm -rf web/videos
	@rm -rf web/files