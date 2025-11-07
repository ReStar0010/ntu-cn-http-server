#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

#include <string>
#include <map>

enum class ClientReqState{
    READING_HEADER,
    READING_BODY,
    READING_COMPLETE
};

struct ClientState{
    int fd;
    std::string read_buffer;
    std::string write_buffer;
    size_t bytes_sent;
    bool keep_alive;
    ClientReqState req_state;
    size_t content_length; // which is body size
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
};

void initial_client(ClientState &client);
void clear_client(ClientState &client);

#endif // CLIENT_STATE_H

