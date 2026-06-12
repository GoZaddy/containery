// Filesystem functions
#pragma once

#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <cstring>
#include <nlohmann/json.hpp>

using namespace std;

using json = nlohmann::json;



void processLayer(const std::string& image_name, const std::string& bearer_token, std::string digest);