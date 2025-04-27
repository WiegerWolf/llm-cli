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

using namespace std;

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
