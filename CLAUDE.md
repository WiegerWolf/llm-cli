# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LLM-CLI is a command-line interface for interacting with Large Language Models via OpenRouter. It's built in C++20 and features SQLite-based conversation history, built-in tools (web search, URL fetching, research), and support for multiple models.

## Build & Development Commands

### Building
```bash
# Configure and build (from project root)
mkdir build && cd build
cmake .. -DOPENROUTER_API_KEY="your-api-key"
make -j$(nproc)

# Or use the build script
./build.sh

# Build with optimization
cmake .. -DCMAKE_BUILD_TYPE=Release -DOPENROUTER_API_KEY="your-api-key"
make -j$(nproc)
```

### Installation
```bash
# From build directory
sudo make install

# Or use the install script (builds and installs)
./install.sh
```

### Running
```bash
# Run the built executable
./build/llm-cli

# Or after installation
llm-cli
```

### Configuration
API keys can be set via:
1. CMake build-time: `-DOPENROUTER_API_KEY="key"` or `-DBRAVE_SEARCH_API_KEY="key"`
2. Environment variables: `export OPENROUTER_API_KEY="key"`

The keys are configured through `config.h.in` which generates `build/config.h`.

## Architecture

The codebase follows a modular architecture with clear separation of concerns:

### Core Components

**ChatClient** (`chat_client.h/cpp`)
- Orchestrates the main conversation flow
- Coordinates between ModelManager, ApiClient, ToolExecutor, and CommandHandler
- Manages the conversation loop and message context
- Entry point for the conversation logic

**ModelManager** (`model_manager.h/cpp`)
- Fetches models from OpenRouter API asynchronously on startup
- Parses model responses and caches to database
- Manages active model selection and validation
- Default model is "free" (set in config.h.in)

**ApiClient** (`api_client.h/cpp`)
- Handles all OpenRouter API communication via CURL
- Constructs API requests with context and tool definitions
- Implements retry logic with fallback models
- Returns raw JSON responses

**ToolExecutor** (`tool_executor.h/cpp`)
- Executes standard tool_calls from API responses
- Parses and executes fallback `<function>` tags from model content
- Collects tool results and makes follow-up API calls
- Manages the complete tool execution flow

**CommandHandler** (`command_handler.h/cpp`)
- Processes slash commands (`/models`, `/model <id>`)
- Validates and routes command input

### Database Layer

The database is organized into three layers in the `database/` directory:

**DatabaseCore** (`database/database_core.h/cpp`)
- Foundation layer for SQLite operations
- Connection lifecycle management (RAII)
- Cross-platform database path resolution
- Schema initialization and migrations
- Transaction management and SQL execution utilities

**MessageRepository** (`database/message_repository.h/cpp`)
- All message-related database operations
- Message insertion (user, assistant, tool)
- Context history building for API calls
- Time-range queries and cleanup operations

**ModelRepository** (`database/model_repository.h/cpp`)
- Model metadata storage and retrieval
- CRUD operations for models
- Bulk model replacement (atomic)
- Model name lookup for UI

**Legacy Interface** (`database.h/cpp`)
- `PersistenceManager` provides backward-compatible wrapper
- Delegates to MessageRepository and ModelRepository

### Tools

Tools are defined in `tools.h/cpp` with implementations in `tools_impl/`:

- **search_web_tool.cpp**: DuckDuckGo web search
- **visit_url_tool.cpp**: Fetch and parse URL content (uses Gumbo HTML parser)
- **datetime_tool.cpp**: Current date/time
- **read_history_tool.cpp**: Conversation history lookup
- **web_research_tool.cpp**: Multi-step web research
- **deep_research_tool.cpp**: Comprehensive investigation

**Adding a New Tool:**
1. Create implementation in `tools_impl/your_tool.cpp` and `.h`
2. Add tool definition JSON in `ToolManager` constructor in `tools.cpp`
3. Add execution case in `ToolManager::execute_tool()` in `tools.cpp`
4. Include header in `tools.h`

### UI Layer

**CLIInterface** (`cli_interface.h/cpp`)
- Implements `UserInterface` interface (`ui_interface.h`)
- Handles readline integration for user input
- Displays messages and status updates to the terminal

### Dependencies

- **nlohmann/json**: JSON parsing (fetched via CMake FetchContent)
- **Gumbo**: HTML parsing (Google's parser, fetched via CMake)
- **libcurl**: HTTP requests
- **SQLite3**: Database storage
- **libreadline**: CLI input handling

## Key Patterns

### Message Flow
1. User input → `ChatClient::promptUserInput()`
2. Save to database → `MessageRepository::insertUserMessage()`
3. Build context → `MessageRepository::getContextHistory()`
4. API call → `ApiClient::makeApiCall()`
5. Tool execution (if needed) → `ToolExecutor::executeStandardToolCalls()` or `executeFallbackFunctionTags()`
6. Save response → `MessageRepository::insertAssistantMessage()`

### Database Pattern
All database operations use RAII wrappers (`unique_stmt_ptr`) to ensure proper cleanup of SQLite statements.

### Error Handling
- API errors trigger retry logic with fallback models
- Database errors throw `std::runtime_error`
- Tool execution errors return error JSON to the model

## File Organization

```
llm-cli/
├── chat_client.{h,cpp}         # Main orchestrator
├── model_manager.{h,cpp}       # Model management
├── api_client.{h,cpp}          # API communication
├── tool_executor.{h,cpp}       # Tool execution
├── command_handler.{h,cpp}     # Command processing
├── tools.{h,cpp}               # Tool definitions & dispatcher
├── tools_impl/                 # Tool implementations
│   ├── search_web_tool.{h,cpp}
│   ├── visit_url_tool.{h,cpp}
│   ├── datetime_tool.{h,cpp}
│   ├── read_history_tool.{h,cpp}
│   ├── web_research_tool.{h,cpp}
│   └── deep_research_tool.{h,cpp}
├── database/                   # Database layer (modular)
│   ├── database_core.{h,cpp}
│   ├── message_repository.{h,cpp}
│   └── model_repository.{h,cpp}
├── database.{h,cpp}            # Legacy wrapper interface
├── cli_interface.{h,cpp}       # CLI UI implementation
├── ui_interface.h              # UI interface
├── curl_utils.h                # CURL helper utilities
├── model_types.h               # ModelData struct
├── id_types.h                  # ID type definitions
├── main_cli.cpp                # Entry point
├── config.h.in                 # Config template for API keys
└── CMakeLists.txt              # Build configuration
```

## Streaming Support

The project is currently on the `streaming-support-1` branch. See `openrouter-streaming-doc.txt` for OpenRouter streaming API documentation.
