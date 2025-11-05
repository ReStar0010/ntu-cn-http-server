/* server.cpp */
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <cstring>      
#include <unistd.h>     
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <poll.h>       
#include <map>
#include <csignal>      
#include <cerrno>       

#define MAX_CLIENTS 101
using namespace std;
enum class ClientReqState{
    READING_HEADER,
    READING_BODY,
    READING_COMPLETE
};
struct ClientState{
    int fd;
    string read_buffer;
    string write_buffer;
    size_t bytes_sent;
    bool keep_alive;
    ClientReqState req_state;
    size_t content_length; // which is body size
    string method;
    string path;
    map<string, string> headers;
};
typedef struct ClientState ClientState;
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
    client.req_state = ClientReqState::READING_HEADER;
    client.content_length = 0;
    client.method.clear();
    client.path.clear();
    client.headers.clear();
}
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
void process_http_request(ClientState &client){
    // NOTE: routing path
    string status_line;
    string content_type;
    string body;
    string response;
    if(client.method == "GET" && client.path == "/"){
        ifstream ifs("web/index.html");
        if(ifs){
            stringstream ss;
            ss << ifs.rdbuf();
            body = ss.str();
        }
        else{
            body = "<h1>Index file not found</h1>";
        }
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = "Content-Type: text/html\r\n";
    }
    else if(client.method == "GET" && client.path == "/upload/file"){
        ifstream ifs("web/uploadf.html");
        if(ifs){
            stringstream ss;
            ss << ifs.rdbuf();
            body = ss.str();
        }
        else{
            body = "<h1>Upload file not found</h1>";
        }
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = "Content-Type: text/html\r\n";
    }
    else if(client.method == "GET" && client.path == "/upload/video"){
        ifstream ifs("web/uploadv.html");
        if(ifs){
            stringstream ss;
            ss << ifs.rdbuf();
            body = ss.str();
        }
        else{
            body = "<h1>Upload video file not found</h1>";
        }
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = "Content-Type: text/html\r\n";
    }
    else if(client.method == "GET" && client.path == "/file/"){
        ifstream ifs("web/listf.rhtml");
        if(ifs){
            stringstream ss;
            ss << ifs.rdbuf();
            body = ss.str();
        }
        else{
            body = "<h1>Not found(/file/)</h1>";
        }
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = "Content-Type: text/html\r\n";
    }
    else if(client.method == "GET" && client.path == "/video/"){
        ifstream ifs("web/listv.rhtml");
        if(ifs){
            stringstream ss;
            ss << ifs.rdbuf();
            body = ss.str();
        }
        else{
            body = "<h1>Not found(/video/)</h1>";
        }
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = "Content-Type: text/html\r\n";
    }
    else if(client.method == "GET" && regex_match(client.path, regex("^/video/.*"))){
        ifstream ifs("web/player.rhtml");
        if(ifs){
            stringstream ss;
            ss << ifs.rdbuf();
            body = ss.str();
        }
        else{
            body = "<h1>Not found(/video/*)</h1>";
        }
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = "Content-Type: text/html\r\n";
    }
    else if(client.method == "POST" && client.path == "/api/file"){
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = "Content-Type: text/html\r\n";
        body = "<h1>File uploaded successfully</h1>";
    }
    else if(client.method == "POST" && client.path == "/api/video"){
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = "Content-Type: text/html\r\n";
        body = "<h1>Video uploaded successfully</h1>";
    }
    else{
        status_line = "HTTP/1.1 404 Not Found\r\n";
        content_type = "Content-Type: text/html\r\n";
        body = "<h1>404 Not Found</h1>";
    }
    // NOTE: construct HTTP response
    response += status_line;
    response += "Server: CN2025Server/1.0\r\n";
    response += content_type;
    if (client.keep_alive) {
        response += "Connection: keep-alive\r\n";
    } else {
        response += "Connection: Close\r\n";
    }
    response += "Content-Length: " + to_string(body.size()) + "\r\n";
    response += "\r\n";
    response += body;
    client.write_buffer = response;
    client.bytes_sent = 0;
    client.read_buffer.clear();
}

