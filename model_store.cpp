#include "model_store.h"
#include "sqlite_connection.h"
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace {
struct SQLiteStmtDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};
using unique_sqlite_stmt_ptr = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;
} // end anonymous namespace

ModelStore::ModelStore(SQLiteConnection& db_conn) : m_db_conn(db_conn) {}

void ModelStore::clearModelsTable() {
    m_db_conn.exec("DELETE FROM models;");
}

void ModelStore::insertOrUpdateModel(const ModelData& model) {
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
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare insertOrUpdateModel statement: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    unique_sqlite_stmt_ptr stmt_guard(stmt);

    sqlite3_bind_text(stmt, 1, model.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, model.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, model.description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, model.context_length);
    sqlite3_bind_text(stmt, 5, model.pricing_prompt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, model.pricing_completion.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, model.architecture_input_modalities.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, model.architecture_output_modalities.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, model.architecture_tokenizer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 10, model.top_provider_is_moderated);
    sqlite3_bind_text(stmt, 11, model.per_request_limits.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, model.supported_parameters.c_str(), -1, SQLITE_STATIC);

    sqlite3_bind_int(stmt, 13, model.created_at_api);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("Failed to execute insertOrUpdateModel statement: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
}

std::vector<ModelData> ModelStore::getAllModels() {
    const char* sql = "SELECT id, name, description, context_length, pricing_prompt, pricing_completion, "
                      "architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, "
                      "top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api, last_updated_db "
                      "FROM models ORDER BY name";

    sqlite3_stmt* stmt = nullptr;
    std::vector<ModelData> models;

    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare getAllModels statement: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    unique_sqlite_stmt_ptr stmt_guard(stmt);

    auto get_text_or_empty = [&](int col_idx) {
        const unsigned char* text = sqlite3_column_text(stmt, col_idx);
        return text ? reinterpret_cast<const char*>(text) : "";
    };

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ModelData model;
        model.id = get_text_or_empty(0);
        model.name = get_text_or_empty(1);
        model.description = get_text_or_empty(2);
        model.context_length = sqlite3_column_int(stmt, 3);
        model.pricing_prompt = get_text_or_empty(4);
        model.pricing_completion = get_text_or_empty(5);
        model.architecture_input_modalities = get_text_or_empty(6);
        model.architecture_output_modalities = get_text_or_empty(7);
        model.architecture_tokenizer = get_text_or_empty(8);
        model.top_provider_is_moderated = sqlite3_column_int(stmt, 9);
        model.per_request_limits = get_text_or_empty(10);
        model.supported_parameters = get_text_or_empty(11);

        model.created_at_api = sqlite3_column_int(stmt, 12);
        // last_updated_db is consciously ignored here
        models.push_back(model);
    }
    return models;
}

std::optional<ModelData> ModelStore::getModelById(const std::string& model_id) {
    const char* sql = "SELECT id, name, description, context_length, pricing_prompt, pricing_completion, "
                      "architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, "
                      "top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api, last_updated_db "
                      "FROM models WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare getModelById statement: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    unique_sqlite_stmt_ptr stmt_guard(stmt);

    sqlite3_bind_text(stmt, 1, model_id.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ModelData model;
        auto get_text_or_empty = [&](int col_idx) {
            const unsigned char* text = sqlite3_column_text(stmt, col_idx);
            return text ? reinterpret_cast<const char*>(text) : "";
        };

        model.id = get_text_or_empty(0);
        model.name = get_text_or_empty(1);
        model.description = get_text_or_empty(2);
        model.context_length = sqlite3_column_int(stmt, 3);
        model.pricing_prompt = get_text_or_empty(4);
        model.pricing_completion = get_text_or_empty(5);
        model.architecture_input_modalities = get_text_or_empty(6);
        model.architecture_output_modalities = get_text_or_empty(7);
        model.architecture_tokenizer = get_text_or_empty(8);
        model.top_provider_is_moderated = sqlite3_column_int(stmt, 9);
        model.per_request_limits = get_text_or_empty(10);
        model.supported_parameters = get_text_or_empty(11);

        model.created_at_api = sqlite3_column_int(stmt, 12);
        return model;
    }
    return std::nullopt;
}

std::optional<std::string> ModelStore::getModelNameById(const std::string& model_id) {
    const char* sql = "SELECT name FROM models WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    unique_sqlite_stmt_ptr stmt_guard(stmt);
    sqlite3_bind_text(stmt, 1, model_id.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 0);
        if (name) {
            return std::string(reinterpret_cast<const char*>(name));
        }
    }
    return std::nullopt;
}

void ModelStore::replaceModelsInDB(const std::vector<ModelData>& models) {
    m_db_conn.exec("BEGIN");
    try {
        clearModelsTable();
        for (const auto& model : models) {
            insertOrUpdateModel(model);
        }
        m_db_conn.exec("COMMIT");
    } catch (const std::exception& e) {
        m_db_conn.exec("ROLLBACK");
        throw;
    }
}