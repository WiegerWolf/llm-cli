# Part II: API Interaction and Model Caching - Implementation Plan

This document outlines the detailed step-by-step plan to implement Part II: API Interaction and Model Caching for the "Dynamic Model Fetching, Caching, Selection, and Metadata" feature.

## Overall Goal
Integrate dynamic fetching of AI models from an API, parse the response, cache the models in a local database, and handle potential errors. This involves changes primarily in `chat_client.cpp`, `chat_client.h`, and `config.h.in`, with related considerations for `database.h` and `database.cpp`.

## Detailed Steps

### 1. Configuration (`config.h.in` and `config.h`)
*   [ ] Define `OPENROUTER_API_URL_MODELS` in `config.h.in` (e.g., `"#define OPENROUTER_API_URL_MODELS \"https://openrouter.ai/api/v1/models\""`).
*   [ ] Ensure `OPENROUTER_API_URL_MODELS` is correctly processed by CMake into `config.h`.
*   [ ] Define `DEFAULT_MODEL_ID` in `config.h.in` (e.g., `"#define DEFAULT_MODEL_ID \"deepseek/deepseek-chat-v3-0324:free\""`).
*   [ ] Ensure `DEFAULT_MODEL_ID` is correctly processed by CMake into `config.h`.

### 2. `ModelData` Structure Definition
*   [ ] Define a `struct ModelData` to hold model attributes. This could be in `chat_client.h`, `database.h`, or a new `model_types.h` file. For initial planning, let's assume `chat_client.h` or a new `model_types.h`.
    ```cpp
    // In a suitable header (e.g., model_types.h or chat_client.h)
    struct ModelData {
        std::string id;          // e.g., "openai/gpt-4"
        std::string name;        // e.g., "GPT-4"
        // Add other relevant fields based on API response, e.g.:
        // int context_length;
        // double input_cost_per_mtok;
        // double output_cost_per_mtok;
        // std::string architecture;
        // bool supports_tools;
    };
    ```

### 3. `ChatClient` Header Modifications (`chat_client.h`)
*   [ ] Include `config.h` for `OPENROUTER_API_URL_MODELS` and `DEFAULT_MODEL_ID`.
*   [ ] Include the header where `ModelData` is defined (if not in `chat_client.h` itself).
*   [ ] Declare new private method: `std::string fetchModelsFromAPI();`
*   [ ] Declare new private method: `std::vector<ModelData> parseModelsFromAPIResponse(const std::string& api_response);`
*   [ ] Declare new private method: `void cacheModelsToDB(const std::vector<ModelData>& models);`
*   [ ] Declare new private method: `void initializeModels();` (This will orchestrate fetching, parsing, and caching).
*   [ ] Add private member variable(s) for managing the model initialization thread and its status:
    *   `std::thread model_init_thread;` (Consider `jthread` if C++20 is available and appropriate for automatic joining).
    *   `std::atomic<bool> models_initialized_successfully{false};`
    *   `std::atomic<bool> model_initialization_attempted{false};`
### 4. `ChatClient` Implementation (`chat_client.cpp`)

#### 4.1. `fetchModelsFromAPI()` Method
*   [ ] Implement `std::string ChatClient::fetchModelsFromAPI()`.
*   [ ] Include `<curl/curl.h>`, `<string>`, `<vector>`, `"curl_utils.h"`, `"config.h"`.
*   [ ] Use `CURL* curl = curl_easy_init();` with RAII wrapper (`curl_guard`).
*   [ ] Get `OPENROUTER_API_URL_MODELS` from `config.h`.
*   [ ] Set `CURLOPT_URL` to the API URL.
*   [ ] Set up a GET request (default, no specific `CURLOPT_HTTPGET` needed if no POST fields).
*   [ ] Retrieve API key using `get_openrouter_api_key()`.
*   [ ] Add `Authorization: Bearer <API_KEY>` header using `curl_slist_append` with RAII wrapper (`headers_guard`).
*   [ ] Add other recommended headers: `HTTP-Referer`, `X-Title`.
*   [ ] Use `WriteCallback` (from `curl_utils.h`) and a `std::string response_buffer` for `CURLOPT_WRITEDATA`.
*   [ ] Perform `curl_easy_perform(curl)`.
*   [ ] Check `CURLcode res` for errors. If `res != CURLE_OK`, log error using `ui.displayError()` (e.g., "API request to fetch models failed: " + `curl_easy_strerror(res)`) and return an empty string or throw an exception.
*   [ ] Return `response_buffer`.

