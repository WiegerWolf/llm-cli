# LLM-CLI & LLM-GUI: Local LLM Chat Assistant with Web Tools

LLM-CLI/GUI is a chat assistant that connects to OpenRouter LLM APIs (e.g., GPT-4) and provides advanced web research tools. It offers both a command-line interface (`llm-cli`) for power users and a graphical interface (`llm-gui`) built with ImGui.

## Features

- **Chat with LLMs** (default: GPT-4.1-nano via OpenRouter)
- **Command-Line Interface (`llm-cli`)**: Fast, scriptable terminal interaction.
- **Graphical User Interface (`llm-gui`)**: User-friendly chat window powered by ImGui.
- **Web search**: Uses Brave Search (HTML scraping), DuckDuckGo (HTML scraping fallback), and Brave Search API (final fallback).
- **Visit URLs**: Extracts readable content from web pages.
- **Web research**: Multi-step research with synthesis.
- **Deep research**: Breaks down complex goals into sub-queries.
- **Read conversation history**: Accesses past interactions from an SQLite database.
- **Tool call support**: LLM can invoke tools via OpenAI function-calling API.
- **.env support**: Manages API keys securely.
- **Multi-threaded**: Enables responsive UI and non-blocking web research.

## Screenshots (LLM-GUI)

The main window displays the conversation history, user input field, and status indicators:

![Screenshot from 2025-05-05 19-26-59](https://github.com/user-attachments/assets/ea840a3a-7dd9-45dc-9cd3-0b55b0804f15)

## Quick Start

### Prerequisites

**Common:**
- C++17 compiler (GCC, Clang, MSVC)
- [CMake](https://cmake.org/) >= 3.15

**For `llm-cli` (Command-Line Interface):**
- [libcurl](https://curl.se/libcurl/) (HTTP requests)
- [libsqlite3](https://www.sqlite.org/) (Conversation history)
- [libreadline](https://tiswww.case.edu/php/chet/readline/rltop.html) (Terminal input editing - Linux/macOS)
- [libgumbo](https://github.com/google/gumbo-parser) (HTML parsing)

**For `llm-gui` (Graphical User Interface):**
- All `llm-cli` dependencies above
- [GLFW](https://www.glfw.org/) (Windowing and Input)
- OpenGL drivers (Graphics Rendering)
- Dear ImGui (for the GUI) is included as a Git submodule (located in `extern/imgui`) and is compiled directly as part of the project.

**Installation Examples:**

*   **Ubuntu/Debian:**
    ```sh
    sudo apt-get update
    sudo apt-get install build-essential cmake libcurl4-openssl-dev libsqlite3-dev libreadline-dev libgumbo-dev libglfw3-dev
    # Ensure OpenGL drivers are installed for your graphics card.
    # Check your distribution's documentation or GPU vendor's website.
    ```
*   **macOS (using Homebrew):**
    ```sh
    brew install cmake curl sqlite readline gumbo-parser glfw
    # OpenGL is typically included with macOS.
    ```
*   **Windows (using vcpkg or similar):**
    ```powershell
    # Example using vcpkg (adjust triplets as needed)
    vcpkg install curl sqlite3 readline gumbo glfw3
    # Ensure you have appropriate graphics drivers installed.
    ```
    *Note: Windows build might require additional configuration in CMakeLists.txt for library paths.*

### Initialize Git Submodules (Required after cloning)

After cloning the repository, you need to initialize and update the Git submodules (which includes Dear ImGui for the graphical interface). Run the following command in the project root directory:

```sh
git submodule update --init --recursive
```

### API Key Setup (Required)

Create a `.env` file in the project root directory by copying the example:
```sh
cp .env.example .env
```
Then, edit `.env` and add your API keys:
```dotenv
OPENROUTER_API_KEY=sk-or-...your-key-here...
BRAVE_SEARCH_API_KEY=bsk-...your-key-here... # Optional: For Brave Search API fallback
```
The application loads these keys at runtime. Alternatively, you can embed keys at compile time using CMake flags (less secure, see `CMakeLists.txt`), but using `.env` is recommended.

### Build & Installation

This project builds two executables: `llm-cli` (command-line) and `llm-gui` (graphical).

**1. Build Only (Local Development):**
Use the `build.sh` script to compile the executables into the `build/` directory without installing them system-wide.
```sh
chmod +x build.sh
./build.sh
# Executables will be in ./build/llm-cli and ./build/llm-gui
```

**2. Build and Install (System-Wide):**
Use the `install.sh` script to compile *and* install both `llm-cli` and `llm-gui` to your system's standard binary location (e.g., `/usr/local/bin`). This requires `sudo` privileges for the installation step.
```sh
chmod +x install.sh
./install.sh
```
This script handles creating the build directory, running CMake, building both targets, and installing them.

### Run

**After using `install.sh`:**
If you installed the applications, they should be available in your system PATH:
```sh
# Run the command-line version
llm-cli

# Run the graphical version
llm-gui
```

**After using `build.sh` (local build):**
If you only built locally, run them directly from the `build` directory:
```sh
# Run the command-line version
./build/llm-cli

# Run the graphical version
./build/llm-gui
```

## Usage

**LLM-CLI:**
- Type your message and press Enter.
- Use Ctrl+D or type `/exit` to quit.
- The assistant will use tools (web search, visit_url, etc.) as needed.

**LLM-GUI:**
- Type your message in the input box at the bottom and press Enter or click "Send".
- Close the window to exit.
- Tool calls and results are displayed within the chat history.

## Tools Available

- `search_web`: Searches the web using multiple strategies (HTML scraping, API).
- `visit_url`: Fetches and extracts the main text content from a URL.
- `web_research`: Performs multi-step web research and synthesizes findings.
- `deep_research`: Breaks down complex research goals into sub-queries and aggregates results.
- `read_history`: Queries the local conversation history database.
- `get_current_datetime`: Retrieves the current date and time.

## Development

- **Core Logic:** `chat_client.cpp` (LLM interaction, tool execution), `database.cpp` (SQLite history)
- **Interfaces:**
    - `cli_interface.cpp`: Handles command-line input/output.
    - `gui_interface.cpp`: Manages the ImGui-based graphical interface.
    - `ui_interface.h`: Abstract base class for UI interactions.
- **Entry Points:** `main_cli.cpp` (for `llm-cli`), `main_gui.cpp` (for `llm-gui`)
- **Tool Implementations:** Located in the `tools_impl/` directory (e.g., `search_web_tool.cpp`).
- **Tool Definitions:** `tools.cpp` / `tools.h` (Tool registration and definition structure).
- **Build System:** `CMakeLists.txt`, `build.sh`, `install.sh`

### Adding a New Tool

1.  **Create Implementation:** Add new `.h` and `.cpp` files in the `tools_impl/` directory for your tool's logic (e.g., `my_new_tool.h`, `my_new_tool.cpp`). Inherit from `Tool` base class if appropriate or implement the necessary functions.
2.  **Define Metadata:** Add the tool's definition (name, description, parameters) to the `get_tool_definitions()` function in `tools.cpp`.
3.  **Register Execution Logic:** Include your new tool's header in `chat_client.cpp` and add a case to handle its execution within the `ChatClient::execute_tool` method, calling your implementation logic from `tools_impl/`.
4.  **Update CMake:** Add your new source files (`tools_impl/my_new_tool.cpp`) to the `add_library(llm_core ...)` section in `CMakeLists.txt`.
5.  Re-run CMake and build.
