#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include "client_state.h"
#include <string>

void parse_header(ClientState &client, const std::string &header_strs);
std::string get_MIME_type(const std::string &filename);
bool replace_first(std::string &s, const std::string &tag, const std::string &replacement);
std::string url_encode(const std::string &str);
std::string url_decode(const std::string &str);
std::string html_escape(const std::string &str);

#endif // HTTP_UTILS_H

