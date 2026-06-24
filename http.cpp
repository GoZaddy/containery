#include "http.h"
#include "constants.h"

using namespace containery;



size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}



// TODO: implement better error handling - 404s etc
HttpResponse makeHttpGetRequest(
    const string& url, 
    vector<std::string> header_lines, 
    bool is_head_request, 
    std::string file_output,
    bool follow_location
){
    CURL* curl = curl_easy_init();
    std::string response;

    struct curl_slist* headers = nullptr;
    if (header_lines.size() > 0){
        for (const auto& header_line: header_lines){
            headers = curl_slist_append(headers, header_line.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (is_head_request){
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    

    FILE* file = nullptr;
    if (file_output == ""){
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,  &response);
    } else {
        file = fopen(file_output.c_str(), "wb"); 
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,  file);
    }

    if (follow_location){
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    }
    

    CURLcode res = curl_easy_perform(curl);

    int http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // cleanup 
    curl_easy_cleanup(curl);
    if (headers != nullptr){
        curl_slist_free_all(headers);
    }
    if (file != nullptr){
        fclose(file);
    }

    json json_response = nullptr;

    if (!is_head_request && file_output == ""){
        json_response = json::parse(response);
    }

    

    return HttpResponse{
        .json_response = json_response,
        .code = http_code
    };
}