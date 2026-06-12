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
#include "http.h"
#include "fs.h"
#include "constants.h"
 
using namespace std;
using namespace containerify;


using json = nlohmann::json;



std::string getArchitecture() {
#if defined(__x86_64__) || defined(_M_X64)
    return "amd64";
#elif defined(__i386__) || defined(_M_IX86)
    return "i386";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64v8";
#elif defined(__arm__) || defined(_M_ARM)
    #if defined(__ARM_ARCH_7__)
        return "armv7";
    #else
        return "armv6";
    #endif
#elif defined(__riscv) && (__riscv_xlen == 64)
    return "riscv64";
#else
    return "unknown";
#endif
}






// Retrieve container image from docker hub
void retrieveDockerHubImage(const std::string& image_name){

    std::string docker_registry_api_url = DOCKER_REGISTRY_URL + image_name;
    CURL* curl = curl_easy_init();
    std::string response;


    // get bearer token
    std::string token = makeHttpGetRequest("https://auth.docker.io/token?service=registry.docker.io&scope=repository:" + image_name + ":pull").json_response["token"];


    // get image manifest
    const std::string manifest_url = docker_registry_api_url + "/manifests/latest";

    cout << manifest_url << endl;
    std::string bearer_token = "Authorization: Bearer " + token;
    json manifests_result = makeHttpGetRequest(manifest_url, {bearer_token, "Accept: application/vnd.docker.distribution.manifest.list.v2+json"}).json_response["manifests"];

    cout << "manifests: " << manifests_result << endl;

    std::string arch = getArchitecture();

    cout << "arch: " << arch << endl;

    std::string platform_specific_digest;

    for(int i = 0; i < manifests_result.size(); ++i){
        json curr = manifests_result[i];
        std::string curr_arch = curr["platform"]["architecture"];
        
        if (curr["platform"]["variant"] != nullptr){
            curr_arch += curr["platform"]["variant"];
        }

        if (arch == curr_arch){
            cout << "arch found!" << endl;

            platform_specific_digest = curr["digest"];
            break;
        }
    }

    // get platform-specific manifest

    json ps_manifest = makeHttpGetRequest(docker_registry_api_url + "/manifests/"+platform_specific_digest, {bearer_token}).json_response;

    for (int i = 0; i < ps_manifest["layers"].size(); ++i){
        cout << "processing layer " << i+1 << endl;
        processLayer(image_name, bearer_token, ps_manifest["layers"][i]["digest"]);
    }

}

pid_t clone3_syscall(struct clone_args *args){
    return (pid_t) syscall(SYS_clone3, args, sizeof(*args));
}

void startContainer(){
    cout << "Starting container..." << endl;

    struct clone_args args{};
    args.flags = CLONE_NEWPID | CLONE_NEWUTS;
    args.exit_signal = SIGCHLD;

    // create new process with new pid namespace

    pid_t pid = clone3_syscall(&args);

    if (pid == -1) {
        cerr << "Failed to create container: " << strerror(errno) << endl;
        return;
    } else if (pid == 0) {
        // child/container process
        const char* hostname = "mycontainer";
        int result = sethostname(hostname, strlen(hostname));

        if (result == 0) {
            std::cout << "Successfully changed hostname to: " << hostname << std::endl;
        } else {
            std::cerr << "Failed to set hostname. Error: " << std::strerror(errno) << std::endl;
            std::cerr << "Are you running this program as root?" << std::endl;
        }
        cout << "Container started with PID: " << getpid() << endl;
    } else {
        // parent process
        cout << "Container created with PID: " << pid << endl;
        
    }


    char hostname[HOST_NAME_MAX + 1]; 
    
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::cout << "GetHostname: " << hostname << std::endl;
    } else {
        std::perror("gethostname failed");
        return;
    }

    if (pid != 0) {
        // wait on container process to finish
        waitpid(pid, NULL, 0);
    }

    

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Invalid usage. See --help for help" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    if (command == "--help") {
        cout << "Usage: " << argv[0] << " run <image_name>" << endl;
        cout << "Example: " << argv[0] << " run ubuntu:latest" << endl;
        return 0;
    } else if (command == "run") {
        if (argc < 3){
            cerr << "Usage: " << argv[0] << "run <image_name>" << endl;
            return 1;
        }
        const std::string image_name = argv[2];
        retrieveDockerHubImage(image_name);
        startContainer();
        return 0;
    } else {
        cerr << "Unknown command: " << command << ". See --help for help" << std::endl;
        return 1;
    }
}
