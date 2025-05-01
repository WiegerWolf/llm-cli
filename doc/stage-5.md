# Refactoring Stage 5: Refactor ChatClient I/O

**Goal:** Replace all direct standard input/output operations (`readline`, `std::cout`, `std::cerr`) within `ChatClient` with calls to the injected `UserInterface` object's methods.

**Prerequisites:** Stage 4 completed. `ChatClient` has a `UserInterface& ui` member and is constructed with it.

**Checklist:**

1.  [X] **Modify `ChatClient::promptUserInput()` (`chat_client.cpp`):**
    *   Remove the entire existing implementation (which should be identical to `CliInterface::promptUserInput`).
    *   Replace it with: `return ui.promptUserInput();`. **DONE**
2.  [X] **Modify `ChatClient::run()` (`chat_client.cpp`):**
    *   Replace `std::cout << "Chatting with " << this->model_name << " - Type your message (Ctrl+D to exit)\n";`
        with `ui.displayOutput("Chatting with " + this->model_name + " - Type your message (Ctrl+D to exit)\n");`. **DONE**
    *   Replace `cout << "\nExiting...\n";` (in `main.cpp`, actually, but conceptually related) - **Action:** Check `main.cpp` and ensure exit message uses `ui.displayOutput` if desired, or leave as `cout`. Let's leave it in `main.cpp` for now. **DONE** (Left in main.cpp)
3.  [X] **Modify `ChatClient::handleApiError()` (`chat_client.cpp`):**
    *   Replace `std::cerr << "API Error Received: " << api_response["error"].dump(2) << std::endl;`
        with `ui.displayError("API Error Received: " + api_response["error"].dump(2));`. **DONE**
    *   Replace `std::cerr << "Error: Invalid API response structure (First Response).\nResponse was: " << api_response.dump() << "\n";`
        with `ui.displayError("Invalid API response structure (First Response). Response was: " + api_response.dump());`. **DONE**
4.  [X] **Modify `ChatClient::executeStandardToolCalls()` (`chat_client.cpp`):**
    *   Replace `std::cerr << "API Error Received (Final Response): " << final_response_json["error"].dump(2) << std::endl;`
        with `ui.displayError("API Error Received (Final Response): " + final_response_json["error"].dump(2));`. **DONE**
    *   Replace `std::cerr << "Error: Failed to get a valid final text response after tool execution and 3 attempts.\n";`
        with `ui.displayError("Failed to get a valid final text response after tool execution and 3 attempts.");`. **DONE**
    *   Replace `std::cout << final_content << "\n\n"; std::cout.flush();`
        with `ui.displayOutput(final_content + "\n\n");`. **DONE**
5.  [X] **Modify `ChatClient::executeFallbackFunctionTags()` (`chat_client.cpp`):**
    *   Replace `std::cout << "[Executing function from content: " << function_name << "]\n"; std::cout.flush();`
        with `ui.displayStatus("[Executing function from content: " + function_name + "]");`. **DONE**
    *   Replace `std::cerr << "API Error Received (Fallback Final Response): " << final_response_json["error"].dump(2) << std::endl;`
        with `ui.displayError("API Error Received (Fallback Final Response): " + final_response_json["error"].dump(2));`. **DONE**
    *   Replace `std::cerr << "Error: Failed to get final response after fallback tool execution.\n";`
        with `ui.displayError("Failed to get final response after fallback tool execution.");`. **DONE**
    *   Replace `std::cout << final_content << "\n\n"; std::cout.flush();`
        with `ui.displayOutput(final_content + "\n\n");`. **DONE**
6.  [X] **Modify `ChatClient::printAndSaveAssistantContent()` (`chat_client.cpp`):**
    *   Replace `std::cout << txt << "\n\n";` with `ui.displayOutput(txt + "\n\n");`. **DONE**
    *   Replace `std::cout << dumped << "\n\n";` with `ui.displayOutput(dumped + "\n\n");`. **DONE**
    *   Remove `std::cout.flush();`. **DONE**
7.  [X] **Modify `ChatClient::processTurn()` (`chat_client.cpp`):**
    *   Replace `std::cerr << "Error: " << e.what() << "\n";`
        with `ui.displayError(e.what());`. **DONE**
8.  [X] **Modify `ChatClient::executeAndPrepareToolResult()` (`chat_client.cpp`):**
    *   Replace `std::cerr << "Tool execution error for '" << function_name << "': " << e.what() << "\n";`
        with `ui.displayError("Tool execution error for '" + function_name + "': " + e.what());`. **DONE**
9.  [X] **Review Includes:** Check `chat_client.cpp` and remove `#include <iostream>` and `#include <readline/readline.h>`, `#include <readline/history.h>`, `#include <cstdlib>` (for `free`) if they are no longer needed. **DONE** (`iostream` removed, `cstdlib` kept for `getenv`)
10. [ ] **Build & Verify:**
    *   Run `./build.sh`.
    *   Ensure compilation succeeds.
    *   Run `./build/llm`. Test basic chat, ensure input is read via `readline` (from `CliInterface`) and output/errors appear correctly formatted by `CliInterface`. Tool status messages will still use `std::cout`/`cerr` directly.
