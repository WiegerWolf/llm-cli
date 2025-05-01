# Refactoring Stage 1: Define the UI Abstraction                                                                                                                         
                                                                                                                                                                         
**Goal:** Create the abstract base class `UserInterface` that defines the contract between the core application logic and the user interface.                            
                                                                                                                                                                         
**Checklist:**                                                                                                                                                           
                                                                                                                                                                         
1.  [X] **Create `ui_interface.h`:**                                                                                                                                     
    *   Add standard include guards (`#pragma once` or `#ifndef`/`#define`/`#endif`).                                                                                    
    *   Include necessary standard headers (`<string>`, `<optional>`).                                                                                                   
2.  [X] **Define `UserInterface` Class:**                                                                                                                                
    *   Declare `class UserInterface`.                                                                                                                                   
3.  [X] **Declare Pure Virtual Methods:**                                                                                                                                
    *   `virtual std::optional<std::string> promptUserInput() = 0;`                                                                                                      
    *   `virtual void displayOutput(const std::string& output) = 0;`                                                                                                     
    *   `virtual void displayError(const std::string& error) = 0;`                                                                                                       
    *   `virtual void displayStatus(const std::string& status) = 0;`                                                                                                     
    *   `virtual void initialize() = 0;`                                                                                                                                 
    *   `virtual void shutdown() = 0;`                                                                                                                                   
4.  [X] **Declare Virtual Destructor:**                                                                                                                                  
    *   `virtual ~UserInterface() = default;`                                                                                                                            
5.  [X] **Review:** Ensure the header file compiles and the interface definition matches the plan.                                                                       
