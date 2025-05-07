# Part III: GUI Changes for Model Selection - Detailed Plan

This document outlines the step-by-step plan to implement GUI changes for model selection as part of the "Dynamic Model Fetching, Caching, Selection, and Metadata" feature.

## I. `GuiInterface` Modifications (`gui_interface.h`, `gui_interface.cpp`)

### A. Prerequisites: Database Manager Access
1.  [ ] **Modify `GuiInterface` to access `PersistenceManager`**:
    *   In `gui_interface.h`:
        *   Add `PersistenceManager& db_manager_ref;` as a private member.
        *   Update the constructor: `GuiInterface(PersistenceManager& db_manager);`
    *   In `gui_interface.cpp`:
        *   Initialize `db_manager_ref` in the constructor's initializer list.
    *   In `main_gui.cpp`:
        *   Update `GuiInterface` instantiation: `GuiInterface gui_ui(db_manager);`

### B. Data Structures and Member Variables in `gui_interface.h`:
1.  [ ] **Define `ModelEntry` struct**:
    *   This struct will hold model information (ID and name) for display and selection.
    *   ```cpp
        // In gui_interface.h
        struct ModelEntry {
            std::string id;
            std::string name;
            // Potentially other metadata if needed for display in the future
        };
        ```
2.  [ ] **Add member variables for model state**:
    *   `private: std::string current_selected_model_id;`
    *   `private: const std::string default_model_id = "DEFAULT_MODEL_ID"; // Define an actual default ID`

