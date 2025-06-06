#include <iostream> // Keep for cerr fallback
#include <string>
#include <stdexcept> // For runtime_error
#include "chat_client.h" // Include the ChatClient header
#include "cli_interface.h" // Include the CLI UI implementation header
#include "database.h"    // Include the PersistenceManager header
#include <stop_token>      // Include for std::stop_token

// Use std namespace explicitly to avoid potential conflicts
using std::cerr;
using std::endl;
using std::string;

int main() {
    CliInterface cli_ui; // Instantiate the CLI UI
    PersistenceManager db_manager; // Instantiate the DB manager
    try {
        cli_ui.initialize(); // Initialize the UI

        ChatClient client(cli_ui, db_manager); // Inject the UI and DB manager into the client
        client.initialize_model_manager(); // Attempt to initialize models, fetch from API, or use default
        // Pass a default (non-stoppable) stop_token as the CLI uses Ctrl+D for exit
        client.run();

        cli_ui.shutdown(); // Shutdown the UI
        cli_ui.displayOutput("\nExiting...\n", ""); // Use UI for final message
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
