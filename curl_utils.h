#pragma once

#include <string>
#include <cstddef> // For size_t

// Standard CURL write callback function to append data to a std::string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append(static_cast<char*>(contents), total_size);
    return total_size;
}
