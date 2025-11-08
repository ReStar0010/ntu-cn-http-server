#include "responses.h"
#include <string>
#include <sstream>

using namespace std;

string create_401_response(){
    string body = "Unauthorized\n";
    string response;
    response += "HTTP/1.1 401 Unauthorized\r\n";
    response += "Server: CN2025Server/1.0\r\n"; 
    response += "WWW-Authenticate: Basic realm=\"B12902078\"\r\n"; 
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: " + to_string(body.length()) + "\r\n";
    response += "Connection: Close\r\n"; 
    response += "\r\n";
    response += body;
    return response;
}

string create_404_response(){
    string body = "Not Found\n";
    string response;
    response += "HTTP/1.1 404 Not Found\r\n";
    response += "Server: CN2025Server/1.0\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: 10\r\n";
    response += "\r\n";
    response += body;
    return response;
}

string create_405_response(){
    string body = "Method Not Allowed\n";
    string response;
    response += "HTTP/1.1 405 Method Not Allowed\r\n";
    response += "Server: CN2025Server/1.0\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: " + to_string(body.length()) + "\r\n";
    response += "\r\n";
    response += body;
    return response;
}

string create_500_response(){
    string response;
    response += "HTTP/1.1 500 Internal Server Error\r\n";
    response += "Server: CN2025Server/1.0\r\n";
    response += "Content-Length: 0\r\n";
    response += "\r\n";
    return response;
}

