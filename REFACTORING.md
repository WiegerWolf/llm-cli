# Code Refactoring Documentation

## Overview

This document describes the major refactoring performed on the LLM-CLI project to improve maintainability, readability, and extensibility by splitting large monolithic files into smaller, focused modules.

## Problem Statement

### Before Refactoring

The project had two problematic files:

1. **`chat_client.cpp`** - 1333 lines
   - Mixed multiple responsibilities: model management, API communication, tool execution, command handling, and conversation orchestration
   - Difficult to navigate and understand
   - Hard to test individual components
   - High risk of merge conflicts in collaborative development

2. **`database.cpp`** - 725 lines
   - Combined message operations, model operations, and settings management
   - Could benefit from separation of concerns

## Solution: Modular Architecture

### New File Structure

The refactoring split `chat_client.cpp` into 5 focused modules:

```
Project Root
├── chat_client.h/cpp (~200 lines)        # Main orchestration
├── model_manager.h/cpp (~400 lines)      # Model management
├── api_client.h/cpp (~250 lines)         # API communication
├── tool_executor.h/cpp (~400 lines)      # Tool execution
└── command_handler.h/cpp (~200 lines)    # Command handling
```

## Module Descriptions

### 1. ModelManager (`model_manager.h/cpp`)

**Responsibility:** All model-related operations

**Key Functions:**
- Fetching models from OpenRouter API
- Parsing API responses
- Caching models to database
- Managing active model selection
- Asynchronous model loading on startup

**Public API:**
```cpp
class ModelManager {
public:
    void initialize();
    std::string getActiveModelId() const;
    void setActiveModel(const std::string& model_id);
    bool areModelsLoading() const;
};
```

**Benefits:**
- Isolated model logic from conversation flow
- Easy to test model fetching independently
- Clear separation of initialization concerns

### 2. ApiClient (`api_client.h/cpp`)

**Responsibility:** All API communication with OpenRouter

**Key Functions:**
- Constructing API requests with context and tools
- Managing CURL operations
- Handling API responses and errors
- Implementing retry logic with fallback models
- Building secure conversation history

**Public API:**
```cpp
class ApiClient {
public:
    std::string makeApiCall(const std::vector<Message>& context, 
                           ToolManager& toolManager,
                           bool use_tools = false);
};
```

**Benefits:**
- Encapsulated all network communication
- Retry logic is self-contained
- Easy to mock for testing
- Clear interface for API interactions

### 3. ToolExecutor (`tool_executor.h/cpp`)

**Responsibility:** Tool call execution (both standard and fallback)

**Key Functions:**
- Executing standard `tool_calls` from API responses
- Parsing and executing fallback `<function>` tags
- Collecting tool results
- Making follow-up API calls after tool execution
- Handling complex fallback function tag parsing

**Public API:**
```cpp
class ToolExecutor {
public:
    bool executeStandardToolCalls(const nlohmann::json& response_message,
                                  std::vector<Message>& context);
    
    bool executeFallbackFunctionTags(const std::string& content,
                                     std::vector<Message>& context);
};
```

**Benefits:**
- Isolated complex tool execution logic
- Standard and fallback paths are clearly separated
- Easier to maintain and debug tool-related issues

### 4. CommandHandler (`command_handler.h/cpp`)

**Responsibility:** Processing slash commands

**Key Functions:**
- `/models` - List all available models
- `/model <id>` - Change the active model
- Command parsing and routing
- Error handling for invalid commands

**Public API:**
```cpp
class CommandHandler {
public:
    bool handleCommand(const std::string& input);
};
```

**Benefits:**
- Easy to add new commands
- Clear command implementation structure
- Separated user input handling from core logic

### 5. ChatClient (`chat_client.h/cpp`)

**Responsibility:** Orchestration and conversation flow

**Key Functions:**
- Main conversation loop
- Delegating to specialized modules
- Coordinating the turn processing flow
- Error handling at the top level

**Simplified Structure:**
```cpp
class ChatClient {
private:
    std::unique_ptr<ModelManager> modelManager;
    std::unique_ptr<ApiClient> apiClient;
    std::unique_ptr<ToolExecutor> toolExecutor;
    std::unique_ptr<CommandHandler> commandHandler;
    
public:
    void run();                    // Main loop
    void processTurn(...);         // Orchestrates one turn
};
```

