# Refactoring Stage 6: Refactor ToolManager and Tool Implementations

**Goal:** Pass the `UserInterface` reference down through `ToolManager` to individual tool implementations and replace their direct console output with calls to `ui.displayStatus()` or `ui.displayError()`.

**Prerequisites:** Stage 5 completed. `ChatClient` uses `ui` member for its I/O.

**Checklist:**

1.  [ ] **Modify `tools.h`:**
    *   Add `class UserInterface;` forward declaration near the top.
    *   Update `ToolManager::execute_tool` signature:
        ```cpp
        std::string execute_tool(PersistenceManager& db, class ChatClient& client, UserInterface& ui, const std::string& tool_name, const nlohmann::json& args);
        ```
2.  [ ] **Modify `tools.cpp`:**
    *   Include `ui_interface.h`.
    *   Update the definition of `ToolManager::execute_tool` to match the new signature (add `UserInterface& ui` parameter).
    *   Inside `execute_tool`:
        *   Replace `std::cout << "[Searching web for: " << query << "]\n"; std::cout.flush();` with `ui.displayStatus("[Searching web for: " + query + "]");`.
        *   Replace `std::cout << "[Getting current date and time]\n"; std::cout.flush();` with `ui.displayStatus("[Getting current date and time]");`.
        *   Replace `std::cout << "[Visiting URL: " << url_to_visit << "]\n"; std::cout.flush();` with `ui.displayStatus("[Visiting URL: " + url_to_visit + "]");`.
        *   Replace `std::cout << "[Reading history (" << start_time << " to " << end_time << ", Limit: " << limit << ")]\n"; std::cout.flush();` with `ui.displayStatus("[Reading history (" + start_time + " to " + end_time + ", Limit: " + std::to_string(limit) + ")]");`.
        *   Replace `std::cout << "[Performing web research on: " << topic << "]\n"; std::cout.flush();` with `ui.displayStatus("[Performing web research on: " + topic + "]");`.
        *   Replace `std::cout << "[Performing deep research for: " << goal << "]\n"; std::cout.flush();` with `ui.displayStatus("[Performing deep research for: " + goal + "]");`.
        *   Update the call to `perform_web_research`: `return perform_web_research(db, client, ui, topic);`.
        *   Update the call to `perform_deep_research`: `return perform_deep_research(db, client, ui, goal);`.
    *   Remove `#include <iostream>` if no longer needed.
3.  [ ] **Modify `tools_impl/web_research_tool.h`:**
    *   Add `class UserInterface;` forward declaration.
    *   Update signature: `std::string perform_web_research(PersistenceManager& db, ChatClient& client, UserInterface& ui, const std::string& topic);`.
4.  [ ] **Modify `tools_impl/web_research_tool.cpp`:**
    *   Include `ui_interface.h`.
    *   Update function definition to match the new signature (add `UserInterface& ui` parameter).
    *   Replace all `std::cout << " [Research Step X: ...]\n"; std::cout.flush();` lines with `ui.displayStatus(" [Research Step X: ...]");`.
    *   Replace `std::cout << "[Web research complete for: " << topic << "]\n"; std::cout.flush();` with `ui.displayStatus("[Web research complete for: " + topic + "]");`.
    *   Replace `std::cerr << "Web research failed during execution: " << e.what() << "\n";` with `ui.displayError("Web research failed during execution: " + std::string(e.what()));`.
    *   Remove `#include <iostream>` if no longer needed.
5.  [ ] **Modify `tools_impl/deep_research_tool.h`:**
    *   Add `class UserInterface;` forward declaration.
    *   Update signature: `std::string perform_deep_research(PersistenceManager& db, ChatClient& client, UserInterface& ui, const std::string& goal);`.
6.  [ ] **Modify `tools_impl/deep_research_tool.cpp`:**
    *   Include `ui_interface.h`.
    *   Update function definition to match the new signature (add `UserInterface& ui` parameter).
    *   Replace all `std::cout << " [Deep Research Step X: ...]\n"; std::cout.flush();` lines with `ui.displayStatus(" [Deep Research Step X: ...]");`.
    *   Replace `std::cout << "[Deep research complete for: " << goal << "]\n"; std::cout.flush();` with `ui.displayStatus("[Deep research complete for: " + goal + "]");`.
    *   Replace `std::cerr << "Deep research failed during execution: " << e.what() << "\n";` with `ui.displayError("Deep research failed during execution: " + std::string(e.what()));`.
    *   **Crucially:** Update the lambda function passed to `std::async` for the parallel web research:
        *   Capture `ui` by reference: `[&db, &client, &ui, &sub_query]`
        *   Call `perform_web_research` with `ui`: `std::string result = perform_web_research(db, client, ui, sub_query);`
    *   Remove `#include <iostream>` if no longer needed.
7.  [ ] **Modify `chat_client.cpp`:**
    *   In `ChatClient::executeAndPrepareToolResult`: Update the call to `toolManager.execute_tool` to pass the `ui` member: `tool_result_str = toolManager.execute_tool(db, *this, ui, function_name, function_args);`.
    *   In `ChatClient::executeFallbackFunctionTags`: Update the call to `executeAndPrepareToolResult` (it doesn't call `toolManager.execute_tool` directly, but the previous step ensures the `ui` is passed down when `execute_tool` *is* called). Ensure status messages here use `ui.displayStatus`.
8.  [ ] **Review Includes:** Check `tools.cpp`, `tools_impl/web_research_tool.cpp`, `tools_impl/deep_research_tool.cpp` for unneeded `#include <iostream>`.
9.  [ ] **Build & Verify:**
    *   Run `./build.sh`.
    *   Ensure compilation succeeds.
    *   Run `./build/llm`. Test tools (`search_web`, `visit_url`, `web_research`, `deep_research`). Verify their status messages are now displayed via the `CliInterface` (e.g., `[Status] Searching web...`).
