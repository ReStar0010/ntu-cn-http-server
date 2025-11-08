/* client.cpp */
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>      // 
#include <vector>
#include <sstream>
#include "utils/base64.h"

using namespace std;

// 
int g_server_fd = -1;
string g_host;
int g_port;
string g_auth_credential; 
bool connect_to_server() {
    if (g_server_fd != -1) {
        close(g_server_fd);
    }

    // NOTE resolve hostname
    struct hostent *server = gethostbyname(g_host.c_str());
    if (server == NULL) {
        cerr << "Error: No such host: " << g_host << endl;
        return false;
    }

    // NOTE socket connect to server
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        cerr << "Error creating socket: " << strerror(errno) << endl;
        return false;
    }

    // NOTE initialize server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // NOTE connect to server
    if (connect(g_server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Error connecting: " << strerror(errno) << endl;
        close(g_server_fd);
        g_server_fd = -1;
        return false;
    }
    
    cout << "Connected to " << g_host << ":" << g_port << endl;
    return true;
}


int main(int argc, char *argv[]) {
    // NOTE: argument parsing check 
    if (argc < 3 || argc > 4) {
        cerr << "Usage: ./client [host] [port] [username:password]" << endl; // 
        return -1; // 
    }

    g_host = argv[1];
    g_port = stoi(argv[2]);
    
    if (argc == 4) {
        g_auth_credential = argv[3]; // 
        cout << "Credentials provided: " << g_auth_credential << endl;
    }

    if (!connect_to_server()) {
        return 1;
    }

    // NOTE: interactive command loop
    string line;
    while (true) {
        cout << "> "; // 
        if (!getline(cin, line)) {
            break; // EOF (e.g., Ctrl+D)
        }
        
        stringstream ss(line);
        string cmd;
        ss >> cmd;
        
        if (cmd == "quit") {
            cout << "Bye." << endl;
            break; // 
        } else if (cmd == "get") {
            // TODO: Implement 'get'
            cout << "Command 'get' not implemented yet." << endl;
        } else if (cmd == "put") {
            // TODO: Implement 'put'
            cout << "Command 'put' not implemented yet." << endl;
        } else if (cmd == "putv") {
            // TODO: Implement 'putv'
            cout << "Command 'putv' not implemented yet." << endl;
        } else if (cmd == "auth") {
            // TODO: Implement 'auth'
            cout << "Command 'auth' not implemented yet." << endl;
        } else if (!cmd.empty()) {
            cerr << "Command Not Found." << endl; // 
        }
    }
    if (g_server_fd != -1) {
        close(g_server_fd);
    }
    return 0;
}