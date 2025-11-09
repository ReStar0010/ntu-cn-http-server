/* client.cpp */
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <iomanip>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>      // 
#include <vector>
#include <sstream>
#include <map>
#include <fstream>
#include "utils/base64.h"
#include "utils/http_utils.h"

using namespace std;
struct HttpResponse{
    int status_code;
    string status_message;
    map<string, string> headers;
    string body;
    bool connection_close;
};
typedef struct HttpResponse HttpResponse;
// NOTE Global variables
int g_server_fd = -1;
string g_host;
int g_port;
string g_auth_credential;

bool connect_to_server() {
    if (g_server_fd != -1) {
        close(g_server_fd);
        g_server_fd = -1;
    }

    struct addrinfo hints, *res, *p;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;

    string port_str = to_string(g_port);

    // NOTE: getaddrinfo
    if ((status = getaddrinfo(g_host.c_str(), port_str.c_str(), &hints, &res)) != 0) {
        cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
        return false;
    }

    // NOTE iterate all result
    for(p = res; p != NULL; p = p->ai_next) {
        // 
        g_server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (g_server_fd < 0) {
            perror("client: socket");
            continue;
        }
        if (connect(g_server_fd, p->ai_addr, p->ai_addrlen) < 0) {
            close(g_server_fd);
            g_server_fd = -1;
            perror("client: connect");
            continue;
        }
        break;
    }

    freeaddrinfo(res); 

    if (g_server_fd == -1) {
        cerr << "Failed to connect to " << g_host << ":" << g_port << endl;
        return false;
    }

    return true;
}
string build_http_request(const string &method, const string &path, const map<string, string> &headers, const string &body="") {
   stringstream request;
    request << method << " " << path << " HTTP/1.1\r\n";
    request << "Host: " << g_host << ":" << g_port << "\r\n"; 
    request << "User-Agent: CN2025Client/1.0\r\n";  
    request << "Connection: keep-alive\r\n";
    if(!g_auth_credential.empty()){
        string encoded_auth = base64_encode(g_auth_credential);
        request << "Authorization: Basic " << encoded_auth << "\r\n";
    }
    for(const auto &header: headers){
        request << header.first << ": " << header.second << "\r\n";
    }
    if(!body.empty()){
        request << "Content-Length: " << body.length() << "\r\n";
    }
    request << "\r\n";
    request << body;
    return request.str();
}

