#include <iostream>
#include <string>
#include <curl/curl.h>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>
#include <string>
#include <cstdlib> // For getenv
#include <stdexcept> // For runtime_error
#include "chat_client.h" // Include the new header

// Removed includes that are now in chat_client.cpp or other headers
// #include <curl/curl.h>
// #include <nlohmann/json.hpp>
// #include <sstream>
// #include <iomanip>
// #include <readline/readline.h>
// #include <readline/history.h>
// #include "database.h"
// #include "tools.h" 

using namespace std;

// WriteCallback moved to chat_client.cpp
// ChatClient class definition and methods moved to chat_client.h/cpp

int main() {
    try {
        ChatClient client;
        client.run();
        cout << "\nExiting...\n";
    } catch (const std::exception& e) {
        cerr << "Fatal Error: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown Fatal Error." << endl;
        return 1;
    }
    return 0;
}
