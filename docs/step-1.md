# Plan for Part I: Database Schema and Operations

This document outlines the step-by-step plan to implement the database changes for "Dynamic Model Fetching, Caching, Selection, and Metadata - Part I".

## I. General Preparations

*   [ ] **Define `Model` Struct (`database.h`):**
    *   Create a new struct `Model` to represent AI model data.
    *   Fields:
        *   `std::string id;`
        *   `std::string name;`
        *   `std::string description;`
        *   `int context_length;`
        *   `std::string pricing_prompt;`
        *   `std::string pricing_completion;`
        *   `std::string architecture_input_modalities;` // JSON string
        *   `std::string architecture_output_modalities;` // JSON string
        *   `std::string architecture_tokenizer;`
        *   `int top_provider_is_moderated;` // 0 or 1 (boolean)
        *   `std::string per_request_limits;` // JSON string
        *   `std::string supported_parameters;` // JSON string
        *   `long long created_at_api;` // UNIX Timestamp (INTEGER)
        *   `std::string last_updated_db;` // Timestamp string (YYYY-MM-DD HH:MM:SS), retrieved from DB

## II. `models` Table Implementation

*   [ ] **Define `models` Table Schema (`database.cpp` - `PersistenceManager::Impl::Impl()`):**
    *   Add the `CREATE TABLE IF NOT EXISTS models` statement within the `Impl` constructor, after existing table creations.
    *   SQL Statement:
        ```sql
        CREATE TABLE IF NOT EXISTS models (
            id TEXT PRIMARY KEY,
            name TEXT,
            description TEXT,
            context_length INTEGER,
            pricing_prompt TEXT,
            pricing_completion TEXT,
            architecture_input_modalities TEXT, -- JSON array of strings
            architecture_output_modalities TEXT, -- JSON array of strings
            architecture_tokenizer TEXT,
            top_provider_is_moderated INTEGER, -- Boolean (0 or 1)
            per_request_limits TEXT, -- JSON object as string
            supported_parameters TEXT, -- JSON array of strings
            created_at_api INTEGER, -- Timestamp from API 'created' field
            last_updated_db TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- When this record was last updated in local DB
        );
        ```

*   [ ] **Implement `models` Table CRUD Operations:**

    *   [ ] **Function to insert a new model record (or update if exists - UPSERT):**
        *   **`database.h`:** Add declaration: `void saveOrUpdateModel(const Model& model);`
        *   **`database.cpp`:** Implement `PersistenceManager::saveOrUpdateModel`.
            *   SQL: `INSERT OR REPLACE INTO models (id, name, description, context_length, pricing_prompt, pricing_completion, architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);`
            *   Bind all fields from the `Model` struct. The `last_updated_db` column will be automatically updated by SQLite due to `DEFAULT CURRENT_TIMESTAMP` and the nature of `REPLACE`.

    *   [ ] **Function to retrieve a model by its `id`:**
        *   **`database.h`:** Add declaration: `std::optional<Model> getModelById(const std::string& id);`
        *   **`database.cpp`:** Implement `PersistenceManager::getModelById`.
            *   SQL: `SELECT id, name, description, context_length, pricing_prompt, pricing_completion, architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api, DATETIME(last_updated_db, 'localtime') as last_updated_db FROM models WHERE id = ?;`
            *   Populate and return `std::optional<Model>`.

    *   [ ] **Function to retrieve all models:**
        *   **`database.h`:** Add declaration: `std::vector<Model> getAllModels(bool orderByName = true);`
        *   **`database.cpp`:** Implement `PersistenceManager::getAllModels`.
            *   SQL (if `orderByName` is true): `SELECT id, name, description, context_length, pricing_prompt, pricing_completion, architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api, DATETIME(last_updated_db, 'localtime') as last_updated_db FROM models ORDER BY name ASC;`
            *   SQL (if `orderByName` is false): `SELECT id, name, description, context_length, pricing_prompt, pricing_completion, architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api, DATETIME(last_updated_db, 'localtime') as last_updated_db FROM models;`
            *   Populate and return `std::vector<Model>`.

    *   [ ] **Function to clear all models from the table:**
        *   **`database.h`:** Add declaration: `void clearAllModels();`
        *   **`database.cpp`:** Implement `PersistenceManager::clearAllModels`.
            *   SQL: `DELETE FROM models;`

