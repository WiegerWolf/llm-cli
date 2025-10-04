#pragma once

#include <string>
#include <functional>
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
 * - /new - Start a new chat session
 * - /chats - List all chat sessions
 * Provides centralized command parsing and execution
 */
class CommandHandler {
public:
    explicit CommandHandler(UserInterface& ui_ref,
                           PersistenceManager& db_ref,
                           ModelManager& model_manager_ref);

    // Handle a command input. Returns true if command was handled, false otherwise
    bool handleCommand(const std::string& input);

    // Callback for session switching
    void setSessionSwitchCallback(std::function<void(int)> callback);

private:
    UserInterface& ui;
    PersistenceManager& db;
    ModelManager& modelManager;
    std::function<void(int)> sessionSwitchCallback;

    // Individual command handlers
    void handleModelsCommand();
    void handleModelCommand(const std::string& model_id_arg);
    void handleNewCommand();
    void handleChatsCommand();
};