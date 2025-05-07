# Part IV: Message Metadata - Implementation Plan

This document outlines the detailed steps to implement Part IV: Message Metadata, focusing on storing and potentially displaying the model ID associated with each message.

## 1. Database Layer Modifications (`database.h`, `database.cpp`)

### 1.1. Update `Message` Struct (`database.h`)
   - [ ] Add a new member `std::string model_id;` to the `Message` struct.
     ```cpp
     // In database.h
     struct Message {
         std::string role;
         std::string content;
         int id = 0;
         std::string timestamp;
         std::string model_id; // <-- New field
     };
     ```

### 1.2. Modify Database Schema (`database.cpp`)
   - [ ] Add a `model_id TEXT` column to the `messages` table.
   - [ ] Consider if this column should be `NULL` or have a default for older messages or messages not associated with a model (e.g., user messages, tool messages if not tracking model for them). For simplicity, allowing `NULL` is a reasonable start.
     ```sql
     // In PersistenceManager::Impl constructor in database.cpp
     // CREATE TABLE IF NOT EXISTS messages (
     //     id INTEGER PRIMARY KEY AUTOINCREMENT,
     //     timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
     //     role TEXT CHECK(role IN ('system','user','assistant', 'tool')),
     //     content TEXT,
     //     model_id TEXT -- <-- New column
     // );
     ```
   - **Note:** Schema changes might require a migration strategy for existing databases if this were a production system. For this development phase, altering the `CREATE TABLE IF NOT EXISTS` statement is sufficient.

### 1.3. Update `PersistenceManager::Impl::insertMessage` (`database.cpp`)
   - [ ] Modify the SQL query to include `model_id`.
   - [ ] Bind the `model_id` from the `Message` struct to the prepared statement.
     ```cpp
     // In PersistenceManager::Impl::insertMessage in database.cpp
     // const char* sql = "INSERT INTO messages (role, content, model_id) VALUES (?, ?, ?)";
     // ...
     // sqlite3_bind_text(stmt, 3, msg.model_id.c_str(), -1, SQLITE_STATIC);
     ```

### 1.4. Update `PersistenceManager` Public Interface (`database.h`, `database.cpp`)
   - [ ] Modify `saveAssistantMessage` to accept `model_id`.
     ```cpp
     // In database.h
     // void saveAssistantMessage(const std::string& content, const std::string& model_id);
     
     // In database.cpp
     // void PersistenceManager::saveAssistantMessage(const std::string& content, const std::string& model_id) {
     //     impl->insertMessage({"assistant", content, 0, "", model_id}); // Assuming id and timestamp are auto-generated or handled by insertMessage
     // }
     ```
   - [ ] Review `saveUserMessage` and `saveToolMessage`. According to the requirements, only assistant messages explicitly need `model_id`. For user and tool messages, `model_id` can be empty or null in the database.
     - If `saveUserMessage` calls `insertMessage`, ensure the `Message` struct is populated with an empty `model_id`.
     - If `saveToolMessage` calls `insertMessage`, ensure the `Message` struct is populated with an empty `model_id`.
     The current implementation of `insertMessage` takes a `Message` struct, so the change in 1.3 means the `Message` struct passed to it must have the `model_id` field. The public `saveUserMessage` and `saveAssistantMessage` will need to construct the `Message` struct appropriately.

     ```cpp
     // In database.cpp (adjusting for the Message struct update)
     // void PersistenceManager::saveUserMessage(const std::string& content) {
     //     impl->insertMessage({"user", content, 0, "", ""}); // Empty model_id
     // }
     // void PersistenceManager::saveAssistantMessage(const std::string& content, const std::string& model_id) {
     //     impl->insertMessage({"assistant", content, 0, "", model_id});
     // }
     // void PersistenceManager::saveToolMessage(const std::string& content) {
     //     // ... (validation logic)
     //     impl->insertMessage({"tool", content, 0, "", ""}); // Empty model_id
     // }
     ```