int main(int argc, char *argv[]){
    // NOTE: filter arguments
    if(argc != 2){
        cerr << "Usage: " << argv[0] << " <PORT>" << endl;
        return 1;
    }
    // NOTE: check the port in range
    int port = stoi(argv[1]);
    if(port < 1025 || port > 65535){
        cerr << "Port number out of range" << endl;
        return 1;
    }
    // NOTE: create server socket (blocking mode by default)
    cout << "Creating server socket... port: " << port << endl;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        cerr << "Create listening socket failed: " << strerror(errno) << endl;
        return 1;
    }
    int opt = 1;
    // NOTE: set socket options
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        cerr << "Set socket options failed: " << strerror(errno) << endl;
        close(server_fd);
        return 1;
    }
    // NOTE: Removed fcntl() - server socket remains in blocking mode
    // NOTE: server socket structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    // NOTE: bind address
    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        cerr << "Bind address failed: " << strerror(errno) << endl;
        close(server_fd);
        return 1;
    }
    // NOTE: start listening
    if(listen(server_fd, MAX_CLIENTS - 1) < 0){
        cerr << "Listen failed: " << strerror(errno) << endl;
        close(server_fd);
        return 1;
    }
    // NOTE: initialize pollfd array for event-driven I/O with blocking sockets
    struct pollfd fds[MAX_CLIENTS];
    ClientState clients[MAX_CLIENTS];
    memset(fds, 0, sizeof(fds));
    for(int i = 0; i < MAX_CLIENTS; ++i)
        fds[i].fd = -1;
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    cout << "Server is listening on port " << port << "..." << endl;
    
    // NOTE: poll main loop - blocking mode on sockets, poll() for multiplexing
    while(true){
        // NOTE: poll() blocks until event occurs on any socket
        int events_number = poll(fds, MAX_CLIENTS, -1);
        if(events_number == -1){
            cerr << "Poll error: " << strerror(errno) << endl;
            break;
        }
        
        // NOTE: check if server socket has incoming connection
        if(fds[0].revents & POLLIN){
            // NOTE: accept new connection (blocking, but poll() guarantees it won't hang)
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if(client_fd < 0){
                cerr << "Accept failed: " << strerror(errno) << endl;
            }
            else{                
                int i;
                for(i=1;i<MAX_CLIENTS;i++){
                    if(fds[i].fd == -1){
                        initial_client(clients[i]);
                        clients[i].fd = client_fd;
                        // NOTE: add to pollfd array
                        fds[i].fd = client_fd;
                        fds[i].events = POLLIN;
                        cout << "New connection from " << inet_ntoa(client_addr.sin_addr) 
                             << ":" << ntohs(client_addr.sin_port) << " assigned to fd " << client_fd << endl;
                        break;
                    }
                }
                if(i == MAX_CLIENTS){
                    cerr << "Max clients reached, rejecting connection from " 
                         << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;
                    close(client_fd);
                }
            }
        }
        
        // NOTE: check client sockets for incoming data
        for(int i=1;i<MAX_CLIENTS;i++){
            if(fds[i].fd == -1)
                continue;
            
            // NOTE: poll() indicated this socket has data or error
            if(fds[i].revents & (POLLIN | POLLERR)){
                char tmp_buffer[1024];
                memset(tmp_buffer, 0, sizeof(tmp_buffer));
                // NOTE: blocking read, but poll() guarantees data is available
                ssize_t bytes_read = read(fds[i].fd, tmp_buffer, sizeof(tmp_buffer));
                
                if(bytes_read < 0){
                    cerr << "Read error on fd " << fds[i].fd << ": " << strerror(errno) << endl;
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    clear_client(clients[i]);
                    continue;
                }
                else if(bytes_read == 0){
                    cout << "Client on fd " << fds[i].fd << " disconnected." << endl;
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    clear_client(clients[i]);
                    continue;
                }
                else{
                    // NOTE: received data
                    cout << "Received " << bytes_read << " bytes from fd " << fds[i].fd << ": " << string(tmp_buffer, bytes_read) << endl;
                    clients[i].read_buffer.append(string(tmp_buffer, bytes_read));
                }
                // NOTE: deal with each client's request state
                bool keep_processing = true;
                while(keep_processing){
                    keep_processing = false;
                    if(clients[i].req_state == ClientReqState::READING_HEADER){
                        size_t header_end_pos = clients[i].read_buffer.find("\r\n\r\n");
                        if(header_end_pos != string::npos){
                            // NOTE: complete header received
                            string header_strs = clients[i].read_buffer.substr(0, header_end_pos);
                            parse_header(clients[i], header_strs);
                            cout << "Parsed HTTP header from fd " << fds[i].fd << endl;
                            clients[i].read_buffer.erase(0, header_end_pos + 4); // remove header from request
                            // NOTE: check if need to read body
                            if(clients[i].content_length > 0){
                                clients[i].req_state = ClientReqState::READING_BODY;
                                keep_processing = true;
                            }
                            else{
                                clients[i].req_state = ClientReqState::READING_COMPLETE;
                                keep_processing = true;
                            }
                        }
                    }
                    else if(clients[i].req_state == ClientReqState::READING_BODY){
                        if(clients[i].read_buffer.size() >= clients[i].content_length){
                            // NOTE: body received completely
                            clients[i].req_state = ClientReqState::READING_COMPLETE;
                            keep_processing = true;
                        }
                    }
                    else if(clients[i].req_state == ClientReqState::READING_COMPLETE){
                        // NOTE: process complete HTTP request
                        string body;
                        if(clients[i].content_length > 0){
                            body = clients[i].read_buffer.substr(0, clients[i].content_length);
                            clients[i].read_buffer.erase(0, clients[i].content_length);
                        }
                        process_http_request(clients[i]);
                        cout << "Processed HTTP request from fd " << fds[i].fd <<  "method: " << clients[i].method << " path: " << clients[i].path << endl;
                       if(clients[i].write_buffer.size() > 0){
                            // NOTE: data to send
                            fds[i].events = POLLOUT;
                       }
                       clients[i].req_state = ClientReqState::READING_HEADER;
                       clients[i].content_length = 0;
                       if(clients[i].read_buffer.size() > 0){
                            // NOTE: process next request in buffer
                            keep_processing = true;
                       }

                    }
                }
            }
            if(fds[i].revents & POLLOUT){
                // NOTE: ready to send data
                if(!clients[i].write_buffer.empty()){
                    // NOTE: send remaining data
                    ssize_t bytes_to_send = clients[i].write_buffer.size() - clients[i].bytes_sent;
                    ssize_t bytes_sent = write(fds[i].fd, clients[i].write_buffer.c_str() + clients[i].bytes_sent, bytes_to_send);
                    if(bytes_sent < 0){
                        cerr << "Write error on fd " << fds[i].fd << ": " << strerror(errno) << endl;
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        clear_client(clients[i]);
                        continue;
                    }
                    cout << "Sent " << bytes_sent << " bytes to fd " << fds[i].fd << endl;
                    clients[i].bytes_sent += bytes_sent;
                    // NOTE: check if send all data
                    if(bytes_sent >= bytes_to_send){
                        cout << "All data sent to fd " << fds[i].fd << endl;
                        clients[i].write_buffer.clear();
                        clients[i].bytes_sent = 0;
                        if(clients[i].keep_alive){
                            fds[i].events = POLLIN;
                        }
                        else{
                            cout << "Closing connection to fd " << fds[i].fd << " as per Connection: close header." << endl;
                            close(fds[i].fd);
                            fds[i].fd = -1;
                            clear_client(clients[i]);
                        }
                    }
                    else{
                        cout << "Remaining " << clients[i].write_buffer.size() - clients[i].bytes_sent << " bytes to send to fd " << fds[i].fd << endl;   
                        clients[i].bytes_sent += bytes_sent;
                    }
                }
                else{
                    // NOTE: nothing to send, switch back to read only
                    fds[i].events = POLLIN;
                }
            }
        }
    }
    
    // NOTE: cleanup - close all sockets
    for(int i=0;i<MAX_CLIENTS;i++){
        if(fds[i].fd != -1){
            close(fds[i].fd);
            clear_client(clients[i]);
        }
    }
    return 0;
}