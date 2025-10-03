#pragma once

#include "database_core.h"
#include "../model_types.h"
#include <vector>
#include <string>
#include <optional>

namespace database {

/**
 * ModelRepository - Manages model metadata storage and retrieval
 * 
 * Responsibilities:
 * - Model CRUD operations (Create, Read, Update, Delete)
 * - Bulk model replacement (atomic)
 * - Model queries by ID
 * - Model name lookup for UI display
 * - Model caching (future enhancement)
 */
class ModelRepository {
public:
    /**
     * Constructor
     * @param core Reference to DatabaseCore for connection access
     */
    explicit ModelRepository(DatabaseCore& core);
    
    // CRUD operations
    
    /**
     * Insert or update a model in the database
     * @param model The model data to insert or update
     * @throws std::runtime_error if operation fails
     */
    void insertOrUpdateModel(const ModelData& model);
    
    /**
     * Clear all models from the database
     * @throws std::runtime_error if operation fails
     */
    void clearAllModels();
    
    /**
     * Atomically replace all models in the database
     * Uses transactions to ensure atomicity
     * @param models Vector of models to replace existing models with
     * @throws std::runtime_error if operation fails (transaction will be rolled back)
     */
    void replaceModels(const std::vector<ModelData>& models);
    
    // Query operations
    
    /**
     * Get all models from the database, ordered by name
     * @return Vector of all models
     * @throws std::runtime_error if query fails
     */
    std::vector<ModelData> getAllModels();
    
    /**
     * Get a specific model by its ID
     * @param id The model ID to look up
     * @return Optional containing the model if found, nullopt otherwise
     * @throws std::runtime_error if query fails
     */
    std::optional<ModelData> getModelById(const std::string& id);
    
    /**
     * Get just the model name by ID (lightweight query for UI display)
     * @param id The model ID to look up
     * @return Optional containing the model name if found, nullopt otherwise
     * @throws std::runtime_error if query fails
     */
    std::optional<std::string> getModelNameById(const std::string& id);

private:
    DatabaseCore& core_;  // Reference to database core for connection access
    
    /**
     * Build a ModelData object from a database row
     * @param stmt The prepared statement pointing to a row
     * @return ModelData object populated from the row
     */
    ModelData buildModelFromRow(sqlite3_stmt* stmt);
    
    /**
     * Bind ModelData fields to a prepared statement
     * @param stmt The prepared statement to bind to
     * @param model The model data to bind
     */
    void bindModelToStatement(sqlite3_stmt* stmt, const ModelData& model);
};

} // namespace database