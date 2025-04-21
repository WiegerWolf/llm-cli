#include "tools_impl/datetime_tool.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

std::string get_current_datetime() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    std::tm now_tm;
#ifdef _WIN32
    localtime_s(&now_tm, &now_c);
#else
    localtime_r(&now_c, &now_tm);
#endif
    ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S %Z");
    return ss.str();
}
