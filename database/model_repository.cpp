#include "model_repository.h"
#include <stdexcept>

namespace database {

ModelRepository::ModelRepository(DatabaseCore& core)
    : core_(core) {
}

void ModelRepository::insertOrUpdateModel(const ModelData& model) {
    const char* sql = R"(
INSERT INTO models (
    id, name, description, context_length, pricing_prompt, pricing_completion,
    architecture_input_modalities, architecture_output_modalities, architecture_tokenizer,
    top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    name=excluded.name,
    description=excluded.description,
    context_length=excluded.context_length,
    pricing_prompt=excluded.pricing_prompt,
    pricing_completion=excluded.pricing_completion,
    architecture_input_modalities=excluded.architecture_input_modalities,
    architecture_output_modalities=excluded.architecture_output_modalities,
    architecture_tokenizer=excluded.architecture_tokenizer,
    top_provider_is_moderated=excluded.top_provider_is_moderated,
    per_request_limits=excluded.per_request_limits,
    supported_parameters=excluded.supported_parameters,
    created_at_api=excluded.created_at_api,
    last_updated_db=CURRENT_TIMESTAMP
)";

    auto stmt = core_.prepareStatement(sql);
    bindModelToStatement(stmt.get(), model);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        throw std::runtime_error("insertOrUpdateModel failed: " + std::string(sqlite3_errmsg(core_.getConnection())));
    }
}

void ModelRepository::clearAllModels() {
    core_.exec("DELETE FROM models;");
}

void ModelRepository::replaceModels(const std::vector<ModelData>& models) {
    core_.beginTransaction();
    try {
        clearAllModels();
        for (const auto& model : models) {
            insertOrUpdateModel(model);
        }
        core_.commitTransaction();
    } catch (const std::exception& e) {
        core_.rollbackTransaction();
        throw std::runtime_error("Failed to replace models in DB: " + std::string(e.what()));
    }
}

std::vector<ModelData> ModelRepository::getAllModels() {
    const char* sql = R"(
SELECT
    id, name, description, context_length, pricing_prompt, pricing_completion,
    architecture_input_modalities, architecture_output_modalities, architecture_tokenizer,
    top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api,
    last_updated_db
FROM models ORDER BY name ASC;
)";

    auto stmt = core_.prepareStatement(sql);
    
    // Helper lambda to safely get text, handling NULLs by returning empty string
    auto get_text_or_empty = [&](int col_idx) {
        const unsigned char* text = sqlite3_column_text(stmt.get(), col_idx);
        return text ? reinterpret_cast<const char*>(text) : "";
    };

    std::vector<ModelData> models;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        ModelData model;
        model.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        model.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        
        model.description = get_text_or_empty(2);
        model.context_length = sqlite3_column_int(stmt.get(), 3);
        model.pricing_prompt = get_text_or_empty(4);
        model.pricing_completion = get_text_or_empty(5);
        model.architecture_input_modalities = get_text_or_empty(6);
        model.architecture_output_modalities = get_text_or_empty(7);
        model.architecture_tokenizer = get_text_or_empty(8);
        model.top_provider_is_moderated = (sqlite3_column_int(stmt.get(), 9) == 1);
        model.per_request_limits = get_text_or_empty(10);
        model.supported_parameters = get_text_or_empty(11);
        model.created_at_api = sqlite3_column_int64(stmt.get(), 12);
        model.last_updated_db = get_text_or_empty(13);

        models.push_back(model);
    }

    if (sqlite3_errcode(core_.getConnection()) != SQLITE_OK && 
        sqlite3_errcode(core_.getConnection()) != SQLITE_DONE) {
        throw std::runtime_error("getAllModels failed during step: " + std::string(sqlite3_errmsg(core_.getConnection())));
    }
    
    return models;
}