### C. New Public Methods in `gui_interface.h` (implement in `gui_interface.cpp`):
1.  [ ] **`std::vector<ModelEntry> getAvailableModels() const;`**
    *   **Purpose**: Fetches all available models from the database.
    *   **Implementation (`gui_interface.cpp`)**:
        *   Call `db_manager_ref.getAllModels()` (this method might need to be ensured in `PersistenceManager` if it doesn't map directly, or adapt to what `PersistenceManager` provides for listing models).
        *   Transform the raw database result (e.g., `std::vector<std::map<std::string, std::string>>`) into `std::vector<ModelEntry>`. Each entry should have `id` and `name`.
        *   Return the `std::vector<ModelEntry>`.
2.  [ ] **`void setSelectedModel(const std::string& model_id);`**
    *   **Purpose**: Saves the user's selected model ID to the database and updates internal state.
    *   **Implementation (`gui_interface.cpp`)**:
        *   Call `db_manager_ref.saveSetting("selected_model_id", model_id);`.
        *   Update `this->current_selected_model_id = model_id;`.
3.  [ ] **`std::string getSelectedModelId() const;`**
    *   **Purpose**: Retrieves the currently selected (active) model ID.
    *   **Implementation (`gui_interface.cpp`)**:
        *   Return `this->current_selected_model_id;`.

### D. Modifications to Existing Methods in `gui_interface.cpp`:
1.  [ ] **Update `GuiInterface::initialize()`**:
    *   **Purpose**: Load the persisted selected model ID on application startup.
    *   **Implementation**:
        *   After `db_manager_ref` is initialized and available.
        *   `std::optional<std::string> loaded_model_id_opt = db_manager_ref.loadSetting("selected_model_id");`
        *   `if (loaded_model_id_opt.has_value()) {`
            *   `// Optional: Validate if loaded_model_id_opt.value() is a known/valid model ID.`
            *   `// For now, assume any non-empty string from DB is potentially valid until models are loaded.`
            *   `this->current_selected_model_id = loaded_model_id_opt.value();`
        *   `} else {`
            *   `this->current_selected_model_id = default_model_id;`
            *   `db_manager_ref.saveSetting("selected_model_id", this->default_model_id); // Persist default if none was set`
        *   `}`

## II. `main_gui.cpp` Modifications

### A. State Variables (e.g., static within `main()` or a suitable scope):
1.  [ ] `static std::vector<GuiInterface::ModelEntry> available_models_list;`
2.  [ ] `static std::string current_gui_selected_model_id;`
3.  [ ] `static int current_gui_selected_model_idx = -1; // Index for ImGui::Combo`
4.  [ ] `static bool models_list_loaded = false;`

### B. Initialization Logic (within `main()`, after `gui_ui` and `client` are initialized):
1.  [ ] **Load initial model list and set selected model for GUI**:
    *   `available_models_list = gui_ui.getAvailableModels();`
    *   `models_list_loaded = true;`
    *   `current_gui_selected_model_id = gui_ui.getSelectedModelId(); // Get from GuiInterface, which loaded from DB`
    *   **Find index**: Iterate `available_models_list` to find the index of `current_gui_selected_model_id`. Set `current_gui_selected_model_idx` accordingly.
        *   If not found, `current_gui_selected_model_idx` could be set to 0 (first model) or -1 (prompt "Select Model"). If set to 0, ensure `current_gui_selected_model_id` is updated to `available_models_list[0].id` and `gui_ui.setSelectedModel()` and `client.setActiveModel()` are called to synchronize. For simplicity, if the DB-loaded ID isn't in the list, defaulting to the first available model and updating state might be a robust approach.

### C. UI Rendering (within the main render loop, likely inside "Settings" or a new section):
1.  [ ] **Add ImGui Combo Box for Model Selection**:
    *   `if (models_list_loaded && !available_models_list.empty()) {`
    *   `    const char* combo_preview_value = (current_gui_selected_model_idx >= 0 && current_gui_selected_model_idx < available_models_list.size()) ? available_models_list[current_gui_selected_model_idx].name.c_str() : "Select a Model";`
    *   `    if (ImGui::BeginCombo("Active Model", combo_preview_value)) {`
    *   `        for (int i = 0; i < available_models_list.size(); ++i) {`
    *   `            const bool is_selected = (current_gui_selected_model_idx == i);`
    *   `            if (ImGui::Selectable(available_models_list[i].name.c_str(), is_selected)) {`
    *   `                current_gui_selected_model_idx = i;`
    *   `                current_gui_selected_model_id = available_models_list[i].id;`
    *   `                gui_ui.setSelectedModel(current_gui_selected_model_id);`
    *   `                client.setActiveModel(current_gui_selected_model_id); // Notify ChatClient`
    *   `            }`
    *   `            if (is_selected) { ImGui::SetItemDefaultFocus(); }`
    *   `        }`
    *   `        ImGui::EndCombo();`
    *   `    }`
    *   `} else if (models_list_loaded && available_models_list.empty()) {`
    *   `    ImGui::Text("No models available.");`
    *   `} else {`
    *   `    ImGui::Text("Loading models...");`
    *   `}`
    *   Consider adding a "Refresh Models" button if dynamic model list updates are needed later.

## III. `ChatClient` Modifications (`chat_client.h`, `chat_client.cpp`)

### A. Member Variables in `chat_client.h`:
1.  [ ] `private: std::string active_model_id;`
2.  [ ] `private: const std::string default_model_id = "DEFAULT_MODEL_ID"; // Match GuiInterface`

### B. New Public Method in `chat_client.h` (implement in `chat_client.cpp`):
1.  [ ] **`void setActiveModel(const std::string& model_id);`**
    *   **Purpose**: Updates the model ID used by `ChatClient`.
    *   **Implementation (`chat_client.cpp`)**:
        *   `this->active_model_id = model_id;`
        *   `// Log or display status: gui_interface.displayStatus("ChatClient active model set to: " + model_id);`
        *   `// Any internal ChatClient logic needed when model changes (e.g., clear context, reload model-specific settings).`

### C. Modifications to Constructor or Initialization in `chat_client.cpp`:
1.  [ ] **In `ChatClient::ChatClient(...)` or an early init method called from `main_gui.cpp`**:
    *   The initial `active_model_id` needs to be set. `main_gui.cpp` will orchestrate this:
        *   After `gui_ui` is initialized (and has loaded `selected_model_id` from DB).
        *   After `client` (ChatClient) is constructed.
        *   `main_gui.cpp` will call: `std::string initial_model_id = gui_ui.getSelectedModelId();`
        *   `client.setActiveModel(initial_model_id);`

## IV. `Database` (`PersistenceManager`) Interactions Summary

*   **`PersistenceManager::getAllModels()` (or equivalent)**:
    *   Called by `GuiInterface::getAvailableModels()`.
    *   Expected to return a collection of model data including at least `id` and `name` for each model.
*   **`PersistenceManager::saveSetting(const std::string& key, const std::string& value)`**:
    *   Called by `GuiInterface::setSelectedModel()` with `key = "selected_model_id"`.
    *   Called by `GuiInterface::initialize()` to save default if no setting exists.
*   **`PersistenceManager::loadSetting(const std::string& key)`**:
    *   Called by `GuiInterface::initialize()` with `key = "selected_model_id"`.

## V. Overall Data Flow

1.  **Application Startup**:
    *   `main_gui.cpp`: Instantiates `PersistenceManager db_manager;`.
    *   `main_gui.cpp`: Instantiates `GuiInterface gui_ui(db_manager);`.
    *   `gui_ui.initialize()`: Loads `"selected_model_id"` from `db_manager` or sets default. `gui_ui.current_selected_model_id` is populated.
    *   `main_gui.cpp`: Instantiates `ChatClient client(gui_ui, db_manager);`.
    *   `main_gui.cpp`: `std::string startup_model_id = gui_ui.getSelectedModelId();`.
    *   `main_gui.cpp`: `client.setActiveModel(startup_model_id);`.
    *   `main_gui.cpp`: `available_models_list = gui_ui.getAvailableModels();`.
    *   `main_gui.cpp`: GUI dropdown is initialized to reflect `startup_model_id`.

2.  **User Selects a New Model in GUI**:
    *   `main_gui.cpp` (ImGui Combo): User interaction updates `current_gui_selected_model_idx` and `current_gui_selected_model_id`.
    *   `main_gui.cpp` calls `gui_ui.setSelectedModel(current_gui_selected_model_id);`.
        *   `gui_ui` saves to DB via `db_manager_ref.saveSetting("selected_model_id", ...)`.
        *   `gui_ui` updates its internal `current_selected_model_id`.
    *   `main_gui.cpp` calls `client.setActiveModel(current_gui_selected_model_id);`.
        *   `client` updates its internal `active_model_id` and performs any necessary actions.