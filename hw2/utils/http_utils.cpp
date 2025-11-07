#include "http_utils.h"
#include <sstream>
#include <string>
#include <iostream>
#include <cctype>

using namespace std;

void parse_header(ClientState &client, const string &header_strs){
    client.headers.clear();
    client.keep_alive = true; // default
    stringstream ss(header_strs);
    string line;
    getline(ss, line);
    // NOTE: parse first line
    if (!line.empty() && line.back() == '\r') line.pop_back();
    stringstream first_line_ss(line);
    first_line_ss >> client.method >> client.path;
    while(getline(ss, line)){
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if(line.empty()) break;
        size_t colon_pos = line.find(':');
        if(colon_pos != string::npos){
            string key = line.substr(0, colon_pos);
            string value = line.substr(colon_pos + 1);
            // NOTE: trim space
            key.erase(0, key.find_first_not_of("\t "));
            key.erase(key.find_last_not_of("\t ") + 1);
            value.erase(0, value.find_first_not_of("\t "));
            value.erase(value.find_last_not_of("\t ") + 1);
            for(char &C : key) C = tolower(C); // to lower case
            client.headers[key] = value;
        }
    }
    if(client.headers.find("connection") != client.headers.end()){
        string conn_value = client.headers["connection"];
        for(char &C : conn_value) C = tolower(C);
        if(conn_value == "close"){
            client.keep_alive = false;
        }
    }
    client.content_length = 0;
    if(client.headers.find("content-length") != client.headers.end()){
        try{
            client.content_length = stoul(client.headers["content-length"]);
        }
        catch(...){
            client.content_length = 0;
            cout << "Invalid Content-Length header value." << endl;
        }
    }
}

string get_MIME_type(const string &filename){
    size_t dot_pos = filename.find_last_of(".");
    if(dot_pos == string::npos){
        return "text/plain";
    }
    string extension = filename.substr(dot_pos + 1);
    // normalize to lower-case
    for (char &c : extension) c = tolower(c);

    if (extension == "html" || extension == "rhtml") {
        return "text/html";
    } else if (extension == "css") {
        return "text/css";
    } else if (extension == "js") {
        return "application/javascript";
    } else if (extension == "png") {
        return "image/png";
    } else if (extension == "jpg" || extension == "jpeg") {
        return "image/jpeg";
    } else if (extension == "gif") {
        return "image/gif";
    } else if (extension == "mpd") {
        return "application/dash+xml";
    } else if (extension == "mp4") {
        return "video/mp4";
    }
    return "application/octet-stream";
}

bool replace_first(string &s, const string &tag, const string &replacement){
    size_t pos = s.find(tag);
    if(pos == string::npos)
        return false;
    s.replace(pos, tag.length(), replacement);
    return true;
}

string url_encode(const string &str){
    string encoded;
    encoded.reserve(str.length() * 3); // Reserve space for worst case
    for(size_t i = 0; i < str.length(); ++i){
        unsigned char c = str[i];
        if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~'){
            encoded += c;
        }
        else{
            encoded += '%';
            encoded += "0123456789ABCDEF"[c >> 4];
            encoded += "0123456789ABCDEF"[c & 0x0F];
        }
    }
    return encoded;
}

string url_decode(const string &str){
    string decoded;
    decoded.reserve(str.length());
    for(size_t i = 0; i < str.length(); ++i){
        if(str[i] == '%' && i + 2 < str.length()){
            int value = 0;
            if(isdigit(str[i+1])){
                value = (str[i+1] - '0') * 16;
            }
            else if(str[i+1] >= 'A' && str[i+1] <= 'F'){
                value = (str[i+1] - 'A' + 10) * 16;
            }
            else if(str[i+1] >= 'a' && str[i+1] <= 'f'){
                value = (str[i+1] - 'a' + 10) * 16;
            }
            
            if(isdigit(str[i+2])){
                value += (str[i+2] - '0');
            }
            else if(str[i+2] >= 'A' && str[i+2] <= 'F'){
                value += (str[i+2] - 'A' + 10);
            }
            else if(str[i+2] >= 'a' && str[i+2] <= 'f'){
                value += (str[i+2] - 'a' + 10);
            }
            
            decoded += static_cast<char>(value);
            i += 2;
        }
        else if(str[i] == '+'){
            decoded += ' ';
        }
        else{
            decoded += str[i];
        }
    }
    return decoded;
}

