#pragma once
#include <string>
#include "database.h"
class ChatClient;
class UserInterface; // Forward declaration

std::string perform_deep_research(Database& db, ChatClient& client, UserInterface& ui, const std::string& goal);
