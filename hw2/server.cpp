/* server.cpp */
#include <iostream>
#include <string>
#include <vector>
#include <cstring>      
#include <unistd.h>     
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <poll.h>       
#include <fcntl.h>     
#include <csignal>      
#include <cerrno>       

#define MAX_CLIENTS 101
using namespace std;
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
    // NOTE: create server socket
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
    // NOTE: set non-blocking mode
    if(fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0){
        cerr << "Set non-blocking mode failed: " << strerror(errno) << endl;
        close(server_fd);
        return 1;
    }
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
    // NOTE: initializze pollfd
    struct pollfd fds[MAX_CLIENTS];
    memset(fds, 0, sizeof(fds));
    for(int i = 0; i < MAX_CLIENTS; ++i)
        fds[i].fd = -1;
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    cout << "Server is listening on port " << port << "..." << endl;
    // NOTE: poll main loop
    while(true){
        int events_number = poll(fds, MAX_CLIENTS, -1);
        if(events_number == -1){
            cerr << "Poll error: " << strerror(errno) << endl;
            break;
        }
        if(fds[0].revents & POLL_IN){
            // NOTE: accept new connection(non blocking)
            while(true){
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if(client_fd < 0){
                    cerr << "Accept failed: " << strerror(errno) << endl;
                    break;
                }
                // NOTE: set non-blocking mode for client socket
                if(fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0){
                    cerr << "Set non-blocking mode failed: " << strerror(errno) << endl;
                    close(client_fd);
                    continue;
                }
                // NOTE: add client into pollfd
                int i;
                for(i=1;i<MAX_CLIENTS;i++){
                    if(fds[i].fd == -1){
                        fds[i].fd = client_fd;
                        fds[i].events = POLL_IN;
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
        else{
            // NOTE: check client sockets
            for(int i=1;i<MAX_CLIENTS;i++){
                if(fds[i].fd == -1)
                    continue;
                if(fds[i].revents & POLLIN){
                    char buffer[1024];
                    memset(buffer, 0, sizeof(buffer));
                    ssize_t bytes_read = read(fds[i].fd, buffer, sizeof(buffer));
                    if(bytes_read < 0){
                        cerr << "Read error on fd " << fds[i].fd << ": " << strerror(errno) << endl;
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        continue;
                    }
                    else if(bytes_read == 0){
                        cout << "Client on fd " << fds[i].fd << " disconnected." << endl;
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        continue;
                    }
                    else{
                        cout << "Received " << bytes_read << " bytes from fd " << fds[i].fd << ": " << string(buffer, bytes_read) << endl;
                        // FIX: Add HTTP parser here
                        const char* response = 
                            "HTTP/1.1 200 OK\r\n"
                            "Server: CN2025Server/1.0\r\n" // 
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 12\r\n"
                            "Connection: Close\r\n" // [cite: 199]
                            "\r\n"
                            "Hello World\n";
                        ssize_t bytes_sent = write(fds[i].fd, response, strlen(response));
                        close(fds[i].fd);
                        fds[i].fd = -1;
                    }
                }
            }
        }
    }
    // NOTE: end of poll loop
    for(int i=0;i<MAX_CLIENTS;i++){
        close(fds[i].fd);
        fds[i].fd = -1;
    }
    return 0;
}