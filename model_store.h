#pragma once

#include "database_fwd.h"
#include "model_types.h"
#include <optional>
#include <string>
#include <vector>

namespace app {
namespace db {

/*
 * Manages persistence for AI model metadata.
 * This class handles all CRUD operations on the `models` table.
 */
class ModelStore {
public:
    /// Constructs a ModelStore using an existing database connection.
    explicit ModelStore(SQLiteConnection& db_conn);

    /// Removes all records from the models table.
    void clearModelsTable();

    /// Inserts a new model or updates it if it already exists.
    void insertOrUpdateModel(const ModelData& model);

    /// Retrieves all models from the database.
    std::vector<ModelData> getAllModels();

    /// Fetches a single model by its unique ID.
    std::optional<ModelData> getModelById(const std::string& model_id);

    /// Fetches just the name of a model by its unique ID.
    std::optional<std::string> getModelNameById(const std::string& model_id);

    /// Replaces the entire set of models in the DB with a new list.
    void replaceModelsInDB(const std::vector<ModelData>& models);

private:
    SQLiteConnection& m_db_conn;
};

} // namespace db
} // namespace app
