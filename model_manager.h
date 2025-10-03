#pragma once

#include <string>
#include <vector>
#include "model_types.h"
#include "database.h"
#include "ui_interface.h"

// Forward declarations
class PersistenceManager;
class UserInterface;

/**
 * ModelManager handles all model-related operations:
 * - Fetching models from the OpenRouter API
 * - Parsing API responses
 * - Caching models to the database
 * - Managing the active model selection
 * - Asynchronous model loading on startup
 */
class ModelManager {
public:
    explicit ModelManager(UserInterface& ui_ref, PersistenceManager& db_ref);
    ~ModelManager();

    // Initialization - must be called before using the model manager
    void initialize();

    // Get the currently active model ID
    std::string getActiveModelId() const { return active_model_id; }
    
    // Set the active model (validates existence and persists selection)
    void setActiveModel(const std::string& model_id);

private:
    // References to dependencies
    UserInterface& ui;
    PersistenceManager& db;

    // Active model state
    std::string active_model_id;

    // Private methods for model loading pipeline
    void loadModels();
    std::string fetchModelsFromAPI();
    std::vector<ModelData> parseModelsFromAPIResponse(const std::string& api_response);
    void cacheModelsToDB(const std::vector<ModelData>& models);
    
    // Helper to select the appropriate active model from a list
    void selectActiveModel(const std::vector<ModelData>& available_models, 
                          const std::string& context_msg,
                          const std::string& previously_selected_model_id);
};