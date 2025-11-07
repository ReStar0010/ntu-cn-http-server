#include "auth.h"
#include "base64.h"
#include "responses.h"
#include <iostream>
#include <fstream>
#include <string>
#include <map>

using namespace std;

static map<string, string> g_secret;

bool load_secret(const string &filename){
    ifstream ifs(filename);
    if(!ifs){
        cerr << "Failed to open secret file: " << filename << endl;
        return false;
    }
    string line;
    while(getline(ifs, line)){
        size_t colone_pos = line.find(":");
        if(colone_pos != string::npos){
            string username = line.substr(0, colone_pos);
            string password = line.substr(colone_pos + 1);
            g_secret[username] = password;
            cout << "Load Secret: " << username << ":" << password << endl;
        }
    }
    cout << "Load " << g_secret.size() << " Secrets Completed" << endl;
    return true;
}

bool authenticate_user(ClientState &client){
    bool is_auth = false;
    if(client.headers.find("authorization") != client.headers.end()){
        string auth_header = client.headers["authorization"];
        string prefix = "Basic ";
        if(auth_header.substr(0, prefix.size()) == prefix){
            string credentials_base64 = auth_header.substr(prefix.size());
            string credentials = base64_decode(credentials_base64);
            size_t colon_pos = credentials.find(":");
            if(colon_pos != string::npos){
                string username = credentials.substr(0, colon_pos);
                string password = credentials.substr(colon_pos + 1);
                if(g_secret.find(username) != g_secret.end() && g_secret[username] == password){
                    is_auth = true;
                }
            }
        }
    }
    if(!is_auth){
        // NOTE: not authorized
        client.write_buffer = create_401_response();
        client.bytes_sent = 0;
        client.keep_alive = false; 
        cout << "Unauthorized " << client.method << " " << client.path << " for fd " << client.fd << endl;        
        return false;
    }
    cout << "Authorized " << client.method << " " << client.path << " for fd " << client.fd << endl;
    return true;
}

