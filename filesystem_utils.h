#ifndef FILESYSTEM_UTILS_H
#define FILESYSTEM_UTILS_H

#include <filesystem>

namespace utils {
    // Get the user's home directory path (cross-platform)
    // Returns an empty path if the home directory cannot be determined
    std::filesystem::path get_home_directory_path();
}

#endif // FILESYSTEM_UTILS_H