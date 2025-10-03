# LLM-CLI - Command Line Interface for LLM Interactions

A powerful command-line interface for interacting with Large Language Models through OpenRouter, featuring tool support, conversation history, and model management.

## Recent Updates

**Major Code Refactoring (2025-10):** The codebase has undergone a significant refactoring to improve maintainability and code organization. The monolithic `chat_client.cpp` (1333 lines) has been split into focused modules:

- `model_manager` - Model fetching, parsing, and selection
- `api_client` - API communication and retry logic
- `tool_executor` - Tool call execution (standard and fallback)
- `command_handler` - Slash command processing
- `chat_client` - Main orchestration

See [`REFACTORING.md`](REFACTORING.md) for detailed documentation of the new architecture.

## Features

- 🤖 Support for multiple LLM models via OpenRouter
- 🛠️ Built-in tools:
  - Web search (DuckDuckGo)
  - URL content fetching
  - Current date/time
  - Conversation history lookup
  - Web research (multi-step)
  - Deep research (comprehensive investigation)
- 💾 SQLite-based conversation history
- 🔄 Async model loading
- ⌨️ Readline support for better CLI experience
- 📝 Model management via slash commands

## Prerequisites

- C++20 compatible compiler (GCC 10+ or Clang 12+)
- CMake 3.15+
- libcurl
- SQLite3
- libreadline
- OpenRouter API key

## Building

```bash
mkdir build && cd build
cmake .. -DOPENROUTER_API_KEY="your-api-key"
make -j$(nproc)
```

## Installation

```bash
sudo make install
```

Or run the install script:

```bash
./install.sh
```

## Usage

### Basic Usage

```bash
llm-cli
```

### Slash Commands

- `/models` - List all available models
- `/model <model-id>` - Switch to a specific model

### Example Session

```
> /models
Available Models (current: GPT-4):
  [*] GPT-4 (openai/gpt-4) - Context: 8192 tokens
      Claude 3 Opus (anthropic/claude-3-opus) - Context: 200000 tokens
      ...

> /model anthropic/claude-3-opus
Model changed to: Claude 3 Opus

> What's the weather like in San Diego today?
[Searching web for: San Diego weather today]
Based on current search results...
```

## Project Structure

```
llm-cli/
├── chat_client.h/cpp          # Main conversation orchestration
├── model_manager.h/cpp        # Model management
├── api_client.h/cpp           # API communication
├── tool_executor.h/cpp        # Tool execution
├── command_handler.h/cpp      # Command handling
├── database.h/cpp             # Data persistence
├── tools.h/cpp                # Tool definitions
├── tools_impl/                # Tool implementations
│   ├── search_web_tool.cpp
│   ├── visit_url_tool.cpp
│   ├── datetime_tool.cpp
│   ├── read_history_tool.cpp
│   ├── web_research_tool.cpp
│   └── deep_research_tool.cpp
├── cli_interface.h/cpp        # CLI UI implementation
├── main_cli.cpp               # Entry point
└── REFACTORING.md             # Detailed refactoring documentation
```

## Configuration

API keys can be set via:
1. Compile-time: `-DOPENROUTER_API_KEY="key"`
2. Environment variable: `export OPENROUTER_API_KEY="key"`

## Development

### Code Organization

The project follows a modular architecture with clear separation of concerns:

- **Model Management**: `ModelManager` handles all model-related operations
- **API Communication**: `ApiClient` manages all OpenRouter API interactions
- **Tool Execution**: `ToolExecutor` processes both standard and fallback tool calls
- **Command Handling**: `CommandHandler` processes user slash commands
- **Orchestration**: `ChatClient` coordinates the overall conversation flow

See [`REFACTORING.md`](REFACTORING.md) for architectural details.

### Adding New Features

**New Tool:**
1. Create implementation in `tools_impl/`
2. Add tool definition in `tools.cpp`
3. Add execution case in `ToolManager::execute_tool()`

**New Command:**
1. Add handler method in `CommandHandler`
2. Add routing in `CommandHandler::handleCommand()`

**New Model Source:**
1. Extend `ModelManager::fetchModelsFromAPI()`
2. Update parsing logic as needed

## License

[Your License Here]

## Contributing

Contributions are welcome! Please ensure:
- Code follows existing style
- New features include appropriate tests
- Large changes are discussed in issues first
- Commit messages are clear and descriptive

## Acknowledgments

- OpenRouter for LLM API access
- nlohmann/json for JSON parsing
- Google Gumbo for HTML parsing
- SQLite for data persistence