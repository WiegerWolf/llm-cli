#include "cli_interface.h"                                                                                                                                               
#include <iostream>                                                                                                                                                      
#include <readline/readline.h>                                                                                                                                           
#include <readline/history.h>                                                                                                                                            
#include <cstdlib> // For free()                                                                                                                                         
                                                                                                                                                                         
// Performs any necessary initialization for the CLI.
// (Currently no specific initialization steps are required).
void CliInterface::initialize() {
    // Readline library initializes itself implicitly on first use.
    // History loading could be added here if desired.
}

// Performs any necessary cleanup for the CLI.
// (Currently no specific shutdown steps are required).
void CliInterface::shutdown() {
    // History saving could be added here if desired.
}

// Prompts the user for input using the readline library.
// Readline provides line editing, history (up/down arrows), and completion capabilities.
// Handles Ctrl+D (returns nullopt) and adds valid input to history.
std::optional<std::string> CliInterface::promptUserInput() {
    // Display the prompt "> " and read a line of input.
    char* input_cstr = readline("> ");

    // Check if readline returned nullptr (indicates EOF, typically Ctrl+D).
    if (!input_cstr) {
        std::cout << std::endl; // Print a newline after Ctrl+D for cleaner terminal output
        return std::nullopt; // Signal to the caller to exit.
    }                                                                                                                                                                    
    std::string input(input_cstr);                                                                                                                                       
    free(input_cstr); // Free memory allocated by readline                                                                                                               
                                                                                                                                                                                                        
    // Add non-empty input to history                                                                                                                                    
    if (!input.empty()) {                                                                                                                                                
        add_history(input.c_str());                                                                                                                                      
    }                                                                                                                                                                    
    return input;                                                                                                                                                        
}                                                                                                                                                                        
                                                                                                                                                                         
// Displays standard output to the console (stdout).                                                                                                                     
// Ensures output ends with a newline for proper formatting.                                                                                                             
void CliInterface::displayOutput(const std::string& output, const std::string& model_id) {
    // model_id is not currently used in CLI display but is part of the interface
    (void)model_id; // Mark as unused to prevent compiler warnings
    std::cout << output;
    // Add newline if output doesn't already end with one
    if (output.empty() || output.back() != '\n') {
        std::cout << '\n';
    }
    std::cout.flush(); // Ensure output is displayed immediately
}
                                                                                                                                                                         
// Displays error messages to the console (stderr).                                                                                                                      
// Prefixes with "Error: " and ensures output ends with a newline.                                                                                                       
void CliInterface::displayError(const std::string& error) {                                                                                                              
    std::cerr << "Error: " << error; // Add "Error: " prefix                                                                                                             
    // Add newline if error doesn't already end with one                                                                                                                 
    if (error.empty() || error.back() != '\n') {                                                                                                                         
        std::cerr << '\n';                                                                                                                                               
    }                                                                                                                                                                    
    std::cerr.flush(); // Ensure error is displayed immediately                                                                                                          
}                                          
 // Displays status messages to the console (stdout).                                                                                                                     
// Prefixes with "[Status]" for clarity and ensures a newline.                                                                                                           
void CliInterface::displayStatus(const std::string& status) {                                                                                                            
    std::cout << "[Status] " << status;                                                                                                                                  
    // Add newline if status doesn't already end with one                                                                                                                
    if (status.empty() || status.back() != '\n') {                                                                                                                       
        std::cout << '\n';                                                                                                                                               
    }                                                                                                                                                                    
    std::cout.flush(); // Ensure status is displayed immediately                                                                                                         
}       

// Implementation for isGuiMode - CLI is never GUI mode.
bool CliInterface::isGuiMode() const {
    return false;
}
