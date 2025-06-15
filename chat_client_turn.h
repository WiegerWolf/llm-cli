#pragma once

#include <string>
#include <nlohmann/json_fwd.hpp>

// Forward-declare ChatClient to avoid including the full header.
// The functions only use references, which is compatible with a forward declaration.
class ChatClient;

namespace chat {

void promptUserInput(ChatClient& client);
void processTurn(ChatClient& client);
std::string executeAndPrepareToolResult(ChatClient& client,
                                         const nlohmann::json& toolCall);
bool executeStandardToolCalls(ChatClient& client,
                               const nlohmann::json& assistantMsg);
bool executeFallbackFunctionTags(ChatClient& client,
                                  const nlohmann::json& assistantMsg);
void saveUserInput(ChatClient& client,
                    const std::string& userInput);
void printAndSaveAssistantContent(ChatClient& client,
                                   const std::string& content);

} // namespace chat