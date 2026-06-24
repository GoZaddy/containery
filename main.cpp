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
#include "types.h"
#include <sys/mount.h>
#include <filesystem>
 
using namespace std;
using namespace containery;


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
int retrieveDockerHubImage(const std::string& image_name, const std::string& container_name){

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

    cout << "token: " << token << endl;

    std::string arch = getArchitecture();

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

    cout << "platform specific manifest: " << ps_manifest.dump(4) << endl;

    ImageInfo image_info;
    image_info.image_name = image_name;
    image_info.version = ps_manifest["annotations"]["org.opencontainers.image.version"];

    if (!std::filesystem::is_directory(IMAGES_DIR + image_info.image_name + "/" + image_info.version + "/layers")) {
        for (int i = 0; i < ps_manifest["layers"].size(); ++i){
            cout << "processing layer " << i+1 << endl;
            processLayer(image_info, bearer_token, ps_manifest["layers"][i]["digest"]);
        }
    } else {
        cout << "Image already exists in local storage. Skipping download and extraction." << endl;
    }
    
    // create container filesystem
    if (std::filesystem::is_directory(CONTAINERS_DIR + container_name)){
        cerr << "Container with name " << container_name << " already exists. Please choose a different name." << endl;
        return -1;
    }

    // create directories for overlayfs
    string upper_dir = CONTAINERS_DIR + container_name + "/upper";
    string work_dir = CONTAINERS_DIR + container_name + "/work";
    string merged_dir = CONTAINERS_DIR + container_name + "/merged";

    std::filesystem::create_directories(upper_dir);
    std::filesystem::create_directories(work_dir);
    std::filesystem::create_directories(merged_dir);

    string lower_dir_options;

    for (int i = ps_manifest["layers"].size() - 1; i >= 0; --i){
        string digest = ps_manifest["layers"][i]["digest"];
        string digest_hash = digest.substr(digest.find(':') + 1);
        lower_dir_options += IMAGES_DIR + image_info.image_name + "/" + image_info.version + "/layers/" + digest_hash;
        if (i != 0){
            lower_dir_options += ":";
        }
    }


    string options = "lowerdir=" + lower_dir_options + ",upperdir=" + upper_dir + ",workdir=" + work_dir;

    mount("overlay", merged_dir.c_str(), "overlay", 0, options.c_str());

    return 0;
}

pid_t clone3_syscall(struct clone_args *args){
    return (pid_t) syscall(SYS_clone3, args, sizeof(*args));
}

void startContainer(const std::string& container_name) {
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
        string merged_dir = CONTAINERS_DIR + container_name + "/merged";
        string put_old_dir = merged_dir + "/put_old";
       
        mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL);

        std::filesystem::create_directories(put_old_dir);
        syscall(SYS_pivot_root, merged_dir.c_str(), put_old_dir.c_str());
        umount2("/put_old", MNT_DETACH);
        rmdir("/put_old");

        // test that filesystem isolation works
        char* argv[] = { (char*)"cat", (char*)"/etc/os-release", nullptr };
        execvp("cat", argv);

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
        cout << "Usage: " << argv[0] << " run <image_name> <container_name>" << endl;
        cout << "Example: " << argv[0] << " run ubuntu:latest mycontainer" << endl;
        return 0;
    } else if (command == "run") {
        if (argc < 4){
            cerr << "Usage: " << argv[0] << "run <image_name> <container_name>" << endl;
            return 1;
        }
        const std::string image_name = argv[2];
        const std::string container_name = argv[3];
        if (retrieveDockerHubImage(image_name, container_name) != -1) {
            startContainer(container_name);
        }
        

        
        return 0;
    } else {
        cerr << "Unknown command: " << command << ". See --help for help" << std::endl;
        return 1;
    }
}
