#include "request_handler.h"
#include "auth.h"
#include "http_utils.h"
#include "responses.h"
#include "video.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>

using namespace std;

void process_http_request(ClientState &client, string request_body){
    // NOTE: routing path
    string status_line;
    string content_type;
    string body;
    string response;

    // NOTE: Define flags for the response's stage
    bool endpoint_defined = false;
    bool method_allowed = false;
    bool auth_required = false;
    bool resource_exists = false;
    bool everything_ok = false;
    if(client.path == "/"){
        endpoint_defined = true;
        if(client.method == "GET"){
            method_allowed = true;
            auth_required = false;
            ifstream ifs("web/index.html");
            if(ifs){
                stringstream ss;
                ss << ifs.rdbuf();
                body = ss.str();
                resource_exists = true;
            }
            else{
                body = "<h1>Index file not found</h1>";
                resource_exists = true;
            }
            everything_ok = true;
            status_line = "HTTP/1.1 200 OK\r\n";
            content_type = "Content-Type: text/html\r\n";
        }
    }
    else if(client.path == "/upload/file"){
        endpoint_defined = true;
        if(client.method == "GET"){
            method_allowed = true;
            auth_required = true;
            if(authenticate_user(client) == false){
                client.write_buffer = create_401_response();
                client.bytes_sent = 0;
                return;
            }
            ifstream ifs("web/uploadf.html");
            if(ifs){
                stringstream ss;
                ss << ifs.rdbuf();
                body = ss.str();
                resource_exists = true;
                everything_ok = true;
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "Content-Type: text/html\r\n";
            }
            else{
                resource_exists = false;
            }
        }
    }
    else if(client.path == "/upload/video"){
        endpoint_defined = true;
        if(client.method == "GET"){
            method_allowed = true;
            auth_required = true;
            if(authenticate_user(client) == false){
                client.write_buffer = create_401_response();
                client.bytes_sent = 0;
                return;
            }
            ifstream ifs("web/uploadv.html");
            if(ifs){
                stringstream ss;
                ss << ifs.rdbuf();
                body = ss.str();
                resource_exists = true;
                everything_ok = true;
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "Content-Type: text/html\r\n";
            }
            else{
                resource_exists = false;
            }
        }
    }
    else if(client.path == "/file/"){
        endpoint_defined = true;
        if(client.method == "GET"){
            method_allowed = true;
            auth_required = false;
            ifstream ifs("web/listf.rhtml");
            if(ifs.is_open()){
                resource_exists = true;
                // NOTE: read template content
                stringstream body_ss;
                body_ss << ifs.rdbuf();
                string template_content = body_ss.str();
                string file_list_html;
                DIR *dir;
                struct dirent *entry;
                if ((dir = opendir("web/files/")) != NULL) {
                    while ((entry = readdir(dir)) != NULL) {
                        string filename = entry->d_name;
                        if (filename == "." || filename == "..") continue;
                        
                        string encoded_filename = url_encode(filename);
                        file_list_html += "<tr><td><a href=\"/api/file/"; 
                        file_list_html += encoded_filename;
                        file_list_html += "\">";
                        file_list_html += filename;
                        file_list_html += "</a></td></tr>\n"; 
                    }
                    closedir(dir);
                }

                replace_first(template_content, "<?FILE_LIST?>", file_list_html); 
                
                body = template_content;
                everything_ok = true;
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "Content-Type: text/html\r\n";
            }
            else{
                resource_exists = false;
            }
        }
    }
    else if(client.path.rfind("/video/", 0) == 0 && client.path.length() > 0 && client.path.back() != '/'){
        endpoint_defined = true;
        if(client.method == "GET"){
            method_allowed = true;
            auth_required = false;
            string encoded_videoname = client.path.substr(7); // length of "/video/" is 7
            string videoname = url_decode(encoded_videoname);

            ifstream ifs("web/player.rhtml");
            if(ifs.is_open()){
                resource_exists = true;
                stringstream ss;
                ss << ifs.rdbuf();
                // NOTE: replace tag in html
                string template_content = ss.str();
                string encoded_mpd_videoname = url_encode(videoname);
                string mpd_path = "'/api/video/" + encoded_mpd_videoname + "/dash.mpd'";
                replace_first(template_content, "<?VIDEO_NAME?>", videoname);
                replace_first(template_content, "<?MPD_PATH?>", mpd_path);

                body = template_content;
                everything_ok = true;
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "Content-Type: text/html\r\n";
            }
            else{
                resource_exists = false;
            }
        }
    }
    else if(client.path == "/video/"){
        endpoint_defined = true;
        if(client.method == "GET"){
            method_allowed = true;
            auth_required = false;
            ifstream ifs("web/listv.rhtml");
            if(ifs.is_open()){
                resource_exists = true;
                stringstream body_ss;
                body_ss << ifs.rdbuf();
                string template_content = body_ss.str();
                
                string video_list_html;
                DIR *dir;
                struct dirent *entry;
                
                if ((dir = opendir("web/videos/")) != NULL) {
                    while ((entry = readdir(dir)) != NULL) {
                        // 
                        struct stat st;
                        string full_path = "web/videos/" + string(entry->d_name);
                        if (stat(full_path.c_str(), &st) != 0) continue;

                        string dirname = entry->d_name;
                        if (dirname == "." || dirname == "..") continue;
                        
                        if (S_ISDIR(st.st_mode)) { 
                            video_list_html += "<tr><td><a href=\"/video/"; 
                            video_list_html += dirname;
                            video_list_html += "\">";
                            video_list_html += dirname;
                            video_list_html += "</a></td></tr>\n"; 
                        }
                    }
                    closedir(dir);
                }

                replace_first(template_content, "<?VIDEO_LIST?>", video_list_html); 
                body = template_content;
                everything_ok = true;
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "text/html";
            }
            else{
                resource_exists = false;
            }
        }
    }
    else if(client.path.rfind("/api/file/", 0) == 0){
        endpoint_defined = true;
        if(client.method == "GET"){
            method_allowed = true;
            auth_required = true;
            if(authenticate_user(client) == false){
                client.write_buffer = create_401_response();
                client.bytes_sent = 0;
                return;
            }
            // NOTE: get safe filename
            string encoded_filename = client.path.substr(10); // length of "/api/file/" is 10
            string filename = url_decode(encoded_filename);
            string file_path = "web/files/" + filename;
            ifstream ifs(file_path, ios::binary);
            if(ifs){
                stringstream ss;
                ss << ifs.rdbuf();
                body = ss.str();
                resource_exists = true;
                everything_ok = true;
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "Content-Type: " + get_MIME_type(filename) + "\r\n";
            }
            else{
                resource_exists = false;
            }
        }
    }
    else if(client.path.rfind("/api/video/", 0) == 0){
        endpoint_defined = true;
        if(client.method == "GET"){
            method_allowed = true;
            auth_required = true;
            if(authenticate_user(client) == false){
                client.write_buffer = create_401_response();
                client.bytes_sent = 0;
                return;
            }
            // NOTE: get safe video name
            string encoded_filename = client.path.substr(11); // length of "/api/video/" is 11
            string filename = url_decode(encoded_filename);
            string file_path = "web/videos/" + filename;
            cout << "filepath: " << file_path << endl;
            ifstream ifs(file_path, ios::binary);
            if(ifs){
                stringstream ss;
                ss << ifs.rdbuf();
                body = ss.str();
                resource_exists = true;
                everything_ok = true;
                status_line = "HTTP/1.1 200 OK\r\n";
                content_type = "Content-Type: " + get_MIME_type(filename) + "\r\n";
            }
            else{
                resource_exists = false;
            }
        }
    }
    else if(client.path == "/api/file"){
        endpoint_defined = true;
        if(client.method == "POST"){
            method_allowed = true;
            auth_required = true;
            if(authenticate_user(client) == false){
                client.write_buffer = create_401_response();
                client.bytes_sent = 0;
                return;
            }
            cout << "Authorized upload request " << "for fd " << client.fd << endl;
            // NOTE: parse upload file
            string boundary;
            string content_type_header = client.headers["content-type"];
            size_t boundary_pos = content_type_header.find("boundary=");
            if(content_type_header.rfind("multipart/form-data") != string::npos && boundary_pos != string::npos){
                resource_exists = true;
                boundary = content_type_header.substr(boundary_pos + 9); 
                // NOTE: extract file content between boundaries
                size_t filename_pos = request_body.find("filename=\"");
                size_t filename_end_pos = 0;
                if(filename_pos == string::npos){
                    status_line = "HTTP/1.1 400 Bad Request\r\n";
                    body = "<h1>No filename in multipart/form-data</h1>";
                    content_type = "Content-Type: text/html\r\n";
                    everything_ok = false;
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
                                everything_ok = true;
                                status_line = "HTTP/1.1 200 OK\r\n";
                                body = "File Uploaded\n";
                                content_type = "Content-Type: text/plain\r\n";
                            }
                            else{
                                everything_ok = false;
                            }
                        }
                        else{                        
                            status_line = "HTTP/1.1 400 Bad Request\r\n";
                            body = "<h1>Invalid multipart/form-data format</h1>";
                            content_type = "Content-Type: text/html\r\n";
                            everything_ok = false;
                        }
                    }
                    else{
                        status_line = "HTTP/1.1 400 Bad Request\r\n";
                        body = "<h1>Invalid multipart/form-data format</h1>";
                        content_type = "Content-Type: text/html\r\n";
                        everything_ok = false;
                    }
                }
            }
            else{
                resource_exists = false;
            }
            // NOTE: content_type is set above for success case, or here for error cases
            if(content_type.empty()){
                content_type = "Content-Type: text/html\r\n";
            }
        }
    }
    else if(client.path == "/api/video"){
        endpoint_defined = true;
        if(client.method == "POST"){
            method_allowed = true;
            auth_required = true;
            if(authenticate_user(client) == false){
                client.write_buffer = create_401_response();
                client.bytes_sent = 0;
                return;
            }
            cout << "Authorized video upload request " << "for fd " << client.fd << endl;
            // NOTE: parse upload video
            string boundary;
            string content_type_header = client.headers["content-type"];
            size_t boundary_pos = content_type_header.find("boundary=");
            if(content_type_header.rfind("multipart/form-data") != string::npos && boundary_pos != string::npos){
                resource_exists = true;
                boundary = content_type_header.substr(boundary_pos + 9);
                size_t filename_start_pos = request_body.find("filename=\"");
                if(filename_start_pos == string::npos){
                    status_line = "HTTP/1.1 400 Bad Request\r\n";
                    body = "<h1>No filename in multipart/form-data</h1>";
                    content_type = "Content-Type: text/html\r\n";
                    everything_ok = false;
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
                        everything_ok = true;
                        status_line = "HTTP/1.1 200 OK\r\n";
                        body = "Video Uploaded\n";
                        content_type = "Content-Type: text/plain\r\n";
                    }
                    else{
                        everything_ok = false;
                    }
                }
            }
            else{
                resource_exists = false;
            }
            // NOTE: content_type is set above for success case, or here for error cases
            if(content_type.empty()){
                content_type = "Content-Type: text/html\r\n";
            }
        }
    }
    
    if(!endpoint_defined){
        client.write_buffer = create_404_response();
        client.bytes_sent = 0;
        return;
    }
    
    if(!method_allowed){
        client.write_buffer = create_405_response();
        client.bytes_sent = 0;
        return;
    }
    
    if(!resource_exists){
        client.write_buffer = create_404_response();
        client.bytes_sent = 0;
        return;
    }
    
    if(!everything_ok){
        if(status_line.empty()){
            client.write_buffer = create_500_response();
            client.bytes_sent = 0;
            return;
        }
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

