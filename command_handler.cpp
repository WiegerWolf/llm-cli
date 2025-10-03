#include "command_handler.h"
#include "model_manager.h"
#include <stdexcept>

CommandHandler::CommandHandler(UserInterface& ui_ref,
                               PersistenceManager& db_ref,
                               ModelManager& model_manager_ref)
    : ui(ui_ref), db(db_ref), modelManager(model_manager_ref) {
}

bool CommandHandler::handleCommand(const std::string& input) {
    // Parse command - get first token (up to first space)
    size_t space_pos = input.find(' ');
    std::string command = (space_pos != std::string::npos) 
        ? input.substr(0, space_pos) 
        : input;
    
    // Route to appropriate handler
    if (command == "/models") {
        handleModelsCommand();
        return true;
    } else if (command == "/model") {
        if (space_pos == std::string::npos) {
            ui.displayError("Usage: /model <model-id>. Use /models to see available models.");
            return true;
        }
        
        // Extract and trim model ID
        std::string model_id = input.substr(space_pos + 1);
        size_t start = model_id.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            ui.displayError("Usage: /model <model-id>. Use /models to see available models.");
            return true;
        }
        model_id = model_id.substr(start);
        
        size_t end = model_id.find_last_not_of(" \t\n\r");
        if (end != std::string::npos) {
            model_id = model_id.substr(0, end + 1);
        }
        
        if (model_id.empty()) {
            ui.displayError("Usage: /model <model-id>. Use /models to see available models.");
            return true;
        }
        
        handleModelCommand(model_id);
        return true;
    } else {
        // Unknown command
        ui.displayOutput("\nUnknown command. Available commands:\n"
                        "  /models - List all available models\n"
                        "  /model <model-id> - Change the active model\n", "");
        return true;
    }
}

void CommandHandler::handleModelsCommand() {
    try {
        std::vector<ModelData> models = db.getAllModels();
        
        if (models.empty()) {
            ui.displayError("No models available. Models may still be loading.");
            return;
        }
        
        // Build output string
        std::string output = "\nAvailable Models";
        
        // Add current model info to header
        std::string current_model_id = modelManager.getActiveModelId();
        try {
            auto current_model = db.getModelById(current_model_id);
            if (current_model.has_value()) {
                output += " (current: " + current_model->name + ")";
            }
        } catch (...) {
            // Ignore errors getting current model name
        }
        output += ":\n\n";
        
        // List all models with status indicator
        for (const auto& model : models) {
            // Mark current model with [*]
            if (model.id == current_model_id) {
                output += "  [*] ";
            } else {
                output += "      ";
            }
            
            // Format: Name (ID) - Context: XXXX tokens
            output += model.name + " (" + model.id + ")";
            if (model.context_length > 0) {
                output += " - Context: " + std::to_string(model.context_length) + " tokens";
            }
            output += "\n";
        }
        
        output += "\nUse /model <model-id> to change the active model.\n";
        ui.displayOutput(output, "");
        
    } catch (const std::exception& e) {
        ui.displayError("Error listing models: " + std::string(e.what()));
    }
}

void CommandHandler::handleModelCommand(const std::string& model_id) {
    try {
        // Validate model exists
        auto model = db.getModelById(model_id);
        if (!model.has_value()) {
            ui.displayError("Model '" + model_id + "' not found. Use /models to see available models.");
            return;
        }
        
        // Change the active model
        modelManager.setActiveModel(model_id);
        
        // Persist the selection
        try {
            db.saveSetting("selected_model_id", model_id);
        } catch (const std::exception& e) {
            ui.displayError("Warning: Could not persist model selection: " + std::string(e.what()));
        }
        
        // Display success message
        ui.displayOutput("Model changed to: " + model->name + " (" + model_id + ")\n", "");
        
    } catch (const std::exception& e) {
        ui.displayError("Error changing model: " + std::string(e.what()));
    }
}