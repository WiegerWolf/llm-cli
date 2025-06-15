# LLM-CLI & LLM-GUI: Local LLM Chat Assistant

LLM-CLI/GUI is a powerful C++ chat assistant that connects to OpenRouter-compatible LLM APIs (e.g., GPT-4, Llama 3) and provides advanced web research tools. It offers both a fast, scriptable command-line interface (`llm-cli`) and a feature-rich graphical interface (`llm-gui`) built with ImGui.

![GUI Screenshot](https://github.com/user-attachments/assets/ea840a3a-7dd9-45dc-9cd3-0b55b0804f15)

## Overview

This project provides a versatile platform for interacting with LLMs locally. It is designed for both casual users and developers, with two distinct entry points:

*   **`llm-gui`**: A graphical interface that offers a user-friendly chat experience, complete with an interactive graph visualization of the conversation history.
*   **`llm-cli`**: A lightweight command-line interface for fast, efficient, and scriptable interactions.

The core of the application is a robust chat client that handles API communication, tool integration, and conversation management, with all history persisted in a local SQLite database.

## Features

*   **Dual Interfaces**: Choose between a full-featured GUI (`llm-gui`) and a fast terminal client (`llm-cli`).
*   **Flexible LLM Integration**: Connects to any OpenRouter-compatible API endpoint.
*   **Interactive Graph View**: The GUI features a unique conversation visualization with a hybrid chronological-force layout algorithm to explore complex discussion threads.
*   **Powerful Tooling**: The LLM can invoke a variety of built-in tools:
    *   **`search_web`**: Searches the web using Brave Search and DuckDuckGo.
    *   **`visit_url`**: Fetches and extracts readable content from web pages.
    *   **`web_research`**: Performs multi-step research and synthesizes findings.
    *   **`deep_research`**: Breaks down complex goals into sub-queries for in-depth analysis.
    *   **`read_history`**: Queries the local conversation history from the SQLite database.
*   **Secure API Key Management**: Uses a `.env` file to securely manage your API keys.
*   **Multi-threaded Architecture**: Ensures a responsive UI by running intensive tasks like web research in the background.
*   **Cross-Platform**: Built with standard C++ and CMake, designed for Linux, macOS, and Windows.

## Quick Start

### Prerequisites

You will need the following dependencies installed on your system:

*   **C++20 Compiler**: GCC, Clang, or MSVC.
*   **CMake** (>= 3.15)
*   **libcurl**: For making HTTP requests.
*   **libsqlite3**: For storing conversation history.
*   **libreadline**: For an improved terminal experience in `llm-cli` (Linux/macOS).
*   **libgumbo**: For HTML parsing.
*   **GLFW**: For windowing and input in `llm-gui`.
*   **OpenGL**: For graphics rendering in `llm-gui`.

**Installation Example (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libcurl4-openssl-dev libsqlite3-dev libreadline-dev libgumbo-dev libglfw3-dev
```

### 1. Clone & Initialize Submodules

First, clone the repository. The project uses a Git submodule for the ImGui library, so you must initialize it.

```bash
git clone <repository-url>
cd llm-cli
git submodule update --init --recursive
```

### 2. Set Up API Keys

Create a `.env` file by copying the example and add your OpenRouter API key.

```bash
cp .env.example .env
```

Edit the `.env` file:
```dotenv
OPENROUTER_API_KEY=sk-or-...your-key-here...
BRAVE_SEARCH_API_KEY=...your-optional-key...
```

### 3. Build & Run

The repository includes simple shell scripts to build and install the application.

**Build for Local Development:**
This will compile the `llm-cli` and `llm-gui` executables into the `build/` directory.
```bash
chmod +x build.sh
./build.sh
```
To run the applications:
```bash
./build/llm-cli
./build/llm-gui
```

**Build and Install System-Wide:**
This will compile and install the executables to a system directory (e.g., `/usr/local/bin`), making them accessible from anywhere.
```bash
chmod +x install.sh
sudo ./install.sh
```
To run the applications:
```bash
llm-cli
llm-gui
```

## Project Structure

The repository is organized as follows:

-   `CMakeLists.txt`: The main CMake build script.
-   `build.sh`, `install.sh`: Convenience scripts for building and installing.
-   `main_cli.cpp`, `main_gui.cpp`: The entry points for the CLI and GUI applications, respectively.
-   `chat_client.h/.cpp`: The core logic for handling chat sessions, API calls, and tool execution.
-   `cli_interface.h/.cpp`: Implementation of the command-line user interface.
-   `gui_interface/`: Source files for the ImGui-based graphical user interface.
-   `graph_manager.h/.cpp`, `graph_layout.h/.cpp`, `graph_renderer.h/.cpp`: Components for the conversation graph visualization in the GUI.
-   `database.h/.cpp`: SQLite database management for conversation history.
-   `tools_impl/`: Implementations for the various tools the LLM can use.
-   `extern/`: Contains external libraries, such as ImGui.
-   `tests/`: Unit and behavior tests for the project.

## Development & Contribution

Contributions are welcome. Here are some guidelines for developers:

### Running Tests

The project uses CTest for testing. To run the test suite, build the project (the `build.sh` script does this) and then run CTest from the build directory.

```bash
cd build/
ctest
```

### Adding a New Tool

1.  **Implement the Logic**: Create a new `.h` and `.cpp` file in the `tools_impl/` directory for your tool's implementation.
2.  **Define the Tool**: Add your tool's definition (name, description, parameters) to `tools.cpp`.
3.  **Register the Tool**: Include your tool's header in `chat_client.cpp` and add the logic to execute it in the `ChatClient::execute_tool` method.
4.  **Update Build System**: Add your new source files to the `add_library(llm_core ...)` section in `CMakeLists.txt`.
5.  **Rebuild**: Run `./build.sh` to compile your changes.

## Additional Resources

*   [https://deepwiki.com/WiegerWolf/llm-cli](https://deepwiki.com/WiegerWolf/llm-cli)

## Git hooks

To enable the pre-commit hook, run the following command:

```
git config core.hooksPath .githooks
```

This hook enforces:
- Code formatting (`tools/check_format.sh`)
- Line of code limit of 500 per file (`scripts/loc_report.py`)
