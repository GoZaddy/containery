// Filesystem functions
#pragma once

#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <cstring>
#include <nlohmann/json.hpp>
#include "types.h"

using namespace std;
using namespace containery;

using json = nlohmann::json;



void processLayer(const ImageInfo& image_info, const std::string& bearer_token, std::string digest);