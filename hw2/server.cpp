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
#include <csignal>      
#include <cerrno>       

#define MAX_CLIENTS 101
using namespace std;
struct ClientState{
    int fd;
    string read_buffer;
    string write_buffer;
    size_t bytes_sent;
};
typedef struct ClientState ClientState;
void initial_client(ClientState &client){
    client.fd = -1;
    client.read_buffer.clear();
    client.write_buffer.clear();
    client.bytes_sent = 0;
}
void clear_client(ClientState &client){
    client.fd = -1;
    client.read_buffer.clear();
    client.write_buffer.clear();
    client.bytes_sent = 0;
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
                    int header_end_pos = clients[i].read_buffer.find("\r\n\r\n");
                    if(header_end_pos != string::npos){
                        // NOTE: The entire request has been received
                        cout << "Complete HTTP request received from fd " << fds[i].fd << endl;
                        cout << "Request: -----\n" << clients[i].read_buffer << "\n-----" << endl;
                        // NOTE: parseing HTTP request
                        string request_str = clients[i].read_buffer.substr(0, header_end_pos);
                        // NOTE: parse first line
                        string method, path, version;
                        size_t method_end = request_str.find(' ');
                        if(method_end != string::npos){
                            method = request_str.substr(0, method_end);
                            size_t path_end = request_str.find(' ', method_end + 1);
                            if(path_end != string::npos){
                                path = request_str.substr(method_end + 1, path_end - method_end - 1);
                                version = request_str.substr(path_end + 1);
                            }
                        }
                        // NOTE: routing path
                        string status_line;
                        string content_type;
                        string body;
                        string response;
                        if(method == "GET" && path == "/"){
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
                        else if(method == "GET" && path == "/upload/file"){
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
                        else if(method == "GET" && path == "/upload/video"){
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
                        else if(method == "GET" && path == "/file/"){
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
                        else if(method == "GET" && path == "/video/"){
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
                        else if(method == "GET" && regex_match(path, regex("^/video/.*"))){
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
                        else{
                            status_line = "HTTP/1.1 404 Not Found\r\n";
                            content_type = "Content-Type: text/html\r\n";
                            body = "<h1>404 Not Found</h1>";
                        }
                        // NOTE: construct HTTP response
                        response += status_line;
                        response += "Server: CN2025Server/1.0\r\n";
                        response += content_type;
                        response += "Content-Length: " + to_string(body.size()) + "\r\n";
                        response += "\r\n";
                        response += body;
                        clients[i].write_buffer = response;
                        clients[i].bytes_sent = 0;
                        clients[i].read_buffer.clear();
                        fds[i].events = POLLOUT | POLLIN;
                    }
                    else{
                        // NOTE: incomplete request, continue reading
                        cout << "Incomplete HTTP request from fd " << fds[i].fd << ", waiting for more data." << endl;
                    }
                }
            }
            if(fds[i].revents & POLLOUT){
                // NOTE: ready to send data
                if(!clients[i].write_buffer.empty()){
                    // NOTE: send remaining data
                    ssize_t bytes_to_send = clients[i].write_buffer.size() - clients[i].bytes_sent;
                    ssize_t bytes_sent = write(fds[i].fd, clients[i].write_buffer.c_str() + clients[i].bytes_sent, bytes_to_send);
                    cout << "Sent " << bytes_sent << " bytes to fd " << fds[i].fd << endl;
                    // NOTE: check if send all data
                    if(bytes_sent == bytes_to_send){
                        cout << "All data sent to fd " << fds[i].fd << endl;
                        clients[i].write_buffer.clear();
                        clients[i].bytes_sent = 0;
                        fds[i].events = POLLIN;
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
            fds[i].fd = -1;
        }
    }
    return 0;
}