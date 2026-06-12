#pragma once

#include <iostream>
#include <sys/syscall.h>
#include <linux/sched.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <climits>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;

using json = nlohmann::json;

size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data);


typedef struct httpResponse {
    json json_response;
    int code;
} HttpResponse;

// TODO: implement better error handling - 404s etc
HttpResponse makeHttpGetRequest(
    const string& url, 
    vector<std::string> header_lines = {}, 
    bool is_head_request = false, 
    std::string file_output = "",
    bool follow_location = false
);