#### 4.2. `parseModelsFromAPIResponse()` Method
*   [ ] Implement `std::vector<ModelData> ChatClient::parseModelsFromAPIResponse(const std::string& api_response)`.
*   [ ] Include `<nlohmann/json.hpp>`, `<vector>`, `<string>`.
*   [ ] Create an empty `std::vector<ModelData> parsed_models;`.
*   [ ] Use a try-catch block for `nlohmann::json::parse_error`.
    *   Inside `try`: `nlohmann::json j = nlohmann::json::parse(api_response);`
    *   Check if `j` is an object and contains a "data" field which is an array (common pattern for OpenRouter list endpoints). Adjust based on actual API response structure.
    *   Iterate through `j["data"]` (or the correct path to the array of models).
        *   For each model object in the array:
            *   Create a `ModelData model_item;`.
            *   Extract `id`: `model_item.id = model_obj.value("id", "");` (use `.value()` for safety).
            *   Extract `name`: `model_item.name = model_obj.value("name", model_item.id);` (fallback name to id).
            *   Extract other relevant fields defined in `ModelData` (e.g., `context_length`, `pricing_info`) using `.value()` with appropriate defaults.
            *   If `model_item.id` is not empty, add to `parsed_models.push_back(model_item);`.
    *   Inside `catch (const nlohmann::json::parse_error& e)`: Log error using `ui.displayError()` (e.g., "Failed to parse models API response: " + `std::string(e.what())`) and return the (possibly empty) `parsed_models`.
*   [ ] Return `parsed_models`.

#### 4.3. `cacheModelsToDB()` Method
*   [ ] Implement `void ChatClient::cacheModelsToDB(const std::vector<ModelData>& models)`.
*   [ ] Include `"database.h"`.
*   [ ] Call `db.clearModelsTable();` (This method needs to be added to `PersistenceManager`).
    *   *Note*: Consider if clearing all models is always desired. The requirement says "Optionally clear". For now, plan to clear.
*   [ ] Loop through `const ModelData& model_item : models`.
    *   Call `db.insertOrUpdateModel(model_item);` (This method needs to be added to `PersistenceManager`).
*   [ ] Add try-catch around database operations if `PersistenceManager` methods can throw. Log errors using `ui.displayError()`.

#### 4.4. `initializeModels()` Method
*   [ ] Implement `void ChatClient::initializeModels()`.
*   [ ] Set `model_initialization_attempted = true;`.
*   [ ] Call `std::string api_response_str = fetchModelsFromAPI();`.
*   [ ] If `api_response_str.empty()` (or however `fetchModelsFromAPI` signals failure):
    *   `ui.displayStatus("Failed to fetch models from API. Attempting to load from cache...");`
    *   `std::vector<ModelData> cached_models = db.getAllModels();` (Method to be added to `PersistenceManager`).
    *   If `cached_models.empty()`:
        *   `ui.displayError("Failed to load models from cache. Falling back to default model.");`
        *   Set current model to `DEFAULT_MODEL_ID` (e.g., `this->model_name = DEFAULT_MODEL_ID;` if `model_name` is used for selection).
        *   `models_initialized_successfully = false;`
    *   Else (`cached_models` is not empty):
        *   `ui.displayStatus("Successfully loaded models from cache.");`
        *   (Optional: Populate an internal list of available models from `cached_models` if `ChatClient` maintains one).
        *   `models_initialized_successfully = true;`
*   [ ] Else (API fetch was successful, `api_response_str` is not empty):
    *   `std::vector<ModelData> fetched_models = parseModelsFromAPIResponse(api_response_str);`
    *   If `fetched_models.empty()`:
        *   `ui.displayError("Fetched models from API, but failed to parse or no models found. Check API response format.");`
        *   (Consider fallback to cache here as well, or just use default).
        *   `models_initialized_successfully = false;`
    *   Else (`fetched_models` is not empty):
        *   `cacheModelsToDB(fetched_models);`
        *   `ui.displayStatus("Successfully fetched and cached models from API.");`
        *   (Optional: Populate internal list of available models).
        *   `models_initialized_successfully = true;`