HttpResponse recv_and_parse_http_response(int socket_fd){
    HttpResponse response;
    char buffer[4096];
    string raw_response;

    // NOTE: receive header
    int bytes_received;
    while((bytes_received = recv(socket_fd, buffer, sizeof(buffer), 0)) > 0){
        raw_response.append(buffer, bytes_received);
        // NOTE: check whether header is fully received
        if(raw_response.find("\r\n\r\n") != string::npos){
            break; 
        }
    }
    // NOTE: parse header
    if(bytes_received <= 0){
        if(bytes_received == 0){
            cout << "Server closed connection." << endl;
        } 
        else {
            perror("recv error");
        }
        response.connection_close = true;
        response.status_code = 0;
        return response;
    }

    size_t header_end_pos = raw_response.find("\r\n\r\n");
    if(header_end_pos == string::npos){
       cerr << "Error: Invalid HTTP response." << endl;
        response.status_code = -1;
        return response;
    }

    string header_part = raw_response.substr(0, header_end_pos);
    string body_part = raw_response.substr(header_end_pos + 4); // pass "\r\n\r\n"

    // NOTE: parse first line
    stringstream header_ss(header_part);
    string line;
    getline(header_ss, line);
    stringstream status_ss(line);
    string http_version;
    status_ss >> http_version >> response.status_code;
    // NOTE: parse other lines
    int content_length = 0;
    while(getline(header_ss, line)){
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if(line.empty()) break;
        size_t colon_pos = line.find(':');
        if(colon_pos != string::npos){
            string key = line.substr(0, colon_pos);
            string value = line.substr(colon_pos + 2);
            for(char &C : key) C = tolower(C); // to lower case
            for(char &C : value) C = tolower(C);
            response.headers[key] = value;
            if(key == "content-length"){
                content_length = stoi(value);
            }
            if(key == "connection" && value == "close"){
                response.connection_close = true;
            }
        }
    }
    // NOTE: parse body
    int current_body_length = body_part.length();
    while(current_body_length < content_length){
        bytes_received = recv(socket_fd, buffer, sizeof(buffer), 0);
        if(bytes_received <= 0){
            if (bytes_received == 0) {
            cout << "Server closed connection." << endl;
         } else {
            perror("recv error");
         }
            response.connection_close = true;
            response.status_code = -1; // 
            return response;
        }
        body_part.append(buffer, bytes_received);
        current_body_length += bytes_received;
    }
    response.body = body_part;
    return response;
}
string read_file_binary(const string& filepath) {
    // NOTE: read entire file into string
    ifstream file(filepath, ios::binary | ios::ate);
    if (!file.is_open()) {
        return ""; // 
    }

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);

    string buffer(size, '\0'); // 
    if (file.read(&buffer[0], size)) {
        return buffer;
    }
    
    return ""; // 
}
string generate_boundary() {
    // NOTE: generate a random boundary string for multipart/form-data
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    int len = 30; 
    string boundary = "----WebKitFormBoundary";
    for (int i = 0; i < len; ++i) {
        boundary += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return boundary;
}


int main(int argc, char *argv[]) {
    // NOTE: argument parsing check 
    if (argc < 3 || argc > 4) {
        cout << "Usage: ./client [host] [port] [username:password]" << endl; // 
        return -1; // 
    }

    g_host = argv[1];
    g_port = stoi(argv[2]);
    
    if (argc == 4) {
        g_auth_credential = argv[3]; // 
    }

    if (!connect_to_server()) {
        return 1;
    }

    // NOTE: interactive command loop
    string line;
    while (true) {
        cout << "> " << flush; // 
        if (!getline(cin, line)) {
            break; // EOF (e.g., Ctrl+D)
        }
        
        stringstream ss(line);
        string cmd;
        ss >> cmd;
        
        if (cmd == "quit") {
            cout << "Bye." << endl;
            break; // 
        } 
        else if (cmd == "get") {
            string filename;
            string linepart;
            // NOTE: send to server
            if(ss >> filename){
                // NOTE: the filename may contain spaces
                while(ss >> linepart){
                    filename += " " + linepart;
                }
                if(g_server_fd == -1){
                    if(!connect_to_server()){
                        cout << "Command failed. Cannot reconnect to server." << endl;
                        continue;
                    }
                }
                string encode_name = url_encode(filename);
                string path = "/api/file/" + encode_name;

                map<string, string> headers;
                string request = build_http_request("GET", path, headers);
                if(send(g_server_fd, request.c_str(), request.length(), 0) == -1){
                    perror("send");
                    close(g_server_fd);
                    g_server_fd = -1;
                    cout << "Command failed. Send error." << endl;
                    continue;
                }
                // NOTE: receive response
                HttpResponse response = recv_and_parse_http_response(g_server_fd);
                if(response.status_code == 200){
                    // Create files directory if it doesn't exist
                    mkdir("files", 0755);
                    ofstream outfile("files/" + filename, ios::binary);
                    if(outfile.fail()){
                        cout << "Command failed. Cannot create local file." << endl;
                        continue;
                    }
                    outfile.write(response.body.c_str(), response.body.size());
                    outfile.close();
                    cout << "Command succeeded." << endl;
                }
                else if(response.status_code == 404){
                    cout << "Command failed. File not found on server." << endl;
                }
                else if(response.status_code == 500){
                    cout << "Command failed. Internal server error." << endl;
                }
                else if(response.status_code == -1){
                    cout << "Command failed. Connection error." << endl;
                    g_server_fd = -1;
                }
                else{
                    cout << "Command failed. Server returned status " << response.status_code << endl;
                }
                if (response.connection_close) {
                    close(g_server_fd);
                    g_server_fd = -1;
                }
            }
            else {
                cout << "Usage: get [file]" << endl; // 
            }
        } 
        else if (cmd == "put" || cmd == "putv") {
            string filename;
            string linepart;
            // NOTE: send to server
            if(ss >> filename){
                // Check if there are more parts (filename with spaces)
                while(ss >> linepart){
                    filename += " " + linepart;
                }
            }
            else {
                if(cmd == "put"){
                    cout << "Usage: put [file]" << endl; 
                }
                else{
                    cout << "Usage: putv [file]" << endl; 
                }
                continue;
            }
            // NOTE: read file from local
            string file_content = read_file_binary(filename);
            if(file_content.empty()){
                cout << "Command failed. File not found on local." << endl; 
                continue;
            }
            if(g_server_fd == -1){
                if(!connect_to_server()){
                    cout << "Command failed. Cannot reconnect to server." << endl;
                    continue;
                }
            }
            // NOTE: build multipart/form-data body
            // Extract basename from filename (in case it contains a path)
            string basename = filename;
            size_t last_slash = filename.find_last_of("/\\");
            if (last_slash != string::npos) {
                basename = filename.substr(last_slash + 1);
            }
            
            string boundary = generate_boundary();
            stringstream body_ss;
            body_ss << "--" << boundary << "\r\n";
            body_ss << "Content-Disposition: form-data; name=\"upfile\"; filename=\"" << basename << "\"\r\n";
            body_ss << "Content-Type: application/octet-stream\r\n";
            body_ss << "\r\n";
            body_ss.write(file_content.c_str(), file_content.length());
            body_ss << "\r\n";
            body_ss << "--" << boundary << "--\r\n";
            string body = body_ss.str();
            string path;
            if(cmd == "put"){
                path = "/api/file";
            }
            else{
                path = "/api/video";
            }
            // NOTE: build request
            map<string, string> headers;
            headers["Content-Type"] = "multipart/form-data; boundary=" + boundary;
            string request = build_http_request("POST", path, headers, body);
            // NOTE: send request
            if (send(g_server_fd, request.c_str(), request.length(), 0) < 0) {
                perror("send");
                close(g_server_fd);
                g_server_fd = -1;
                cout << "Command failed. Send error." << endl;
                continue;
            }
            HttpResponse response = recv_and_parse_http_response(g_server_fd);
            if(response.status_code == 200){
                cout << "Command succeeded." << endl;
            }
            else if(response.status_code == 401){
                cout << "Command failed. Invalid user or wrong password." << endl;
            }
            else if(response.status_code == 404){
                cout << "Command failed. File not found on server." << endl;
            } 
            else if (response.status_code == -1) {
                 cout << "Command failed. Connection error." << endl;
                 g_server_fd = -1; 
            } else {
                cout << "Command failed. Server returned status " << response.status_code << endl;
            }
            if(response.connection_close){
                close(g_server_fd);
                g_server_fd = -1;
            }
        } 
        else if (cmd == "auth") {
            string credential;
            if (ss >> credential){
                g_auth_credential = credential;
                cout << "Command succeeded." << endl;
            }
            else{
                cout << "Usage: auth [username:password]" << endl;
            }
        } 
        else if (!cmd.empty()) {
            cout << "Command not found." << endl; // 
        }
    }
    if (g_server_fd != -1) {
        close(g_server_fd);
    }
    return 0;
}