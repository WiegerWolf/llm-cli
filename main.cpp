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
#include "chat_client.h" // Include the ChatClient header
#include "cli_interface.h" // Include the CLI UI implementation header

using namespace std;

int main() {
    CliInterface cli_ui; // Instantiate the CLI UI
    try {
        cli_ui.initialize(); // Initialize the UI

        ChatClient client(cli_ui); // Inject the UI into the client
        client.run();

        cli_ui.shutdown(); // Shutdown the UI
        cout << "\nExiting...\n"; // Keep final exit message on cout for now
    } catch (const std::exception& e) {
        // If UI is available, use it for errors, otherwise fallback to cerr
        try {
             cli_ui.displayError("Fatal Error: " + std::string(e.what()));
             cli_ui.shutdown(); // Attempt graceful shutdown
        } catch (...) {
             cerr << "Fatal Error: " << e.what() << endl; // Fallback if UI fails
        }
        return 1;
    } catch (...) {
        // If UI is available, use it for errors, otherwise fallback to cerr
        try {
             cli_ui.displayError("Unknown Fatal Error.");
             cli_ui.shutdown(); // Attempt graceful shutdown
        } catch (...) {
             cerr << "Unknown Fatal Error." << endl; // Fallback if UI fails
        }
        return 1;
    }
    return 0; // Normal exit
}
