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

**Common:**
- C++17 compiler
- [CMake](https://cmake.org/) >= 3.15

**For `llm-cli` (Command-Line Interface):**
- [libcurl](https://curl.se/libcurl/)
- [libsqlite3](https://www.sqlite.org/)
- [libreadline](https://tiswww.case.edu/php/chet/readline/rltop.html)
- [libgumbo](https://github.com/google/gumbo-parser)

**For `llm-gui` (Graphical User Interface):**
- All `llm-cli` dependencies above
- [GLFW](https://www.glfw.org/) (Windowing and Input)
- OpenGL drivers (Graphics Rendering)

**Installation Examples:**

*   **Ubuntu/Debian:**
    ```sh
    sudo apt-get update
    sudo apt-get install build-essential cmake libcurl4-openssl-dev libsqlite3-dev libreadline-dev libgumbo-dev libglfw3-dev
    # OpenGL drivers are typically installed via your graphics card manufacturer's instructions or system driver manager.
    ```
*   **macOS (using Homebrew):**
    ```sh
    brew install cmake curl sqlite readline gumbo-parser glfw
    # OpenGL is typically included with macOS.
    ```

### Build & Installation

This project builds two executables: `llm-cli` (command-line) and `llm-gui` (graphical).

**1. API Key Setup (Required):**
Create a `.env` file in the project root to provide API keys at runtime (recommended):
```dotenv
OPENROUTER_API_KEY=sk-or-...
BRAVE_SEARCH_API_KEY=bsk-... # Optional: For Brave Search API fallback
```
Copy the example file: `cp .env.example .env` and edit it with your keys.
Alternatively, you can embed keys at compile time using CMake flags (less secure, see `CMakeLists.txt`).

**2. Build Only (Local Development):**
If you just want to build the executables in the `build/` directory without installing them system-wide, use the `build.sh` script:
```sh
./build.sh
# Executables will be in ./build/llm-cli and ./build/llm-gui
```

**3. Build and Install (System-Wide):**
To build *and* install both `llm-cli` and `llm-gui` to your system's standard binary location (e.g., `/usr/local/bin`), use the `install.sh` script. This requires `sudo` privileges for the installation step.
```sh
chmod +x install.sh
./install.sh
```
This script handles creating the build directory, running CMake configuration, building both targets, and installing them.

### API Key Setup
Create a `.env` file in the project root to provide API keys at runtime (recommended):
```dotenv
OPENROUTER_API_KEY=sk-or-...
BRAVE_SEARCH_API_KEY=bsk-... # Optional: For Brave Search API fallback
```
Alternatively, you can embed keys at compile time using CMake flags (less secure, not recommended for shared environments).

### Run

**After using `install.sh`:**
If you installed the applications using `./install.sh`, they should be available in your system PATH:
```sh
# Run the command-line version
llm-cli

# Run the graphical version
llm-gui
```

**After using `build.sh` (local build):**
If you only built locally using `./build.sh`, run them from the `build` directory:
```sh
./build/llm-cli
./build/llm-gui
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
