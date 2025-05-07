Feature: Dynamic Model Fetching, Caching, Selection, and Metadata #31

This plan outlines the steps to implement dynamic fetching, caching, and selection of models from the OpenRouter API, as well as associating chat messages with the model used.

**I. Database Schema and Operations (`database.cpp`, `database.h`)**

*   [ ] **Define `models` Table Schema:**
    *   Create a new table `models` to store information about available AI models.
    *   Fields:
        *   `id` (TEXT, PRIMARY KEY) - Model ID from API
        *   `name` (TEXT) - Human-readable model name
        *   `description` (TEXT) - Model description
        *   `context_length` (INTEGER) - Model context length
        *   `pricing_prompt` (TEXT) - Cost per prompt token (store as string to preserve precision, convert on use)
        *   `pricing_completion` (TEXT) - Cost per completion token
        *   `architecture_input_modalities` (TEXT) - JSON array of strings
        *   `architecture_output_modalities` (TEXT) - JSON array of strings
        *   `architecture_tokenizer` (TEXT)
        *   `top_provider_is_moderated` (INTEGER) - Boolean (0 or 1)
        *   `per_request_limits` (TEXT) - JSON object as string
        *   `supported_parameters` (TEXT) - JSON array of strings
        *   `created_at_api` (INTEGER) - Timestamp from API `created` field
        *   `last_updated_db` (TIMESTAMP, DEFAULT CURRENT_TIMESTAMP) - When this record was last updated in local DB
*   [ ] **Implement `models` Table CRUD Operations:**
    *   Function to create the `models` table if it doesn't exist.
    *   Function to insert a new model record.
    *   Function to update an existing model record (e.g., `UPSERT` or `INSERT OR REPLACE`).
    *   Function to retrieve a model by its `id`.
    *   Function to retrieve all models (e.g., for populating a dropdown), perhaps ordered by name.
    *   Function to clear all models from the table (for refreshing the cache).
*   [ ] **Modify `messages` Table (or Create `message_metadata`):**
    *   Add a `model_id` column (TEXT) to the `messages` table.
    *   This column will be a foreign key referencing `models.id` (optional, but good practice).
*   [ ] **Update/Create `settings` Table:**
    *   Add a key-value pair for `selected_model_id` (TEXT) to store the ID of the user's preferred model.
    *   Implement functions to get and set this `selected_model_id`.

**II. API Interaction and Model Caching (`chat_client.cpp`, `chat_client.h`, `curl_utils.h`, `config.h.in`)**

*   [ ] **Add Configuration for API:**
    *   Define `OPENROUTER_API_URL_MODELS` (e.g., \"https://openrouter.ai/api/v1/models\") in `config.h.in` and `config.h`.
    *   Define a `DEFAULT_MODEL_ID` ("deepseek/deepseek-chat-v3-0324:free") in `config.h.in` and `config.h` to be used as a fallback or initial model.
*   [ ] **Implement API Fetch Logic in `ChatClient`:**
    *   Create a new method `fetchModelsFromAPI()` that uses `curl_utils` to make a GET request to `OPENROUTER_API_URL_MODELS`.
    *   Note: API key management needs consideration (e.g., from `.env` or config file). For now, assume key is available.
*   [ ] **Parse API Response in `ChatClient`:**
    *   Use a JSON parsing library (e.g., nlohmann/json) to parse the response from `fetchModelsFromAPI()`.
    *   Extract the list of models and their attributes.
*   [ ] **Implement Model Caching Logic in `ChatClient`:**
    *   Create a method `cacheModels(const std::vector<ModelData>& models)` that interacts with `Database` to:
        *   Optionally clear existing models from the `models` table.
        *   Insert/update the fetched models into the `models` table.
*   [ ] **Integrate Model Fetching on Startup in `ChatClient`:**
    *   In the `ChatClient` constructor or an initialization method, call `fetchModelsFromAPI()` and then `cacheModels()`.
    *   This should likely run in a separate thread to avoid blocking the UI.
    *   Implement a mechanism to signal completion or failure to the UI.
*   [ ] **Error Handling for API Fetch:**
    *   If `fetchModelsFromAPI()` fails, log the error.
    *   Fall back to using cached models if available, or the `DEFAULT_MODEL_ID`.
    *   Provide feedback to the user via the GUI if models cannot be fetched/updated.

**III. GUI Changes for Model Selection (`main_gui.cpp`, `gui_interface/gui_interface.cpp`, `gui_interface/gui_interface.h`)**

*   [ ] **Add Model Selection Dropdown in `main_gui.cpp`:**
    *   Use ImGui to add a combo box/dropdown menu to the chat interface.
*   [ ] **Populate Dropdown in `GuiInterface` / `main_gui.cpp`:**
    *   `GuiInterface` needs a method `getAvailableModels()` that calls `Database::getAllModels()`.
    *   `main_gui.cpp` will call this method to get the list of models.
    *   Populate the ImGui combo box with model names, storing their corresponding IDs.
*   [ ] **Handle Model Selection in `GuiInterface` / `main_gui.cpp`:**
    *   When the user selects a model:
        *   Get the `id` of the selected model.
        *   Call a `GuiInterface` method `setSelectedModel(const std::string& model_id)`.
        *   This method will call `Database::setSetting(\"selected_model_id\", model_id)`.
        *   Update the `ChatClient`'s currently active model.
*   [ ] **Load and Apply Selected Model in `ChatClient` / `GuiInterface`:**
    *   On startup, load `selected_model_id` from `Database`.
    *   If found and valid, set it as the active model.
    *   If not found/invalid, use `DEFAULT_MODEL_ID`.
    *   Ensure the GUI dropdown reflects the active model.

**IV. Message Metadata (`chat_client.cpp`, `chat_client.h`, `database.cpp`, `database.h`)**

*   [ ] **Store Model ID with Messages in `ChatClient`:**
    *   When preparing a message for storage, include the `id` of the model used.
    *   Pass this `model_id` to `Database::addMessage()`.
*   [ ] **Update `Database::addMessage()`:**
    *   Modify `Database::addMessage()` to accept and store the `model_id`.
*   [ ] **(Optional) Display Model Used for Each Message in GUI:**
    *   When rendering chat history, retrieve `model_id` for each message.
    *   Fetch model name from `models` table.
    *   Display model name with the message.

**V. General and Error Handling**

*   [ ] **Initial Model Handling:**
    *   Use `DEFAULT_MODEL_ID` on first launch or if API is unavailable.
*   [ ] **Asynchronous Operations and UI Feedback:**
    *   Ensure API fetching/caching are asynchronous.
    *   Provide UI feedback (e.g., \"Loading models...\").
*   [ ] **Testing:**
    *   Test all new functionalities and error scenarios.

---
**References:**
*   OpenRouter API Documentation: https://openrouter.ai/docs/api-reference/list-available-models