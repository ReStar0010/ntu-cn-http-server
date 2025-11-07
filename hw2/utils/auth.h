#ifndef AUTH_H
#define AUTH_H

#include "client_state.h"
#include <string>

bool load_secret(const std::string &filename);
bool authenticate_user(ClientState &client);

#endif // AUTH_H