std::optional<ModelData> ModelRepository::getModelById(const std::string& model_id) {
    const char* sql = "SELECT id, name, description, context_length, pricing_prompt, pricing_completion, architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api, DATETIME(last_updated_db, 'localtime') as last_updated_db FROM models WHERE id = ?;";
    
    auto stmt = core_.prepareStatement(sql);

    if (sqlite3_bind_text(stmt.get(), 1, model_id.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind model_id in getModelById: " + std::string(sqlite3_errmsg(core_.getConnection())));
    }

    int step_result = sqlite3_step(stmt.get());
    if (step_result == SQLITE_ROW) {
        ModelData model;
        model.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        
        const unsigned char* name_text = sqlite3_column_text(stmt.get(), 1);
        model.name = name_text ? reinterpret_cast<const char*>(name_text) : "";

        const unsigned char* desc_text = sqlite3_column_text(stmt.get(), 2);
        model.description = desc_text ? reinterpret_cast<const char*>(desc_text) : "";
        
        model.context_length = sqlite3_column_int(stmt.get(), 3);
        
        const unsigned char* pp_text = sqlite3_column_text(stmt.get(), 4);
        model.pricing_prompt = pp_text ? reinterpret_cast<const char*>(pp_text) : "";

        const unsigned char* pc_text = sqlite3_column_text(stmt.get(), 5);
        model.pricing_completion = pc_text ? reinterpret_cast<const char*>(pc_text) : "";

        const unsigned char* aim_text = sqlite3_column_text(stmt.get(), 6);
        model.architecture_input_modalities = aim_text ? reinterpret_cast<const char*>(aim_text) : "";

        const unsigned char* aom_text = sqlite3_column_text(stmt.get(), 7);
        model.architecture_output_modalities = aom_text ? reinterpret_cast<const char*>(aom_text) : "";

        const unsigned char* at_text = sqlite3_column_text(stmt.get(), 8);
        model.architecture_tokenizer = at_text ? reinterpret_cast<const char*>(at_text) : "";
        
        model.top_provider_is_moderated = sqlite3_column_int(stmt.get(), 9) != 0;
        
        const unsigned char* prl_text = sqlite3_column_text(stmt.get(), 10);
        model.per_request_limits = prl_text ? reinterpret_cast<const char*>(prl_text) : "";

        const unsigned char* sp_text = sqlite3_column_text(stmt.get(), 11);
        model.supported_parameters = sp_text ? reinterpret_cast<const char*>(sp_text) : "";
        
        model.created_at_api = sqlite3_column_int64(stmt.get(), 12);
        
        const unsigned char* ludb_text = sqlite3_column_text(stmt.get(), 13);
        model.last_updated_db = ludb_text ? reinterpret_cast<const char*>(ludb_text) : "";
        
        return model;
    } else if (step_result != SQLITE_DONE) {
        throw std::runtime_error("Failed to execute getModelById statement: " + std::string(sqlite3_errmsg(core_.getConnection())));
    }
    
    return std::nullopt;
}

std::optional<std::string> ModelRepository::getModelNameById(const std::string& model_id) {
    const char* sql = "SELECT name FROM models WHERE id = ?";
    
    auto stmt = core_.prepareStatement(sql);

    if (sqlite3_bind_text(stmt.get(), 1, model_id.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind model_id in getModelNameById: " + std::string(sqlite3_errmsg(core_.getConnection())));
    }

    int step_result = sqlite3_step(stmt.get());
    if (step_result == SQLITE_ROW) {
        const unsigned char* name_text = sqlite3_column_text(stmt.get(), 0);
        if (name_text) {
            return std::string(reinterpret_cast<const char*>(name_text));
        }
    } else if (step_result != SQLITE_DONE) {
        throw std::runtime_error("Error during sqlite3_step in getModelNameById: " + std::string(sqlite3_errmsg(core_.getConnection())));
    }
    
    return std::nullopt;
}

ModelData ModelRepository::buildModelFromRow(sqlite3_stmt* stmt) {
    ModelData model;
    // This is a helper that could be used to reduce duplication in the future
    // Currently not used as each query has slightly different column ordering
    return model;
}

void ModelRepository::bindModelToStatement(sqlite3_stmt* stmt, const ModelData& model) {
    sqlite3_bind_text(stmt, 1, model.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, model.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, model.description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, model.context_length);
    sqlite3_bind_text(stmt, 5, model.pricing_prompt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, model.pricing_completion.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, model.architecture_input_modalities.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, model.architecture_output_modalities.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, model.architecture_tokenizer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 10, model.top_provider_is_moderated ? 1 : 0);
    sqlite3_bind_text(stmt, 11, model.per_request_limits.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, model.supported_parameters.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 13, model.created_at_api);
}

} // namespace database