## III. `messages` Table Modifications

*   [ ] **Add `model_id` column to `messages` table (`database.cpp` - `PersistenceManager::Impl::Impl()`):**
    *   Implement a migration strategy to add the column if it doesn't exist. This should be done within the `Impl` constructor.
    *   Method: Use `PRAGMA table_info('messages')` to check for the column's existence before attempting to add it.
    *   SQL to execute if column doesn't exist: `ALTER TABLE messages ADD COLUMN model_id TEXT;`
    *   Optionally, consider adding a foreign key constraint later: `FOREIGN KEY (model_id) REFERENCES models(id) ON DELETE SET NULL ON UPDATE CASCADE`. For now, just add the column.

*   [ ] **Update `Message` Struct (`database.h`):**
    *   Add field: `std::optional<std::string> model_id;`

*   [ ] **Update `PersistenceManager::Impl::insertMessage` (`database.cpp`):**
    *   Modify the SQL query to include `model_id`. Current: `INSERT INTO messages (role, content) VALUES (?, ?)`
    *   New SQL: `INSERT INTO messages (role, content, model_id) VALUES (?, ?, ?);`
    *   Update `sqlite3_bind_text` to bind `msg.model_id.value_or(nullptr)` (or handle appropriately if it's an empty string vs. SQL NULL).

*   [ ] **Update `PersistenceManager::getContextHistory` and `PersistenceManager::getHistoryRange` (`database.cpp`):**
    *   Modify SQL queries to select the `model_id` column from the `messages` table.
    *   Populate `msg.model_id` in the `Message` struct from the query results. If `model_id` from DB is NULL, `std::optional` should remain empty.

## IV. `settings` Table Updates for `selected_model_id`

*   [ ] **Implement functions to get and set `selected_model_id`:**
    *   The `settings` table and generic `saveSetting`/`loadSetting` functions already exist and can be reused.
    *   **`database.h`:**
        *   Add declaration: `void saveSelectedModelId(const std::string& model_id);`
        *   Add declaration: `std::optional<std::string> loadSelectedModelId();`
    *   **`database.cpp`:**
        *   Implement `PersistenceManager::saveSelectedModelId`: This function will call `impl->saveSetting("selected_model_id", model_id);`.
        *   Implement `PersistenceManager::loadSelectedModelId`: This function will call `impl->loadSetting("selected_model_id");`.

## V. Considerations

*   **Error Handling:** Ensure all new database functions properly check SQLite return codes and throw `std::runtime_error` on failure, consistent with existing code.
*   **Transactions:** For operations like `clearAllModels` followed by repopulating, ensure they are wrapped in transactions if they become part of a larger sequence. The existing transaction methods are suitable.
*   **SQLite Data Types:**
    *   `TEXT` for JSON strings is appropriate.
    *   `INTEGER` for boolean flags (0 or 1) and timestamps.
    *   `TIMESTAMP DEFAULT CURRENT_TIMESTAMP` is correctly used for `last_updated_db`.
*   **Foreign Key for `messages.model_id`:** As noted, adding the column is the first step. The FK constraint can be added later if deemed necessary, considering implications for `NULL` values or if `models` table might not yet contain the referenced `id`.
*   **`DATETIME(last_updated_db, 'localtime')`:** Used in SELECT queries for `models.last_updated_db` to ensure the retrieved timestamp string is in local time, which is generally more user-friendly if displayed. SQLite stores it as UTC by default with `CURRENT_TIMESTAMP`.