# LLM-CLI: Local LLM Chat Assistant with Web Tools

LLM-CLI is a command-line chat assistant that connects to OpenRouter LLM APIs (e.g., GPT-4) and provides advanced web research tools, including web search, URL visiting, and conversation history access. It is designed for power users who want a fast, extensible, and scriptable AI assistant in the terminal.

## Features
- **Chat with LLMs** (default: GPT-4.1-nano via OpenRouter)
- **Web search** using Brave Search (HTML scraping), DuckDuckGo (HTML scraping fallback), and Brave Search API (final fallback)
- **Visit URLs** and extract readable content
- **Web research**: multi-step research with synthesis
- **Deep research**: break down complex goals into sub-queries
- **Read conversation history** from SQLite database
- **Tool call support**: LLM can call tools via OpenAI function-calling API
- **.env support** for API keys
- **Multi-threaded** for fast web research

## Quick Start

### Prerequisites
- Linux (tested on Ubuntu)
- C++17 compiler
- [CMake](https://cmake.org/) >= 3.15
- [libcurl](https://curl.se/libcurl/), [libsqlite3](https://www.sqlite.org/), [libreadline](https://tiswww.case.edu/php/chet/readline/rltop.html), [libgumbo](https://github.com/google/gumbo-parser)

Install dependencies (Ubuntu):
```sh
sudo apt-get install build-essential cmake libcurl4-openssl-dev libsqlite3-dev libreadline-dev libgumbo-dev
```

### Build
```sh
# Clone repo and enter directory
# git clone ...
cd llm-cli

# Set your API keys in .env (see below) or provide them at compile time
cp .env.example .env  # or create .env manually

# Build (Release by default)
# You can optionally embed API keys at compile time (see CMakeLists.txt)
# Example: ./build.sh -DOPENROUTER_API_KEY=your_key -DBRAVE_SEARCH_API_KEY=your_key
./build.sh
```

### API Key Setup
Create a `.env` file in the project root to provide API keys at runtime (recommended):
```dotenv
OPENROUTER_API_KEY=sk-or-...
BRAVE_SEARCH_API_KEY=bsk-... # Optional: For Brave Search API fallback
```
Alternatively, you can embed keys at compile time using CMake flags (less secure, not recommended for shared environments).

### Run
```sh
./build/llm
```

## Usage
- Type your message and press Enter.
- Use Ctrl+D to exit.
- The assistant will use tools (web search, visit_url, etc.) as needed.

## Tools
- `search_web`: Search the web. Tries Brave Search HTML scraping, then DuckDuckGo HTML scraping, then Brave Search API (if key provided).
- `visit_url`: Fetch and extract main text from a URL.
- `web_research`: Multi-step research and synthesis.
- `deep_research`: Break down complex goals and aggregate research.
- `read_history`: Query conversation history.
- `get_current_datetime`: Get the current date/time.

## Development
- Main code: `main.cpp`, `chat_client.cpp`, `tools.cpp`, `database.cpp`
- Tool logic: `tools.cpp`/`tools.h`
- LLM API logic: `chat_client.cpp`
- Conversation DB: `database.cpp` (SQLite)
- Build system: `CMakeLists.txt`, `build.sh`

### Add a New Tool
1. Define the tool in `ToolManager` (see `tools.cpp`/`tools.h`).
2. Implement its logic in `ToolManager::execute_tool`.
3. Add to `get_tool_definitions()`.
