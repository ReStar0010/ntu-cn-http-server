#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include "client_state.h"
#include <string>

void process_http_request(ClientState &client, std::string request_body);

#endif // REQUEST_HANDLER_H

