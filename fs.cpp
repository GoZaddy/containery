#include "fs.h"
#include "http.h"
#include "constants.h"
#include <filesystem>
#include <archive.h>
#include <archive_entry.h>




void extractTarGz(const std::string& archivePath, const std::string& destDir) {
    struct archive* a = archive_read_new();
    archive_read_support_format_tar(a);
    archive_read_support_filter_gzip(a);

    struct archive* disk = archive_write_disk_new();
    archive_write_disk_set_options(disk, ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_TIME);

    archive_read_open_filename(a, archivePath.c_str(), 10240);

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        // prepend destination directory to each file's path
        std::string fullPath = destDir + "/" + archive_entry_pathname(entry);
        archive_entry_set_pathname(entry, fullPath.c_str());

        archive_write_header(disk, entry);
        
        // copy data from archive to disk
        const void* buff;
        size_t size;
        la_int64_t offset;
        while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
            archive_write_data_block(disk, buff, size, offset);
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(disk);
    archive_write_free(disk);
}


void processLayer(const ImageInfo& image_info, const std::string& bearer_token, std::string digest){
    std::string docker_registry_api_url = DOCKER_REGISTRY_URL + image_info.image_name;

    // Send a HEAD request to check if the layer blob exists
    cout << "layer url: " << docker_registry_api_url+"/blobs/"+digest << endl;
    auto head_response = makeHttpGetRequest(docker_registry_api_url+"/blobs/"+digest, {bearer_token}, true);
    cout << "head request code: " << head_response.code << endl;
    if (head_response.code == 200){
        cout << "image layer blob exists!" << endl;
    } else {
        cout << "image layer blob does not exist" << endl;
    }

    // Download the layer blob to /images directory and extract

    std::string digest_hash = digest.substr(digest.find(':') + 1);

    std::string image_layer_dir_path = "/tmp/containery/"+image_info.image_name+ "/";
    std::string image_layer_file_name = digest_hash+".tar.gz";

    // layer_file_path = "./test-layers/"+digest_hash; // for testing purposes, change this to layer_file_path when done

    std::filesystem::create_directories(image_layer_dir_path);

    // cout << "file: " << layer_file_path+tar_gz_file_extension << endl;
    auto layer_download = makeHttpGetRequest(docker_registry_api_url+"/blobs/"+digest, {bearer_token}, false, image_layer_dir_path+image_layer_file_name, true);
    cout << "head request code: " << head_response.code << endl;
    if (head_response.code == 200){
        cout << "image layer blob downloaded!" << endl;
    } else {
        cout << "image layer blob could not be downloaded" << endl;
    }

    extractTarGz(image_layer_dir_path+image_layer_file_name, IMAGES_DIR+image_info.image_name+"/"+image_info.version+"/layers/"+digest_hash);

    // TODO: set up container directories and overlayfs

     // /var/lib/containery
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

    // outside of this, create more namespaces


    // once namespaces are set up, run entrypoint command - will have 
    // to read manifest digest file (using blob endpoint i think) and exec that
}