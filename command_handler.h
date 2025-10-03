#pragma once

#include <string>
#include "database.h"
#include "ui_interface.h"

// Forward declarations
class PersistenceManager;
class UserInterface;
class ModelManager;

/**
 * CommandHandler processes slash commands:
 * - /models - List all available models
 * - /model <id> - Change the active model
 * Provides centralized command parsing and execution
 */
class CommandHandler {
public:
    explicit CommandHandler(UserInterface& ui_ref,
                           PersistenceManager& db_ref,
                           ModelManager& model_manager_ref);
    
    // Handle a command input. Returns true if command was handled, false otherwise
    bool handleCommand(const std::string& input);

private:
    UserInterface& ui;
    PersistenceManager& db;
    ModelManager& modelManager;
    
    // Individual command handlers
    void handleModelsCommand();
    void handleModelCommand(const std::string& model_id_arg);
};