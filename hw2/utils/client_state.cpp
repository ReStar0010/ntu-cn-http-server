#include "client_state.h"

using namespace std;

void initial_client(ClientState &client){
    client.fd = -1;
    client.read_buffer.clear();
    client.write_buffer.clear();
    client.bytes_sent = 0;
    client.keep_alive = true;
    client.req_state = ClientReqState::READING_HEADER;
    client.content_length = 0;
    client.method.clear();
    client.path.clear();
    client.headers.clear();
}

void clear_client(ClientState &client){
    client.fd = -1;
    client.read_buffer.clear();
    client.write_buffer.clear();
    client.bytes_sent = 0;
    client.keep_alive = true;
    client.req_state = ClientReqState::READING_HEADER;
    client.content_length = 0;
    client.method.clear();
    client.path.clear();
    client.headers.clear();
}

