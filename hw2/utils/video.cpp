#include "video.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cerrno>
#include <string>

using namespace std;

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

