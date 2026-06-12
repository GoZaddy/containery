#include "fs.h"
#include "http.h"
#include "constants.h"

using namespace containerify;


void processLayer(const std::string& image_name, const std::string& bearer_token, std::string digest){
    std::string docker_registry_api_url = DOCKER_REGISTRY_URL + image_name;

    // Send a HEAD request to check if the layer blob exists
    auto head_response = makeHttpGetRequest(docker_registry_api_url+"/blobs/"+digest, {bearer_token}, true);
    cout << "head request code: " << head_response.code << endl;
    if (head_response.code == 200){
        cout << "image layer blob exists!" << endl;
    } else {
        cout << "image layer blob does not exist" << endl;
    }

    // Download the layer blob

    auto layer_download = makeHttpGetRequest(docker_registry_api_url+"/blobs/"+digest, {bearer_token}, false, "./"+digest.substr(digest.find(':') + 1) + ".tar.gz", true);
    cout << "head request code: " << head_response.code << endl;
    if (head_response.code == 200){
        cout << "image layer blob downloaded!" << endl;
    } else {
        cout << "image layer blob could not be downloaded" << endl;
    }

    // /var/lib/containerify
    //     /images 
    //         /ubuntu
    //             /26.04
    //                 /layers 
    //                     /hash_of_lower_dir_1
    //                         /..contents
    //                     /hash_of_lower_dir_2
    //                         /..contents
    //     /containers
    //         /container-1
    //             /upper-dir 
    //             /work-dir
    //             /merged 
                
    //         /container-2
    //             /upper-dir 
    //             /work-dir
    //             /merged 


}