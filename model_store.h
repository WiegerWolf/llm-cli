#pragma once

#include "database_fwd.h"
#include <string>
#include <vector>
#include <optional>
#include "model_types.h"

class ModelStore {
public:
    explicit ModelStore(SQLiteConnection& db_conn);

    void clearModelsTable();
    void insertOrUpdateModel(const ModelData& model);
    std::vector<ModelData> getAllModels();
    std::optional<ModelData> getModelById(const std::string& model_id);
    std::optional<std::string> getModelNameById(const std::string& model_id);
    void replaceModelsInDB(const std::vector<ModelData>& models);

private:
    SQLiteConnection& m_db_conn;
};