#include "cli_interface.h"                                                                                                                                               
#include <iostream>                                                                                                                                                      
#include <readline/readline.h>                                                                                                                                           
#include <readline/history.h>                                                                                                                                            
#include <cstdlib> // For free()                                                                                                                                         
                                                                                                                                                                         
// Performs any necessary initialization for the CLI.                                                                                                                    
// Currently, just prints a welcome message. Could load history here later.                                                                                              
void CliInterface::initialize() {                                                                                                                                        
    // std::cout << "CLI Initialized. Welcome!\n"; // Optional: Add welcome message if desired                                                                           
    // rl_initialize(); // Readline initializes itself implicitly on first call usually                                                                                  
}                                                                                                                                                                        
                                                                                                                                                                         
// Performs any necessary cleanup for the CLI.                                                                                                                           
// Currently, just prints a shutdown message. Could save history here later.                                                                                             
void CliInterface::shutdown() {                                                                                                                                          
    // std::cout << "\nCLI Shutting down...\n"; // Optional: Add shutdown message                                                                                        
    // clear_history(); // Example: Clear history on exit if desired                                                                                                     
}                                                                                                                                                                        
                                                                                                                                                                         
// Prompts the user for input using readline.                                                                                                                            
// Handles Ctrl+D (returns nullopt) and adds valid input to history.                                                                                                     
std::optional<std::string> CliInterface::promptUserInput() {                                                                                                             
    char* input_cstr = readline("> "); // Use the classic prompt symbol                                                                                                  
    if (!input_cstr) {                                                                                                                                                   
        return std::nullopt; // User pressed Ctrl+D or similar EOF signal                                                                                                
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
void CliInterface::displayOutput(const std::string& output) {                                                                                                            
    std::cout << output;                                                                                                                                                 
    // Add newline if output doesn't already end with one                                                                                                                
    if (output.empty() || output.back() != '\n') {                                                                                                                       
        std::cout << '\n';                                                                                                                                               
    }                                                                                                                                                                    
    std::cout.flush(); // Ensure output is displayed immediately                                                                                                         
}                                                                                                                                                                        
                                                                                                                                                                         
// Displays error messages to the console (stderr).                                                                                                                      
// Ensures output ends with a newline.                                                                                                                                   
void CliInterface::displayError(const std::string& error) {                                                                                                              
    std::cerr << error;                                                                                                                                                  
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