### 1.5. Update `getContextHistory` and `getHistoryRange` (`database.cpp`)
    - [ ] Modify these methods to retrieve the `model_id` column from the database.
    - [ ] Populate the `model_id` field in the `Message` structs returned.
      ```cpp
      // Example for getContextHistory in database.cpp
      // SELECT id, role, content, timestamp, model_id FROM messages ...
      // ...
      // const unsigned char* mid = sqlite3_column_text(system_stmt, 4); // New index for model_id
      // system_msg.model_id = mid ? reinterpret_cast<const char*>(mid) : "";
      // ... and similarly for the main message loop
      ```

## 2. ChatClient Logic Modifications (`chat_client.h`, `chat_client.cpp`)

### 2.1. Update `ChatClient` calls to `saveAssistantMessage`
   - [ ] In `ChatClient::executeStandardToolCalls`:
     - [ ] When saving the initial assistant message (tool request):
       ```cpp
       // db.saveAssistantMessage(response_message.dump(), this->model_name);
       ```
     - [ ] When saving the final assistant text response:
       ```cpp
       // db.saveAssistantMessage(final_content, this->model_name);
       ```
   - [ ] In `ChatClient::executeFallbackFunctionTags`:
     - [ ] When saving the original assistant message containing the `<function>` tag:
       ```cpp
       // db.saveAssistantMessage(function_block, this->model_name);
       ```
     - [ ] When saving the final assistant text response after fallback execution:
       ```cpp
       // db.saveAssistantMessage(final_content, this->model_name);
       ```
   - [ ] In `ChatClient::printAndSaveAssistantContent`:
     - [ ] This method is called when there are no tool calls.
       ```cpp
       // void ChatClient::printAndSaveAssistantContent(const nlohmann::json& response_message) {
       // ...
       // db.saveAssistantMessage(txt, this->model_name);
       // ... or ...
       // db.saveAssistantMessage(dumped, this->model_name);
       // }
       ```
   - **Note**: `this->model_name` from `ChatClient` will be used as the `model_id`.

## 3. (Optional) GUI Display Considerations

If implementing the optional display of the model name in the GUI:

### 3.1. Retrieve Model ID in GUI Layer
   - [ ] When fetching chat history for display, ensure the `model_id` is retrieved along with other message details (this is covered by step 1.5).

### 3.2. Fetch Model Name (Assumption: `models` table exists)
   - [ ] The requirements state: "Fetch model name from `models` table." This implies a `models` table exists or needs to be created as part of the larger "Dynamic Model Fetching" feature. This plan assumes such a table (e.g., `models (id TEXT PRIMARY KEY, name TEXT, ...)`).
   - [ ] Create a new method in `PersistenceManager` (e.g., `std::optional<std::string> getModelNameById(const std::string& model_id)`).
     - This method would query the `models` table: `SELECT name FROM models WHERE id = ?`.

### 3.3. Display Logic in GUI
   - [ ] For each message being displayed:
     - [ ] If `message.model_id` is not empty:
       - [ ] Call `db.getModelNameById(message.model_id)`.
       - [ ] If a name is found, display it alongside the message (e.g., "Assistant (Model X): message content").
       - [ ] If no name is found (e.g., `model_id` is stale or `models` table is not yet populated), display the `model_id` itself or a placeholder.
     - [ ] If `message.model_id` is empty (e.g., for user messages), display as usual.

## 4. Testing Considerations
- [ ] Verify that `model_id` is correctly saved for assistant messages in all scenarios (direct response, standard tool use, fallback tool use).
- [ ] Verify that user messages and tool messages do not store a `model_id` (or store an empty/null value).
- [ ] If GUI changes are made, verify that the model name (or ID) is displayed correctly.
- [ ] Test database operations with and without an existing `model_id` column to ensure schema alteration/creation works as expected.