*   [ ] (UI Signaling) The UI can periodically check `model_initialization_attempted` and `models_initialized_successfully` or `ChatClient` can use a callback/event to notify the UI.

#### 4.5. `ChatClient` Constructor Modification
*   [ ] In `ChatClient::ChatClient(UserInterface& ui_ref, PersistenceManager& db_ref)`:
    *   The constructor itself no longer directly launches model initialization. Instead, a separate public method `ChatClient::initialize_model_manager()` is called by the application's main function (e.g., in `main_gui.cpp` or `main_cli.cpp`) after the `ChatClient` object is created.
    *   Inside `ChatClient::initialize_model_manager()`:
        *   The actual asynchronous part of model initialization, `ChatClient::loadModelsAsync()`, is launched using `std::async(std::launch::async, &ChatClient::loadModelsAsync, this)`.
        *   The `std::future<void>` returned by `std::async` is stored in a member variable `model_load_future`.
        *   `initialize_model_manager()` then calls `model_load_future.get()` to wait for the `loadModelsAsync` task to complete. This makes the model initialization process effectively synchronous from the perspective of the caller of `initialize_model_manager`, ensuring models are loaded (or attempted to be loaded) before proceeding, while still allowing the underlying fetch/parse operations to be performed asynchronously.
    *   As a safeguard, the `ChatClient::~ChatClient()` destructor checks if `model_load_future.valid()` is true. If so, it calls `model_load_future.wait()` to ensure that the asynchronous task completes before the `ChatClient` object is destroyed. This prevents the `loadModelsAsync` task from potentially accessing a destructed `ChatClient` instance.
    *   The previous approach using `std::thread(&ChatClient::initializeModels, this).detach();` has been replaced by this more robust `std::async` and `std::future` mechanism.

### 5. `Database` / `PersistenceManager` Modifications (`database.h`, `database.cpp`)
*   (These are dependencies for `ChatClient`'s caching logic. Actual implementation is part of the Database Schema task, but methods need to be available).
*   [ ] Ensure `ModelData` struct is accessible (defined or included).
*   [ ] In `PersistenceManager` class (or `Database`):
    *   Declare and implement `void clearModelsTable();`
        *   SQL: `DELETE FROM models;`
    *   Declare and implement `void insertOrUpdateModel(const ModelData& model);`
        *   SQL: `INSERT INTO models (id, name, ...) VALUES (?, ?, ...) ON CONFLICT(id) DO UPDATE SET name=excluded.name, ...;` (SQLite example for upsert).
    *   Declare and implement `std::vector<ModelData> getAllModels();`
        *   SQL: `SELECT id, name, ... FROM models;`
        *   Populate and return `std::vector<ModelData>`.
    *   Ensure the `models` table schema (defined in Part I) includes columns for all fields in `ModelData` (e.g., `id TEXT PRIMARY KEY`, `name TEXT`, `context_length INTEGER`, etc.).

### 6. Error Handling and Fallbacks (Review)
*   [ ] Review all new methods in `ChatClient` for comprehensive error logging using `ui.displayError()` or `ui.displayStatus()`.
*   [ ] Verify fallback logic:
    *   API fetch failure -> Load from cache.
    *   Cache load failure (or empty cache after API fail) -> Use `DEFAULT_MODEL_ID`.
    *   User feedback for each step of fallback.

### 7. API Key Management
*   [ ] The existing `get_openrouter_api_key()` in `chat_client.cpp` should be usable for the `/models` endpoint if it requires the same API key. Confirm if this endpoint is authenticated. (Task states: "assume key is available").

### 8. `curl_utils.h`
*   [ ] No changes anticipated for `curl_utils.h` itself, as `WriteCallback` is generic.

### 9. CMakeLists.txt (Consideration)
*   [ ] Ensure `config.h.in` is processed to `config.h` correctly.
*   [ ] If `model_types.h` is created, add it to the build system.