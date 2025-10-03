#include "filesystem_utils.h"
#include <cstdlib>

namespace utils {

std::filesystem::path get_home_directory_path() {
    #ifdef _WIN32
        const char* userprofile = std::getenv("USERPROFILE");
        if (userprofile) {
            return std::filesystem::path(userprofile);
        }
        const char* homedrive = std::getenv("HOMEDRIVE");
        const char* homepath = std::getenv("HOMEPATH");
        if (homedrive && homepath) {
            return std::filesystem::path(homedrive) / homepath;
        }
    #else // POSIX-like systems
        const char* home_env = std::getenv("HOME");
        if (home_env) {
            return std::filesystem::path(home_env);
        }
    #endif
    return std::filesystem::path(); // Return default-constructed path
}

} // namespace utils