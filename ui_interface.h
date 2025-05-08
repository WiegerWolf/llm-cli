#pragma once                                                                                                                                                             
                                                                                                                                                                         
#include <string>                                                                                                                                                        
#include <optional>
#include <vector> // For ModelData
#include "model_types.h" // For ModelData

// Abstract base class defining the contract for user interaction.
// This allows the core application logic to be decoupled from the specific UI implementation (e.g., CLI, GUI).
class UserInterface {
public:
    // Prompts the user for input.
    // Returns std::nullopt if the user signals end-of-input (e.g., Ctrl+D).
    virtual std::optional<std::string> promptUserInput() = 0;

    // Displays standard output to the user.
    virtual void displayOutput(const std::string& output, const std::string& model_id) = 0;

    // Displays error messages to the user.
    virtual void displayError(const std::string& error) = 0;

    // Displays status messages to the user (e.g., tool execution progress).
    virtual void displayStatus(const std::string& status) = 0;

    // Performs any necessary initialization for the UI.
    virtual void initialize() = 0;

    // Performs any necessary cleanup for the UI.
    virtual void shutdown() = 0;

    // Returns true if the UI is a graphical interface, false otherwise.
    virtual bool isGuiMode() const = 0;

    // --- Methods for Model Loading UI Feedback (Part V) ---
    // Called by ChatClient to inform the UI about the model loading state.
    virtual void setLoadingModelsState(bool isLoading) = 0;

    // Called by ChatClient to provide the UI with the list of available models.
    // This is particularly for the GUI to update its model selection dropdown.
    virtual void updateModelsList(const std::vector<ModelData>& models) = 0;
    // --- End Methods for Model Loading UI Feedback ---

    // Virtual destructor to ensure proper cleanup of derived classes.
    virtual ~UserInterface() = default;
};