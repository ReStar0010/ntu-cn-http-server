/* server.cpp */
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <cstring>      
#include <unistd.h>     
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <poll.h>       
#include <signal.h>
#include <map>
#include <cerrno>       
#include <chrono>
#include "utils/base64.h"


// *FIXME  Remember to turn off output

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
map<string, string> g_secret;
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
string create_401_response(){
    string body = "Unauthorized\n";
    string response;
    response += "HTTP/1.1 401 Unauthorized\r\n";
    response += "Server: CN2025Server/1.0\r\n";
    // 規格要求：必須包含 WWW-Authenticate 標頭
    // 請將 "B10902999" 替換成你自己的學號 
    response += "WWW-Authenticate: Basic realm=\"B12902078\"\r\n"; 
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: " + to_string(body.length()) + "\r\n";
    response += "Connection: Close\r\n"; // 認證失敗時，通常會關閉連線
    response += "\r\n";
    response += body;
    return response;
}
void start_stash_conversation(const string &tmp_video_path, const string &video_name){
    cout << "Starting stash conversation for video: " << video_name << endl;
    cout << "Forking process to stash video" << endl;
    pid_t pid = fork();
    if(pid < 0){
        cerr << "Fork failed for stashing video: " << strerror(errno) << endl;
        return;
    }
    else if(pid == 0){
        // NOTE: mute the child process output
        int fd_null = open("/dev/null", O_WRONLY);
        dup2(fd_null, STDOUT_FILENO);
        dup2(fd_null, STDERR_FILENO);
        close(fd_null);
        // NOTE: child process, do stashing 
        string output_dir = "web/videos/" + video_name; // could be path traversal
        mkdir(output_dir.c_str(), 0755);
        string mpd_output_path = output_dir + "/dash.mpd";
        const char *args[] = {
            "ffmpeg",
            "-re", "-i", tmp_video_path.c_str(),
            "-c:a", "aac", "-c:v", "libx264",
            "-map", "0", "-b:v:1", "6M", "-s:v:1", "1920x1080", "-profile:v:1", "high",
            "-map", "0", "-b:v:0", "144k", "-s:v:0", "256x144", "-profile:v:0", "baseline",
            "-bf", "1", "-keyint_min", "120", "-g", "120", "-sc_threshold", "0", "-b_strategy", "0",
            "-ar:a:1", "22050", "-use_timeline", "1", "-use_template", "1",
            "-adaptation_sets", "id=0,streams=v id=1,streams=a",
            "-f", "dash",
            mpd_output_path.c_str(),
            NULL //
        };
        execvp("ffmpeg", (char* const*)args);
        cerr << "Exec ffmpeg failed for stashing video: " << strerror(errno) << endl;
        exit(1);
    }

    cout << "Stash process forked with PID " << pid << " for video: " << video_name << endl;
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
void process_http_request(ClientState &client, string request_body){
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
    else if(client.method == "GET" && client.path.rfind("/api/file/", 0) == 0){
        // NOTE: get safe filename
        if(authenticate_user(client) == false){
            status_line = "HTTP/1.1 403 Forbidden\r\n";
            body = "<h1>Forbidden</h1>";
        }
        else{
            //TODO -  URL decode filename
            string filename = client.path.substr(10); // length of "/api/file/" is 10
            string file_path = "web/files/" + filename;
            ifstream ifs(file_path, ios::binary);
            if(ifs){
                stringstream ss;
                ss << ifs.rdbuf();
                body = ss.str();
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "Content-Type: " + get_MIME_type(filename) + "\r\n";
            }
            else{
                status_line = "HTTP/1.1 404 Not Found\r\n";
                body = "<h1>File not found</h1>";
            }
        }
    }
    else if(client.method == "GET" && client.path.rfind("/api/video/", 0) == 0){
        // NOTE: get safe video name
        if(authenticate_user(client) == false){
            status_line = "HTTP/1.1 403 Forbidden\r\n";
            body = "<h1>Forbidden</h1>";
        }
        else{
            string filename = client.path.substr(11); // length of "/api/file/" is 10
            //TODO -  URL decode filename
            string file_path = "web/videos/" + filename;
            cout << "filepath: " << file_path << endl;
            ifstream ifs(file_path, ios::binary);
            if(ifs){
                stringstream ss;
                ss << ifs.rdbuf();
                body = ss.str();
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "Content-Type: " + get_MIME_type(filename) + "\r\n";
            }
            else{
                status_line = "HTTP/1.1 404 Not Found\r\n";
                body = "<h1>File not found</h1>";
            }
        }
    }
    else if(client.method == "POST" && client.path == "/api/file"){
        // NOTE: check authorization first
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
            cerr << "Unauthorized upload request " << "for fd " << client.fd << endl;
            return;
        }
        cout << "Authorized upload request " << "for fd " << client.fd << endl;
        // NOTE: parse upload file
        string boundary;
        string content_type_header = client.headers["content-type"];
        size_t boundary_pos = content_type_header.find("boundary=");
        if(content_type_header.rfind("multipart/form-data") != string::npos && boundary_pos != string::npos){
            boundary = content_type_header.substr(boundary_pos + 9); 
            // NOTE: extract file content between boundaries
            size_t filename_pos = request_body.find("filename=\"");
            size_t filename_end_pos = 0;
            if(filename_pos == string::npos){
                status_line = "HTTP/1.1 400 Bad Request\r\n";
                body = "<h1>No filename in multipart/form-data</h1>";
            }
            else{
                // NOTE: extract filename
                filename_pos += 10; // move past 'filename="'
                filename_end_pos = request_body.find("\"", filename_pos);
                string filename = request_body.substr(filename_pos, filename_end_pos - filename_pos);
                size_t last_slash = filename.find_last_of("/\\");
                if (last_slash != string::npos) {
                    filename = filename.substr(last_slash + 1);
                }
                // NOTE: extract file data
                size_t data_start_pos = request_body.find("\r\n\r\n", filename_end_pos) + 4; // move past "\r\n\r\n"
                if(data_start_pos != string::npos){
                    size_t data_end_pos = request_body.find(boundary, data_start_pos);
                    if(data_end_pos != string::npos){
                        string file_content = request_body.substr(data_start_pos, data_end_pos - data_start_pos - 2);
                        string save_path = "web/files/" + filename;
                        // NOTE: write file data
                        ofstream ofs(save_path, ios::binary);
                        if(ofs){
                            ofs.write(file_content.c_str(), file_content.size());
                            ofs.close();
                            status_line = "HTTP/1.1 200 OK\r\n";
                            body = "<h1>File uploaded successfully</h1>";
                        }
                        else{
                            status_line = "HTTP/1.1 500 Internal Server Error\r\n";
                            body = "<h1>Failed to save uploaded file</h1>";
                        }                
                    }
                    else{                        
                        status_line = "HTTP/1.1 400 Bad Request\r\n";
                        body = "<h1>Invalid multipart/form-data format</h1>";
                    }
                }
                else{
                    status_line = "HTTP/1.1 400 Bad Request\r\n";
                    body = "<h1>Invalid multipart/form-data format</h1>";
                }
            }
        }
        else{
            status_line = "HTTP/1.1 400 Bad Request\r\n";
            body = "<h1>multipart/form-data or not boundary</h1>";
        }
        content_type = "Content-Type: text/html\r\n"; // Add this line before response construction
    }
    else if(client.method == "POST" && client.path == "/api/video"){
        // NOTE: check authorization first
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
            cerr << "Unauthorized upload request " << "for fd " << client.fd << endl;
            return;
        }
        cout << "Authorized video upload request " << "for fd " << client.fd << endl;
        // NOTE: parse upload video
        string boundary;
        string content_type_header = client.headers["content-type"];
        size_t boundary_pos = content_type_header.find("boundary=");
        if(content_type_header.rfind("multipart/form-data") != string::npos && boundary_pos != string::npos){
            boundary = content_type_header.substr(boundary_pos + 9);
            size_t filename_start_pos = request_body.find("filename=\"");
            if(filename_start_pos == string::npos){
                status_line = "HTTP/1.1 400 Bad Request\r\n";
                body = "<h1>No filename in multipart/form-data</h1>";
            } 
            else{
                filename_start_pos += 10; // move past filename
                size_t filename_end_pos = request_body.find("\"", filename_start_pos);
                string raw_filename = request_body.substr(filename_start_pos, filename_end_pos - filename_start_pos); // FIX for path traversal
                string filename = raw_filename;
                size_t last_slash = filename.find_last_of("/\\");
                if (last_slash != string::npos) {
                    filename = filename.substr(last_slash + 1);
                }
                // NOTE: parse file name and extension
                string video_name, extension;
                size_t extension_pos = filename.find_last_of(".");
                if(extension_pos != string::npos){
                    video_name = filename.substr(0, extension_pos);
                    extension = filename.substr(extension_pos);
                }
                else{
                   extension = ""; 
                }
                // NOTE: create unique tmp file path
                auto now = chrono::high_resolution_clock::now();
                long long timestamp = chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch()).count();
                string unique_id = "_" + to_string(getpid()) + "_" + to_string(timestamp);
                string tmp_save_path = "web/tmp/" + video_name + unique_id + extension;

                size_t data_start_pos = request_body.find("\r\n\r\n", filename_end_pos) + 4; // move past "\r\n\r\n"
                size_t data_end_pos = request_body.find(boundary, data_start_pos);
                string file_data = request_body.substr(data_start_pos, data_end_pos - data_start_pos - 2); // move back before \r\n
                ofstream ofs(tmp_save_path, ios::binary);
                if(ofs){
                    ofs.write(file_data.c_str(), file_data.size());
                    ofs.close();
                    string video_name = filename;
                    size_t dot_pos = video_name.find_last_of(".");
                    if(dot_pos != string::npos){
                        video_name = video_name.substr(0, dot_pos);
                    }
                    start_stash_conversation(tmp_save_path, video_name);
                    status_line = "HTTP/1.1 200 OK\r\n";
                    body = "<h1>Video Uploaded</h1>";
                }
                else{
                    status_line = "HTTP/1.1 500 Internal Server Error\r\n";
                    body = "<h1>Failed to save uploaded video file</h1>";
                }
                content_type = "Content-Type: text/html\r\n";
            }
        }
        else{
            // Add missing error response
            status_line = "HTTP/1.1 400 Bad Request\r\n";
            body = "<h1>Invalid multipart/form-data or missing boundary</h1>";
        }
        content_type = "Content-Type: text/html\r\n"; // Add this line before response construction
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
void sigchld_handler(int sig) {
    // Reap all dead child processes
    while (waitpid(-1, NULL, WNOHANG) > 0);
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
    // NOTE: prevent zombie processes
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        cerr << "Failed to set SIGCHLD handler: " << strerror(errno) << endl;
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
    // NOTE: check authtication
    if(load_secret("secret") == false){
        cerr << "Load secret file failed." << endl;
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
            if(errno == EINTR){
                continue;
            }
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
                    cout << "-------------END----------------" << endl;
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
                        string request_body;
                        if(clients[i].content_length > 0){
                            request_body = clients[i].read_buffer.substr(0, clients[i].content_length);
                            clients[i].read_buffer.erase(0, clients[i].content_length);
                        }
                        process_http_request(clients[i], request_body);
                        cout << "Processed HTTP request from fd " << fds[i].fd << endl <<  "method: " << clients[i].method << " path: " << clients[i].path << endl;
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
            system("rm -rf web/tmp/*");
        }
    }
    return 0;
}