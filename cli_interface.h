#pragma once                                                                                                                                                             
                                                                                                                                                                         
#include "ui_interface.h" // Include the abstract base class definition                                                                                                  
#include <string>                                                                                                                                                        
#include <optional>                                                                                                                                                      
                                                                                                                                                                         
// Concrete implementation of UserInterface for a command-line environment.                                                                                              
class CliInterface : public UserInterface {                                                                                                                              
public:                                                                                                                                                                  
    CliInterface() = default;                                                                                                                                            
    virtual ~CliInterface() override = default; // Use override for virtual destructor                                                                                   
                                                                                                                                                                         
    // Implementation of the UserInterface contract                                                                                                                      
    virtual std::optional<std::string> promptUserInput() override;                                                                                                       
    virtual void displayOutput(const std::string& output) override;                                                                                                      
    virtual void displayError(const std::string& error) override;                                                                                                        
    virtual void displayStatus(const std::string& status) override;                                                                                                      
    virtual void initialize() override;                                                                                                                                  
    virtual void shutdown() override;                                                                                                                                    
};          