**Benefits:**
- Clear high-level flow
- Focused on coordination, not implementation
- Much easier to understand the overall structure

## Design Principles Applied

### 1. Single Responsibility Principle (SRP)
Each module has one clear purpose:
- ModelManager: Models
- ApiClient: API communication
- ToolExecutor: Tool execution
- CommandHandler: Commands
- ChatClient: Orchestration

### 2. Dependency Injection
Dependencies are passed through constructors:
```cpp
ModelManager(UserInterface& ui, PersistenceManager& db)
ApiClient(UserInterface& ui, std::string& active_model_id)
ToolExecutor(UI& ui, DB& db, ToolManager& tm, ApiClient& api, ChatClient& client, ...)
```

### 3. Clear Interfaces
Each module exposes a minimal, well-defined public API

### 4. Encapsulation
Implementation details are private; only necessary methods are public

### 5. Composition Over Inheritance
ChatClient uses composition to delegate to specialized modules

## Benefits of the Refactoring

### Maintainability
- **Easier to locate code**: Need to modify model loading? Go to ModelManager
- **Smaller files**: Each file is under 400 lines, much easier to navigate
- **Focused changes**: Changes to tool execution don't affect API communication

### Readability
- **Clear structure**: Module names immediately indicate purpose
- **Reduced cognitive load**: Developers only need to understand one module at a time
- **Better organization**: Related code is grouped together

### Testability
- **Unit testing**: Each module can be tested independently
- **Mocking**: Dependencies can be easily mocked for testing
- **Isolated testing**: Test model loading without testing API calls

### Extensibility
- **Easy to add features**: New commands just need updates to CommandHandler
- **Plugin potential**: Tool execution is already separated
- **API changes**: Isolated to ApiClient module

### Collaboration
- **Reduced conflicts**: Different developers can work on different modules
- **Clear ownership**: Each module can have a clear owner/expert
- **Parallel development**: Multiple features can be developed simultaneously

## File Size Comparison

| File | Before | After | Reduction |
|------|--------|-------|-----------|
| chat_client.cpp | 1333 lines | ~200 lines | 85% smaller |
| Total (all modules) | 1333 lines | ~1450 lines | Slightly more (due to headers/structure) |

**Note:** The total is slightly larger due to:
- Necessary header files
- Class declarations
- Better documentation
- Clear separation boundaries

**This is expected and desirable** - the slight increase in total lines is vastly outweighed by the improvements in maintainability and clarity.

## Migration Guide

### For Developers

If you're working with the codebase:

1. **Model operations**: Use `ModelManager` directly
2. **API calls**: Use `ApiClient::makeApiCall()`
3. **Tool execution**: Use `ToolExecutor` methods
4. **Commands**: Add to `CommandHandler`
5. **Main flow**: Modify `ChatClient::processTurn()`

### Example: Adding a New Command

Before (would require editing 1333-line file):
- Find command handling section (~line 1214)
- Add parsing logic
- Add execution logic
- Risk breaking other features

After:
```cpp
// In command_handler.cpp
void CommandHandler::handleMyNewCommand() {
    // Implementation here
}

// In CommandHandler::handleCommand()
else if (command == "/mynew") {
    handleMyNewCommand();
    return true;
}
```

## Compilation

The refactored code compiles successfully with no errors:

```bash
cd build
cmake ..
make -j$(nproc)
```

All new files are properly integrated into CMakeLists.txt.

## Future Improvements

While chat_client.cpp has been successfully refactored, `database.cpp` (725 lines) could benefit from similar treatment:

### Proposed Database Refactoring:
1. **database_core.h/cpp** - Connection management, transactions, schema
2. **message_repository.h/cpp** - All message operations
3. **model_repository.h/cpp** - All model operations
4. **database.h/cpp** - Facade coordinator with settings

This would follow the same principles and provide similar benefits.

## Conclusion

This refactoring significantly improves the codebase without changing functionality:

✅ **Smaller, focused files** (200-400 lines each)  
✅ **Clear separation of concerns**  
✅ **Easier to maintain and extend**  
✅ **Better testability**  
✅ **Improved collaboration potential**  
✅ **Compiles successfully**  
✅ **No functionality changes**  

The investment in refactoring pays immediate dividends in developer productivity and code quality, and will continue to provide value as the project grows.