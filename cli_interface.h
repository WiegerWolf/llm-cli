#pragma once                                                                                                                                                             
                                                                                                                                                                         
#include "ui_interface.h" // Include the abstract base class definition
#include <string>
#include <optional>
#include <vector> // Required for ModelData
#include "model_types.h" // Required for ModelData

// Concrete implementation of UserInterface for a command-line environment.
class CliInterface : public UserInterface {
public:
    CliInterface() = default;
    virtual ~CliInterface() override = default; // Use override for virtual destructor

    // Implementation of the UserInterface contract
    virtual std::optional<std::string> promptUserInput() override;
    virtual void displayOutput(const std::string& output, const std::string& model_id) override;
    virtual void displayError(const std::string& error) override;
    virtual void displayStatus(const std::string& status) override;
    virtual void initialize() override;
    virtual void shutdown() override;
    virtual bool isGuiMode() const override;

    // --- Implementation for Model Loading UI Feedback (Part V) ---
    virtual void setLoadingModelsState(bool isLoading) override;
    virtual void updateModelsList(const std::vector<ModelData>& models) override;
    // --- End Implementation for Model Loading UI Feedback ---
};